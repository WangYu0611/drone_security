param([string]$Role,[string]$Name)
$p2Root='C:\Users\wy331\Documents\UE5DroneControl-p2'
$p2Game=Start-Process -FilePath 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe' -ArgumentList "$p2Root\UE5DroneControl.uproject",'/Game/Level/CesiumWorld','-game',"-ClientRole=$Role",'-P1Http=http://127.0.0.1:19080','-P1Ws=ws://127.0.0.1:19081/ws','-unattended','-windowed','-ResX=1920','-ResY=1080','-nosplash','-ExecCmds="P2.QA,t.MaxFPS 20"',"-abslog=$p2Root\Saved\P2QA\$Name.log" -PassThru
$p2Game.Id | Set-Content "$p2Root\Saved\P2QA\$Name.pid"
