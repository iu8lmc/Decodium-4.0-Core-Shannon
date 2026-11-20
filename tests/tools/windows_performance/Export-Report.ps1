# Export only diagnostics; never include app copies or settings backups.
param([string]$ResultDirectory)
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
if(!$ResultDirectory) {
    $latest=Get-ChildItem -LiteralPath (Join-Path $PSScriptRoot 'results') -Directory |
        Where-Object { Test-Path -LiteralPath (Join-Path $_.FullName 'machine.json') } |
        Sort-Object LastWriteTime -Descending | Select-Object -First 1
    if(!$latest) { throw 'Nessun risultato disponibile.' }
    $ResultDirectory=$latest.FullName
}
$source=(Resolve-Path -LiteralPath $ResultDirectory).Path
if(!(Test-Path -LiteralPath (Join-Path $source 'machine.json'))) { throw 'Cartella risultati non valida.' }
$staging=Join-Path ([IO.Path]::GetTempPath()) ('decodium-report-'+[guid]::NewGuid().ToString('N'))
$archive=Join-Path $source 'report.zip'
try {
    New-Item -ItemType Directory -Path $staging | Out-Null
    foreach($name in @('machine.json','comparison.json')) {
        $file=Join-Path $source $name
        if(Test-Path -LiteralPath $file) { Copy-Item -LiteralPath $file -Destination $staging }
    }
    foreach($run in Get-ChildItem -LiteralPath $source -Directory) {
        if($run.Name -notmatch '^[1-4]-(before|after)$') { continue }
        $destination=Join-Path $staging $run.Name
        New-Item -ItemType Directory -Path $destination | Out-Null
        foreach($name in @('result.json','console.log')) {
            $file=Join-Path $run.FullName $name
            if(Test-Path -LiteralPath $file) { Copy-Item -LiteralPath $file -Destination $destination }
        }
    }
    Compress-Archive -Path (Join-Path $staging '*') -DestinationPath $archive -Force
} finally {
    if(Test-Path -LiteralPath $staging) { Remove-Item -LiteralPath $staging -Recurse -Force }
}
Write-Host "Report da inviare: $archive"
