param(
    [string]$JetsonHost = "192.168.30.104",
    [string]$JetsonUser = "",
    [string]$UnrealRoot = "",
    [string]$PythonExe = "",
    [string]$VcpkgToolchain = "",
    [string]$VsDevCmd = "",
    [string]$BackendConfig = "Backend\\config.yaml"
)

$repoRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$envFile = Join-Path $repoRoot ".env.local"

function Get-VsWherePath {
    $programFilesX86 = [Environment]::GetEnvironmentVariable("ProgramFiles(x86)")
    if (-not $programFilesX86) {
        $programFilesX86 = $env:ProgramFiles
    }

    $candidates = @(
        (Join-Path $programFilesX86 "Microsoft Visual Studio\Installer\vswhere.exe"),
        "C:\Program Files\Microsoft Visual Studio\Installer\vswhere.exe"
    )

    $cmd = Get-Command "vswhere.exe" -ErrorAction SilentlyContinue
    if ($cmd) {
        $candidates = @($cmd.Source) + $candidates
    }

    foreach ($candidate in $candidates) {
        if ($candidate -and (Test-Path $candidate)) {
            return $candidate
        }
    }
    return $null
}

function Resolve-CommandExists([string]$name) {
    return $null -ne (Get-Command $name -ErrorAction SilentlyContinue)
}

function Print-Status([string]$title, [bool]$ok, [string]$detail = "") {
    if ($ok) {
        Write-Host "[OK] $title" -ForegroundColor Green
        if ($detail) { Write-Host "    $detail" }
    } else {
        Write-Host "[WARN] $title" -ForegroundColor Yellow
        if ($detail) { Write-Host "    $detail" }
    }
}

Write-Host "=== UE5DroneControl 环境配置写入 ===" -ForegroundColor Cyan

$vswherePath = Get-VsWherePath
Print-Status "Visual Studio 检测" ($null -ne $vswherePath)
if ($vswherePath) {
    $vsInstall = (& $vswherePath -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath).Trim()
} else {
    $vsInstall = ""
}

$cmake = ""
if (Resolve-CommandExists "cmake") {
    $cmake = (Get-Command cmake).Source
} elseif ($vsInstall) {
    $vsCmake = Join-Path $vsInstall "Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
    if (Test-Path $vsCmake) {
        $cmake = $vsCmake
    }
}
Print-Status "CMake" ($cmake -and (Test-Path $cmake)) $cmake

if (-not $VcpkgToolchain) {
    $candidates = @(
        "C:\dev\vcpkg\scripts\buildsystems\vcpkg.cmake",
        "C:\Users\$env:USERNAME\.vcpkg\scripts\buildsystems\vcpkg.cmake",
        "C:\ProgramData\vcpkg\scripts\buildsystems\vcpkg.cmake"
    )

    if ($vsInstall) {
        $candidates = @((Join-Path $vsInstall "VC\vcpkg\scripts\buildsystems\vcpkg.cmake")) + $candidates
    }

    foreach ($candidate in $candidates) {
        if (Test-Path $candidate) {
            $VcpkgToolchain = $candidate
            break
        }
    }
}

if (-not $VsDevCmd) {
    if ($vsInstall -and (Test-Path (Join-Path $vsInstall "Common7\Tools\VsDevCmd.bat"))) {
        $VsDevCmd = Join-Path $vsInstall "Common7\Tools\VsDevCmd.bat"
    }

    if (-not $VsDevCmd) {
        $legacyCandidates = @(
            "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat",
            "C:\Program Files\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat"
        )
        foreach ($path in $legacyCandidates) {
            if (Test-Path $path) {
                $VsDevCmd = $path
                break
            }
        }
    }
}

Print-Status "vcpkg toolchain" ($VcpkgToolchain -and (Test-Path $VcpkgToolchain)) $VcpkgToolchain
Print-Status "VS DevCmd" ($VsDevCmd -and (Test-Path $VsDevCmd)) $VsDevCmd

if (-not $UnrealRoot) {
    $unrealCandidates = @(
        "D:\Software\Epic Games\UE_5.8",
        "D:\Epic Games\UE_5.8",
        "C:\Program Files\Epic Games\UE_5.8"
    )
    foreach ($candidate in $unrealCandidates) {
        if (Test-Path (Join-Path $candidate "Engine\Binaries\Win64\UnrealEditor.exe")) {
            $UnrealRoot = $candidate
            break
        }
    }
}

if (-not $PythonExe) {
    $projectPython = Join-Path $repoRoot ".venv\Scripts\python.exe"
    $pythonCommand = Get-Command python -ErrorAction SilentlyContinue
    if (Test-Path $projectPython) {
        $PythonExe = $projectPython
    } elseif ($pythonCommand) {
        $PythonExe = $pythonCommand.Source
    } elseif ($UnrealRoot) {
        $unrealPython = Join-Path $UnrealRoot "Engine\Binaries\ThirdParty\Python3\Win64\python.exe"
        if (Test-Path $unrealPython) {
            $PythonExe = $unrealPython
        }
    }
}

Print-Status "Unreal Engine 5.8" ($UnrealRoot -and (Test-Path $UnrealRoot)) $UnrealRoot
Print-Status "Python" ($PythonExe -and (Test-Path $PythonExe)) $PythonExe
Print-Status "Jetson Host" $true $JetsonHost

if ($JetsonUser) {
    Write-Host "已设置 Jetson 默认 SSH 用户: $JetsonUser"
}

$envLines = @(
    "UE5DRONE_ROOT=$repoRoot",
    "UE5DRONE_UE_ROOT=$UnrealRoot",
    "UE5DRONE_PYTHON=$PythonExe",
    "UE5DRONE_BACKEND_CONFIG=$BackendConfig",
    "UE5DRONE_VCPKG_TOOLCHAIN=$VcpkgToolchain",
    "UE5DRONE_VS_DEV_CMD=$VsDevCmd",
    "UE5DRONE_JETSON_HOST=$JetsonHost"
)

Set-Content -Path $envFile -Value ($envLines -join "`r`n") -Encoding UTF8

Write-Host ""
Write-Host "已生成: $envFile" -ForegroundColor Green
Write-Host "下一步建议："
Write-Host "1) 检查 .env.local 中 UE 路径和 vcpkg 路径是否真实可用"
Write-Host "2) 如有 SSH 用户/密码，请在运行 Python 桥接前自行设置相关环境变量"
Write-Host "3) 执行 Backend\\build.bat（或 build_simple.bat）编译后端"
if (-not $VcpkgToolchain) {
    Write-Host "4) 先安装 vcpkg 并配置 VCPKG_ROOT，再重新运行脚本补齐 VCPKG_TOOLCHAIN" -ForegroundColor Yellow
}
if (-not $cmake) {
    Write-Host "5) 安装 CMake（https://cmake.org/download）或 winget install Kitware.CMake" -ForegroundColor Yellow
}

