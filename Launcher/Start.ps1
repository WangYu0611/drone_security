param([ValidateSet('Launch','Check','Build')][string]$Mode = 'Launch')
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$env:PYTHONUTF8 = '1'
[Console]::OutputEncoding = New-Object System.Text.UTF8Encoding($false)
$OutputEncoding = [Console]::OutputEncoding
try {
    $python = $null
    $prefix = @()
    foreach ($name in @('py','python')) {
        $candidate = Get-Command $name -ErrorAction SilentlyContinue
        if (-not $candidate) { continue }
        $probe = @()
        if ($name -eq 'py') { $probe = @('-3') }
        try {
            & $candidate.Source @probe -c 'import sys, tkinter; sys.exit(0 if sys.version_info >= (3,10) else 1)' 2>$null
        } catch { continue }
        if ($LASTEXITCODE -eq 0) { $python = $candidate.Source; $prefix = $probe; break }
    }
    if (-not $python) { throw 'Install Python 3.10+ from python.org with Tcl/Tk and the Python launcher (or add Python to PATH).' }
    $arguments = @((Join-Path $PSScriptRoot 'bootstrap.py'))
    if ($Mode -eq 'Check') { $arguments += '--check' }
    if ($Mode -eq 'Build') { $arguments += '--build' }
    & $python @prefix @arguments
    $result = $LASTEXITCODE
    if ($result -ne 0 -or $Mode -ne 'Launch') { Read-Host 'Press Enter to close' | Out-Null }
    exit $result
} catch {
    Write-Host $_ -ForegroundColor Red
    Read-Host 'Press Enter to close' | Out-Null
    exit 1
}
