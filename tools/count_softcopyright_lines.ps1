param(
    [string]$ProjectRoot = "D:\RedAlert\UE5DroneControl"
)

$sourceRoots = @(
    (Join-Path $ProjectRoot "Source\UE5DroneControl"),
    (Join-Path $ProjectRoot "Backend")
)

$includedFiles = foreach ($root in $sourceRoots) {
    Get-ChildItem -LiteralPath $root -File -Recurse |
        Where-Object {
            $_.Extension -in @(".cpp", ".h") -and
            $_.FullName -notmatch "\\build\\" -and
            $_.FullName -notmatch "\\tests?\\" -and
            $_.FullName -notmatch "\\execution_legacy\\"
        }
}

$backendTopLevel = @(
    (Join-Path $ProjectRoot "Backend\main.cpp"),
    (Join-Path $ProjectRoot "Backend\debug_cli.h")
)

foreach ($path in $backendTopLevel) {
    if (Test-Path -LiteralPath $path) {
        $file = Get-Item -LiteralPath $path
        if ($includedFiles.FullName -notcontains $file.FullName) {
            $includedFiles += $file
        }
    }
}

$details = foreach ($file in $includedFiles | Sort-Object FullName -Unique) {
    $lineCount = (Get-Content -LiteralPath $file.FullName).Count
    [pscustomobject]@{
        File = $file.FullName.Substring($ProjectRoot.Length + 1)
        Lines = $lineCount
    }
}

$ueLines = ($details | Where-Object File -like "Source\*").Lines |
    Measure-Object -Sum |
    Select-Object -ExpandProperty Sum
$backendLines = ($details | Where-Object File -like "Backend\*").Lines |
    Measure-Object -Sum |
    Select-Object -ExpandProperty Sum
$totalLines = ($details.Lines | Measure-Object -Sum).Sum

[pscustomobject]@{
    IncludedFiles = $details.Count
    UECppLines = $ueLines
    BackendCppLines = $backendLines
    TotalCppLines = $totalLines
}

Write-Output "=== LARGEST INCLUDED FILES ==="
$details | Sort-Object Lines -Descending | Select-Object -First 20
