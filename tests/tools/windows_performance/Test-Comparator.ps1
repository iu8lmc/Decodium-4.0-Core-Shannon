$ErrorActionPreference='Stop'
. "$PSScriptRoot/Compare-Decodes.ps1"
$fixtures=@(@{id='one'})
function Reports($before,$after) {
    @(@{events=@(@{event='result';caseId='one';rows=@($before|ForEach-Object {@{message=$_}})})},
      @{events=@(@{event='result';caseId='one';rows=@($after|ForEach-Object {@{message=$_}})})},
      @{events=@(@{event='result';caseId='one';rows=@($after|ForEach-Object {@{message=$_}})})},
      @{events=@(@{event='result';caseId='one';rows=@($before|ForEach-Object {@{message=$_}})})})
}
$same=@(Compare-Decodes -Reports (Reports @('CQ K1ABC FN42','CQ W1XYZ FN31') @('CQ W1XYZ FN31','CQ K1ABC FN42')) -Fixtures $fixtures)
if($same.Count -ne 2 -or @($same|Where-Object {!$_.identical}).Count) { throw 'Equal reordered messages failed' }
$changed=@(Compare-Decodes -Reports (Reports @('CQ K1ABC FN42') @('CQ W1XYZ FN31')) -Fixtures $fixtures)
if(@($changed|Where-Object {$_.identical}).Count) { throw 'Same count with different message accepted' }
$missing=@(Compare-Decodes -Reports (Reports @('CQ K1ABC FN42','CQ K1ABC FN42') @('CQ K1ABC FN42')) -Fixtures $fixtures)
if(@($missing|Where-Object {$_.identical}).Count) { throw 'Missing duplicate accepted' }
$empty=@(Compare-Decodes -Reports (Reports @() @()) -Fixtures $fixtures)
if(@($empty|Where-Object {!$_.identical}).Count) { throw 'Equal empty messages failed' }
'Comparator: four positive/negative cases passed in both A/B pairs'

foreach($message in @('K1ABC W9XYZ EN37','K1ABC W9XYZ EN37                         q0','K1ABC W9XYZ EN37 q12')) {
    if(!(Test-ExpectedDecode 'Q65-15A' $message 'K1ABC W9XYZ EN37')) { throw "Valid Q65 message rejected: $message" }
}
foreach($message in @('K1ABC W9XYZ EN38 q0','X K1ABC W9XYZ EN37 q0','K1ABC W9XYZ EN37 q0 EXTRA','K1ABC W9XYZ EN37 q123','k1abc W9XYZ EN37 q0')) {
    if(Test-ExpectedDecode 'Q65-15A' $message 'K1ABC W9XYZ EN37') { throw "Wrong Q65 message accepted: $message" }
}
if(Test-ExpectedDecode 'FT8' 'K1ABC W9XYZ EN37 q0' 'K1ABC W9XYZ EN37') { throw 'Q65 annotation accepted outside Q65' }
$annotationChanged=@(Compare-Decodes -Reports (Reports @('K1ABC W9XYZ EN37 q0') @('K1ABC W9XYZ EN37 q1')) -Fixtures $fixtures)
if(@($annotationChanged|Where-Object {$_.identical}).Count) { throw 'Full-row comparator ignored annotation change' }
'Q65 expected-message positive/negative gates and full-row preservation passed'
