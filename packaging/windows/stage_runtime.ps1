# Prepara la cartella di runtime che packaging/windows/decodium_setup.iss impacchetta
# (BuildDir = build_mingw64). Va eseguito dopo la build di decodium_qml.
#
# Uso:
#   pwsh -File packaging\windows\stage_runtime.ps1 -BuildDir build1007 `
#       -MsysBin C:\msys64\mingw64\bin -QtQmlSrc C:\msys64\mingw64\share\qt6\qml\QtQuick
#
# Copia: decodium.exe, DLL Qt (windeployqt6 senza FFmpeg), le DLL MinGW e delle
# librerie esterne con le loro dipendenze, il sorgente QML di Decodium (decodium,
# panels, dialogs), le cartelle "impl" di Qt Quick (windeployqt le lascia vuote),
# artwork, sounds, dati e traduzioni.

param(
    [Parameter(Mandatory = $true)][string]$BuildDir,
    [string]$MsysBin = 'C:\msys64\mingw64\bin',
    [string]$QtQmlSrc = 'C:\msys64\mingw64\share\qt6\qml\QtQuick',
    [string]$OutDir = 'build_mingw64'
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$exe = Join-Path $BuildDir 'decodium.exe'
$out = Join-Path $root $OutDir
if (-not (Test-Path $exe)) { throw "decodium.exe non trovato in $BuildDir" }

if (Test-Path $out) { Remove-Item -Recurse -Force $out }
New-Item -ItemType Directory -Force -Path $out | Out-Null
Copy-Item $exe $out -Force

# 1) Qt via windeployqt6, senza FFmpeg (installer piu' piccolo, RX/TX funzionano)
Push-Location $root
& (Join-Path $MsysBin 'windeployqt6.exe') --qmldir qml --release --no-translations --no-ffmpeg (Join-Path $out 'decodium.exe')
Pop-Location
Remove-Item (Join-Path $out 'multimedia\ffmpegmediaplugin.dll') -ErrorAction SilentlyContinue

# 2) Cartelle "impl" di Qt Quick vuote dopo windeployqt6
Get-ChildItem $QtQmlSrc -Directory -Recurse -Filter impl | ForEach-Object {
    $rel = $_.FullName.Substring($QtQmlSrc.Length)
    $dst = Join-Path (Join-Path $out 'qml\QtQuick') $rel
    New-Item -ItemType Directory -Force -Path $dst | Out-Null
    Copy-Item (Join-Path $_.FullName '*') $dst -Recurse -Force
}

# 3) QML di Decodium: decodium, e le cartelle fratelli importate con "../../panels"
foreach ($d in @('decodium', 'panels', 'dialogs')) {
    $t = Join-Path $out "qml\$d"
    if (Test-Path $t) { Remove-Item -Recurse -Force $t }
    Copy-Item (Join-Path $root "qml\$d") $t -Recurse -Force
}
Get-ChildItem (Join-Path $out 'qml\decodium') -Recurse -File -Filter *.qmlc | Remove-Item -Force

# 4) Dati, artwork, suoni, traduzioni di Decodium
foreach ($n in @('artwork', 'sounds')) { Copy-Item (Join-Path $BuildDir $n) $out -Recurse -Force }
Copy-Item (Join-Path $BuildDir 'cty.dat'), (Join-Path $BuildDir 'sat.dat') $out -Force
Get-ChildItem $BuildDir -Filter 'decodium_*.qm' | Copy-Item -Destination $out -Force

# 5) DLL di terze parti, con le dipendenze indirette fino a chiusura
$objdump = Join-Path $MsysBin 'objdump.exe'
$queue = [System.Collections.Generic.Queue[string]]::new()
Get-ChildItem $out -Recurse -Include *.exe, *.dll | ForEach-Object { $queue.Enqueue($_.FullName) }
$seen = @{}
while ($queue.Count -gt 0) {
    $file = $queue.Dequeue()
    & $objdump -p $file | Select-String 'DLL Name:' | ForEach-Object {
        $dll = ($_.Line -split '\s+')[-1]
        if ($dll -match '^(api-ms-|ext-ms-)') { return }
        if ($seen.ContainsKey($dll.ToLower())) { return }
        $seen[$dll.ToLower()] = $true
        $local = Join-Path $out $dll
        if (-not (Test-Path $local)) {
            $src = Join-Path $MsysBin $dll
            if (Test-Path $src) { Copy-Item $src $out -Force; $queue.Enqueue($local) }
        }
    }
}

Write-Output "Runtime pronto in ${out}: $((Get-ChildItem $out -Filter *.dll).Count) DLL"
