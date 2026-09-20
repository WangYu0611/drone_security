param(
    [Parameter(Mandatory = $true)]
    [string]$Path
)

$resolvedPath = (Resolve-Path -LiteralPath $Path).Path
$word = $null
$document = $null

try {
    $word = New-Object -ComObject Word.Application
    $word.Visible = $false
    $word.DisplayAlerts = 0
    $document = $word.Documents.Open($resolvedPath, $false, $true)

    Write-Output "DOCUMENT: $resolvedPath"
    Write-Output "TABLES: $($document.Tables.Count)"

    for ($tableIndex = 1; $tableIndex -le $document.Tables.Count; $tableIndex++) {
        $table = $document.Tables.Item($tableIndex)
        Write-Output "=== TABLE $tableIndex ($($table.Rows.Count)x$($table.Columns.Count)) ==="

        for ($rowIndex = 1; $rowIndex -le $table.Rows.Count; $rowIndex++) {
            $cells = @()
            foreach ($cell in $table.Rows.Item($rowIndex).Cells) {
                $text = $cell.Range.Text -replace "[`r`a]", "" -replace "`v", " "
                $cells += $text.Trim()
            }
            Write-Output ("ROW {0}: {1}" -f $rowIndex, ($cells -join " || "))
        }
    }

    Write-Output "=== NONEMPTY PARAGRAPHS ==="
    for ($paragraphIndex = 1; $paragraphIndex -le $document.Paragraphs.Count; $paragraphIndex++) {
        $text = $document.Paragraphs.Item($paragraphIndex).Range.Text -replace "[`r`a]", "" -replace "`v", " "
        $text = $text.Trim()
        if ($text.Length -gt 0) {
            Write-Output ("P{0}: {1}" -f $paragraphIndex, $text)
        }
    }
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
