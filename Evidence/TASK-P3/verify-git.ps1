param([switch]$RequireClean)
$ErrorActionPreference='Stop'
$base='88b95a952348fcd9bcda328cb3415a56523de5c3'
$expected=@{
 'refs/heads/main'='f6ed7f97fabc21c202333528b0cb030dd4b1b2fe'
 'refs/heads/feat/triple-screen-p1-shared-context'='23ef4422a6b48802525bda42ed70a24984be43b9'
 'refs/heads/feat/triple-screen-p2-command-center-v2'=$base
}
$branch=git branch --show-current
if($branch -ne 'feat/triple-screen-p3-security-plan'){throw 'Unexpected branch'}
git merge-base --is-ancestor $base HEAD
if($LASTEXITCODE -ne 0){throw 'P2 base is not an ancestor'}
foreach($ref in $expected.Keys){if((git rev-parse $ref) -ne $expected[$ref]){throw "Protected ref changed: $ref"}}
$merges=@(git rev-list --merges "$base..HEAD")
if($merges.Count){throw 'Merge found in P3 range'}
$dirty=@(git status --porcelain)
if($RequireClean -and $dirty.Count){throw 'Workspace is not clean'}
[pscustomobject]@{branch=$branch;base=$base;head=(git rev-parse HEAD);protected_refs=$expected;merge_count=$merges.Count;clean=($dirty.Count -eq 0);author=(git var GIT_AUTHOR_IDENT)} | ConvertTo-Json -Depth 4
git status
git log --oneline --decorate -15
