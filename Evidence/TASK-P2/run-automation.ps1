param([string]$Role,[string]$Name,[string]$Tests,[switch]$Legacy,[switch]$Live)
$ErrorActionPreference='Stop'
$p2Root='C:\Users\wy331\Documents\UE5DroneControl-p2'
$p2Endpoint=if($Live){19080}else{19089}
$p2WsPort=if($Live){19081}else{19090}
$p2Arguments=@("$p2Root\UE5DroneControl.uproject","-ClientRole=$Role","-P1Http=http://127.0.0.1:$p2Endpoint","-P1Ws=ws://127.0.0.1:$p2WsPort/ws",'-unattended','-nosplash',"-ExecCmds=`"Automation RunTests $Tests`"",'-TestExit="Automation Test Queue Empty"',"-ReportExportPath=$p2Root\Saved\P2QA\$Name","-abslog=$p2Root\Saved\P2QA\$Name.log")
if($Legacy){$p2Arguments+='-P1DisableSync'}
$p2Run=Start-Process -FilePath 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe' -ArgumentList $p2Arguments -WindowStyle Hidden -PassThru
$p2Run.Id | Set-Content "$p2Root\Saved\P2QA\$Name.pid"
if(-not $p2Run.WaitForExit(300000)){Stop-Process -Id $p2Run.Id; throw "$Name timeout"}
Select-String -Path "$p2Root\Saved\P2QA\$Name.log" -Pattern 'Found .*automation tests|Test Completed'
