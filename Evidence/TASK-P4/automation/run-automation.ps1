param([string]$Role='Map',[string]$Tests='DroneOps.P3.CrossClient+DroneOps.P3.Route',[string]$Name='baseline-map',[int]$HttpPort=19289,[int]$WsPort=19290)
$ErrorActionPreference='Stop'
$p4Root='C:\Users\wy331\Documents\UE5DroneControl-p4'
$p4Args=@("$p4Root\UE5DroneControl.uproject","-ClientRole=$Role","-P1Http=http://127.0.0.1:$HttpPort","-P1Ws=ws://127.0.0.1:$WsPort/ws",'-unattended','-nosplash',"-ExecCmds=`"Automation RunTests $Tests`"",'-TestExit="Automation Test Queue Empty"',"-ReportExportPath=$p4Root\Evidence\TASK-P4\automation\$Name","-abslog=$p4Root\Evidence\TASK-P4\automation\$Name.log")
$p4Run=Start-Process -FilePath 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe' -ArgumentList $p4Args -WindowStyle Hidden -PassThru
$p4Run.Id | Set-Content "$p4Root\Saved\P4QA\$Name.pid"
$p4Run.WaitForExit()
Select-String -LiteralPath "$p4Root\Evidence\TASK-P4\automation\$Name.log" -Pattern 'Found .*automation tests|Test Started|Test Completed'
exit $p4Run.ExitCode
