param([string]$Role,[string]$Tests,[string]$Name)
$ErrorActionPreference='Stop'
$root=(Resolve-Path "$PSScriptRoot/../../..").Path
$argsList=@("$root\UE5DroneControl.uproject","-ClientRole=$Role",'-P1Http=http://127.0.0.1:19380','-P1Ws=ws://127.0.0.1:19381/ws','-unattended','-nosplash',"-ExecCmds=`"Automation RunTests $Tests`"",'-TestExit="Automation Test Queue Empty"',"-ReportExportPath=$root\Evidence\TASK-P5\baseline\$Name","-abslog=$root\Saved\P5Baseline\$Name.log")
$p=Start-Process -FilePath 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe' -ArgumentList $argsList -WindowStyle Hidden -PassThru
@{pid=$p.Id;start_time=$p.StartTime.ToString('o');exe=$p.Path;role=$Role} | ConvertTo-Json | Set-Content "$root\Evidence\TASK-P5\baseline\$Name-ownership.json"
$p.WaitForExit()
Select-String -LiteralPath "$root\Saved\P5Baseline\$Name.log" -Pattern 'Found .*automation tests|Test Started|Test Completed'
exit $p.ExitCode
