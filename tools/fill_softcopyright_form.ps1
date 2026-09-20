param(
    [string]$InputPath = "D:\RedAlert\软著申请教程+模版\软著申请教程+模版\软著申请表.doc",
    [string]$OutputDirectory = "D:\RedAlert\UE5DroneControl\.codex-artifacts"
)

$values = [ordered]@{
    '19' = '45752行'
    '20' = '面向无人机集群任务，构建虚实同步的三维指挥、预演与实机控制平台，降低多机协同操控门槛。'
    '21' = '面向低空经济、无人机集群指挥控制、数字孪生仿真与应急任务规划领域。'
    '22' = '软件基于UE5与Cesium构建高保真三维数字孪生场景，支持无人机注册、状态监视、实时遥测可视化及多机选择；可在地图中编辑航点与阵列，进行影子机预演、路径冲突检测和槽位匹配，并将任务批量下发实机；系统通过HTTP、WebSocket和UDP连接后端、Jetson与PX4，完成UE、WGS84、NED坐标转换、集结调度、指令确认、断线重连、低电量与失联告警，形成“规划—预演—执行—反馈”的闭环控制。'
    '23' = '采用影子机预演与镜像机遥测双模型架构，实现虚实状态解耦；支持多机并发、坐标双向转换、指令重复确认及断线重连，兼具三维直观交互、低延迟通信和模块化扩展能力。'
    '24' = '√物联网软件'
}

$limits = @{
    '20' = 50
    '21' = 50
    '22' = 200
    '23' = 100
}

foreach ($row in $limits.Keys) {
    if ($values[$row].Length -gt $limits[$row]) {
        throw "第 $row 行内容为 $($values[$row].Length) 字，超过 $($limits[$row]) 字限制。"
    }
}

$resolvedInput = (Resolve-Path -LiteralPath $InputPath).Path
New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null

$outputDoc = Join-Path $OutputDirectory "软著申请表-数字孪生无人机集群智能控制系统-V1.0.doc"
$outputDocx = Join-Path $OutputDirectory "软著申请表-数字孪生无人机集群智能控制系统-V1.0.docx"

Copy-Item -LiteralPath $resolvedInput -Destination $outputDoc -Force

$word = $null
$document = $null
try {
    $word = New-Object -ComObject Word.Application
    $word.Visible = $false
    $word.DisplayAlerts = 0
    $document = $word.Documents.Open($outputDoc)

    $table = $document.Tables.Item(1)
    foreach ($row in $values.Keys) {
        $range = $table.Cell([int]$row, 2).Range.Duplicate
        $range.End = $range.End - 1
        $range.Text = $values[$row]
        $range.Font.Color = 16711680
    }

    $document.Save()
    $document.SaveAs2($outputDocx, 16)
}
finally {
    if ($document -ne $null) {
        $document.Close($false)
        [void][Runtime.InteropServices.Marshal]::ReleaseComObject($document)
    }
    if ($word -ne $null) {
        $word.Quit()
        [void][Runtime.InteropServices.Marshal]::ReleaseComObject($word)
    }
    [GC]::Collect()
    [GC]::WaitForPendingFinalizers()
}

Write-Output "DOC: $outputDoc"
Write-Output "DOCX: $outputDocx"
foreach ($row in $values.Keys) {
    Write-Output ("ROW {0} LENGTH {1}: {2}" -f $row, $values[$row].Length, $values[$row])
}
