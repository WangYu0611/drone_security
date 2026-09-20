param([string[]]$Roles=@('Command','Map','Video'))
$ErrorActionPreference='Stop'
$root=(Resolve-Path "$PSScriptRoot/..").Path
$results=@()
$cases=@(
    @{role='Command';tests='DroneOps.P4.Workflow.CommandLiveCRUD+DroneOps.P4.Workflow.CommandReviewReadonlyCopyAndCombo';name='ue-command';expected=2},
    @{role='Map';tests='DroneOps.P4.Workflow.MapLiveDraftGuard+DroneOps.P4.Localization.FTextAndDraftIsolation';name='ue-map';expected=2},
    @{role='Video';tests='DroneOps.P4.VideoTarget.ExplicitSelectionAndBrowserIsolation';name='ue-video';expected=1}
)
foreach($case in $cases){
    if($case.role -notin $Roles){continue}
    $name=$case.name
    $role=$case.role
    $tests=$case.tests
    $log="$root\Saved\P5Baseline\final-$name.log"
    $report="$root\Evidence\TASK-P5\automation\$name"
    $argsList=@("$root\UE5DroneControl.uproject","-ClientRole=$role",'-P1Http=http://127.0.0.1:19380','-P1Ws=ws://127.0.0.1:19381/ws','-unattended','-nosplash',"-ExecCmds=`"Automation RunTests $tests`"",'-TestExit="Automation Test Queue Empty"',"-ReportExportPath=$report","-abslog=$log")
    $process=Start-Process -FilePath 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe' -ArgumentList $argsList -WindowStyle Hidden -PassThru
    $process.WaitForExit()
    $summary=Get-Content "$report/index.json" -Raw | ConvertFrom-Json
    $completed=$summary.succeeded+$summary.succeededWithWarnings
    $results+=@{role=$role;pid=$process.Id;exit_code=$process.ExitCode;succeeded=$completed;succeeded_with_warnings=$summary.succeededWithWarnings;failed=$summary.failed;expected=$case.expected}
    $results | ConvertTo-Json -Depth 8 | Set-Content "$root/Evidence/TASK-P5/automation/final-summary.json"
    Select-String -LiteralPath $log -Pattern 'Found .*automation tests|Test Started|Test Completed'
    if($completed -ne $case.expected -or $summary.failed -ne 0){throw "$role regression failed"}
}
