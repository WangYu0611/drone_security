# Read-only, five 2-second process samples. GPU values are device-wide (WDDM).
$taskRoot = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$owners = Get-Content "$taskRoot/Saved/Stage1/ownership.json" | ConvertFrom-Json
$samples = @()
foreach ($i in 1..5) {
    $start = @{}
    foreach ($owner in $owners) { $p = Get-Process -Id $owner.pid -ErrorAction Stop; $start[$owner.role] = $p.CPU }
    $at = Get-Date
    Start-Sleep -Seconds 2
    $seconds = ((Get-Date) - $at).TotalSeconds
    foreach ($owner in $owners) {
        $p = Get-Process -Id $owner.pid -ErrorAction Stop
        $samples += [pscustomobject]@{timestamp=(Get-Date).ToUniversalTime().ToString('o');role=$owner.role;pid=$p.Id;cpu_one_core_percent=100*($p.CPU-$start[$owner.role])/$seconds;working_set_mib=$p.WorkingSet64/1MB;private_mib=$p.PrivateMemorySize64/1MB}
    }
}
$samples | ConvertTo-Json | Set-Content "$taskRoot/Evidence/TASK-P5.2/native/performance.json"
nvidia-smi --query-gpu=name,utilization.gpu,memory.used,memory.total --format=csv | Set-Content "$taskRoot/Evidence/TASK-P5.2/native/gpu.txt"
