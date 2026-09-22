$ErrorActionPreference = "Stop"

$mediaRoot = $PSScriptRoot
$projectRoot = (Resolve-Path (Join-Path $mediaRoot "..\..")).Path
$backend = Join-Path $projectRoot "Backend\build\Release\DroneBackend.exe"
$backendConfig = Join-Path $projectRoot "Backend\config.yaml"
$mediaMtx = Join-Path $mediaRoot "mediamtx.exe"
$mediaMtxConfig = Join-Path $mediaRoot "mediamtx.yml"
if (-not (Test-Path -LiteralPath $mediaMtx -PathType Leaf)) {
    throw 'MediaMTX is an optional local dependency. See tools/MediaMTX/INSTALL.md; Stage 1 Mock does not require it.'
}
$ffmpeg = "C:\ffmpeg-8.0.1-essentials_build\ffmpeg-8.0.1-essentials_build\bin\ffmpeg.exe"
$ffprobe = "C:\ffmpeg-8.0.1-essentials_build\ffmpeg-8.0.1-essentials_build\bin\ffprobe.exe"
$recordings = "D:\DroneData\recordings\drone-4"

$requiredFiles = @($backend, $backendConfig, $mediaMtx, $mediaMtxConfig, $ffmpeg, $ffprobe)
foreach ($file in $requiredFiles) {
    if (-not (Test-Path -LiteralPath $file -PathType Leaf)) {
        throw "Required file not found: $file"
    }
}

function Test-TcpPort {
    param([int]$Port)

    $pattern = "^\s*TCP\s+\S+:$Port\s+\S+\s+LISTENING"
    return $null -ne (netstat -ano -p tcp | Select-String -Pattern $pattern | Select-Object -First 1)
}

function Wait-TcpPort {
    param([int]$Port, [int]$Seconds = 8)

    for ($attempt = 0; $attempt -lt ($Seconds * 4); $attempt++) {
        if (Test-TcpPort -Port $Port) {
            return $true
        }
        Start-Sleep -Milliseconds 250
    }
    return $false
}

function Test-RtspStream {
    param([string]$Path)

    $url = "rtsp://127.0.0.1:8554/$Path"
    & $ffprobe -v error -rtsp_transport tcp -timeout 1500000 -select_streams v:0 `
        -show_entries stream=codec_name -of "csv=p=0" $url 2>$null | Out-Null
    return $LASTEXITCODE -eq 0
}

if (Test-TcpPort -Port 8080) {
    Write-Host "[OK] DroneBackend is already listening on 127.0.0.1:8080"
}
else {
    Write-Host "[START] DroneBackend"
    Start-Process -FilePath $backend -ArgumentList @($backendConfig) `
        -WorkingDirectory $projectRoot -WindowStyle Hidden `
        -RedirectStandardOutput (Join-Path $projectRoot "Backend\build\DroneBackend.out.log") `
        -RedirectStandardError (Join-Path $projectRoot "Backend\build\DroneBackend.err.log")
    if (-not (Wait-TcpPort -Port 8080)) {
        throw "DroneBackend did not open port 8080."
    }
}

if (Test-TcpPort -Port 8554) {
    Write-Host "[OK] MediaMTX is already listening on 127.0.0.1:8554"
}
else {
    Write-Host "[START] MediaMTX"
    Start-Process -FilePath $mediaMtx -ArgumentList @($mediaMtxConfig) `
        -WorkingDirectory $mediaRoot -WindowStyle Hidden `
        -RedirectStandardOutput (Join-Path $mediaRoot "mediamtx.out.log") `
        -RedirectStandardError (Join-Path $mediaRoot "mediamtx.err.log")
    if (-not (Wait-TcpPort -Port 8554)) {
        throw "MediaMTX did not open port 8554."
    }
}

$streams = @(
    @{ Path = "drone-1"; File = "2026-07-24_18-40-27-124003.mp4" },
    @{ Path = "drone-2"; File = "2026-07-24_18-39-05-065661.mp4" },
    @{ Path = "drone-3"; File = "2026-07-24_17-46-34-371191.mp4" }
)

foreach ($stream in $streams) {
    $inputFile = Join-Path $recordings $stream.File
    if (-not (Test-Path -LiteralPath $inputFile -PathType Leaf)) {
        throw "Recording not found: $inputFile"
    }

    if (Test-RtspStream -Path $stream.Path) {
        Write-Host "[OK] $($stream.Path) is already publishing"
        continue
    }

    Write-Host "[START] $($stream.Path) <- $($stream.File)"
    $publishUrl = "rtsp://127.0.0.1:8554/$($stream.Path)"
    $arguments = @(
        "-hide_banner", "-loglevel", "warning", "-stream_loop", "-1", "-re",
        "-i", $inputFile, "-map", "0:v:0", "-c:v", "copy", "-an",
        "-f", "rtsp", "-rtsp_transport", "tcp", $publishUrl
    )
    Start-Process -FilePath $ffmpeg -ArgumentList $arguments `
        -WorkingDirectory $mediaRoot -WindowStyle Hidden `
        -RedirectStandardOutput (Join-Path $mediaRoot "$($stream.Path).ffmpeg.out.log") `
        -RedirectStandardError (Join-Path $mediaRoot "$($stream.Path).ffmpeg.err.log")
}

Start-Sleep -Seconds 2
$failed = $false
foreach ($stream in $streams) {
    if (Test-RtspStream -Path $stream.Path) {
        Write-Host "[READY] http://127.0.0.1:8889/$($stream.Path)"
    }
    else {
        Write-Host "[FAILED] $($stream.Path) is not readable" -ForegroundColor Red
        $failed = $true
    }
}

if ($failed) {
    exit 1
}

Write-Host "All demo streams are ready. Open the video controls inside UE."
