function Compare-Decodes {
    param([object[]]$Reports, [object[]]$Fixtures)
    $comparisons=New-Object System.Collections.Generic.List[object]
    foreach($pair in @(@(0,1),@(3,2))) {
        foreach($fixture in $Fixtures) {
            $a=@($Reports[$pair[0]].events|Where-Object {$_.event -eq 'result' -and $_.caseId -eq $fixture.id})[0]
            $b=@($Reports[$pair[1]].events|Where-Object {$_.event -eq 'result' -and $_.caseId -eq $fixture.id})[0]
            $left=@($a.rows|ForEach-Object {$_.message}|Sort-Object)
            $right=@($b.rows|ForEach-Object {$_.message}|Sort-Object)
            $same=($left|ConvertTo-Json -Compress) -ceq ($right|ConvertTo-Json -Compress)
            $comparisons.Add(@{caseId=$fixture.id;beforeRun=$pair[0]+1;afterRun=$pair[1]+1;identical=$same;beforeMessages=$left;afterMessages=$right})
        }
    }
    return $comparisons.ToArray()
}

# Expected-message smoke gate only. Full A/B rows above remain unmodified.
function Test-ExpectedDecode {
    param([string]$Mode, [string]$Message, [string]$Expected)
    if($Mode -cmatch '^Q65(?:-\d+[A-E])?$') {
        $normal=($Message -replace '\s+',' ').Trim()
        $wanted=($Expected -replace '\s+',' ').Trim()
        return $normal -cmatch ('^'+[regex]::Escape($wanted)+'(?: q\d{1,2})?$')
    }
    return $Message -eq $Expected
}
