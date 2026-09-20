param([string]$Role,[string]$Name)
$p4Root='C:\Users\wy331\Documents\UE5DroneControl-p4'
$p4Args=@("$p4Root\UE5DroneControl.uproject",'/Game/Level/CesiumWorld','-game',"-ClientRole=$Role",'-P1Http=http://127.0.0.1:19282','-P1Ws=ws://127.0.0.1:19283/ws','-unattended','-windowed','-WinX=0','-WinY=30','-ResX=1920','-ResY=1080','-nosplash','-ExecCmds="P2.QA,t.MaxFPS 20"',"-abslog=$p4Root\Evidence\TASK-P4\native\$Name.log")
$p4Game=Start-Process -FilePath 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe' -ArgumentList $p4Args -WindowStyle Normal -PassThru
$p4Game.Id | Set-Content "$p4Root\Saved\P4QA\native\$Name.pid"

