# Fixed Windows location-service bridge. No network/IP geolocation, settings
# changes, or permission prompts. The caller receives one JSON object only.
param(
    [ValidateRange(1, 600)][int]$TimeoutSeconds = 15,
    [long]$DeadlineUnixMilliseconds = 0
)

$ErrorActionPreference = 'Stop'
$clock = [Diagnostics.Stopwatch]::StartNew()
$budgetMilliseconds = [double]$TimeoutSeconds * 1000.0
if ($DeadlineUnixMilliseconds -gt 0) {
    # The parent deadline includes PowerShell startup, before this script runs.
    $parentRemaining = $DeadlineUnixMilliseconds - [DateTimeOffset]::UtcNow.ToUnixTimeMilliseconds()
    $budgetMilliseconds = [Math]::Max(0.0, [Math]::Min($budgetMilliseconds, $parentRemaining))
}
$watcher = $null
$result = @{ ok = $false; error = 'unavailable' }
try {
    try {
        Add-Type -AssemblyName System.Device
    }
    catch {
        $result = @{ ok = $false; error = 'unsupported' }
        throw
    }
    $watcher = [System.Device.Location.GeoCoordinateWatcher]::new(
        [System.Device.Location.GeoPositionAccuracy]::Default)
    # true suppresses the location permission dialog. Denied/disabled remains so.
    $remaining = $budgetMilliseconds - $clock.Elapsed.TotalMilliseconds
    if ($remaining -gt 0) {
        $null = $watcher.TryStart($true, [TimeSpan]::FromMilliseconds($remaining))
    }
    $result = @{ ok = $false; error = 'timeout' }
    # TryStart can acknowledge startup while the first Position is still Unknown.
    # Wait within the same deadline, without retrying/renewing the timeout budget.
    while ($true) {
        if ($watcher.Permission -eq [System.Device.Location.GeoPositionPermission]::Denied) {
            $result = @{ ok = $false; error = 'denied' }
            break
        }
        if ($watcher.Status -eq [System.Device.Location.GeoPositionStatus]::Disabled) {
            $result = @{ ok = $false; error = 'disabled' }
            break
        }
        $remaining = $budgetMilliseconds - $clock.Elapsed.TotalMilliseconds
        if ($remaining -le 0) { break }
        $position = $watcher.Position
        $coordinate = $position.Location
        if ($coordinate.IsUnknown) {
            Start-Sleep -Milliseconds ([int][Math]::Min(100.0, [Math]::Max(1.0, [Math]::Floor($remaining))))
            continue
        }
        if ([double]::IsNaN($coordinate.Latitude) -or [double]::IsInfinity($coordinate.Latitude) -or
                [double]::IsNaN($coordinate.Longitude) -or [double]::IsInfinity($coordinate.Longitude) -or
                [double]::IsNaN($coordinate.HorizontalAccuracy) -or [double]::IsInfinity($coordinate.HorizontalAccuracy)) {
            $result = @{ ok = $false; error = 'invalid_position' }
        }
        else {
            $altitude = $null
            if (-not [double]::IsNaN($coordinate.Altitude)) {
                if ([double]::IsInfinity($coordinate.Altitude)) {
                    throw 'Invalid system altitude'
                }
                $altitude = $coordinate.Altitude
            }
            $result = @{
                ok = $true
                longitude = $coordinate.Longitude
                latitude = $coordinate.Latitude
                altitude = $altitude
                # Windows Native Location GetAltitude uses a reference ellipsoid.
                altitude_reference = 'ellipsoid'
                accuracy_m = $coordinate.HorizontalAccuracy
                timestamp = $position.Timestamp.ToUnixTimeMilliseconds() / 1000.0
            }
        }
        break
    }
}
catch {
    # Never forward raw exception/diagnostic text or location values to stderr.
    if ($result.error -ne 'unsupported') {
        $result = @{ ok = $false; error = 'unavailable' }
    }
}
finally {
    if ($null -ne $watcher) {
        try { $watcher.Stop() } catch { }
        try { $watcher.Dispose() } catch { }
    }
}
[Console]::Out.WriteLine((ConvertTo-Json -InputObject $result -Compress))
