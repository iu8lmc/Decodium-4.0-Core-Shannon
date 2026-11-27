# Windows PowerShell 5.1; no Python, radio, virtual audio or administrator needed.
param([ValidateRange(1,8)][int]$Threads=2, [switch]$LowCpu,
      [ValidateSet('d3d11','software')][string]$GraphicsBackend='d3d11')
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
$root=$PSScriptRoot
. "$root\Compare-Decodes.ps1"
if($env:OS -ne 'Windows_NT') { throw 'Questo runner richiede Windows.' }
$utf8=New-Object System.Text.UTF8Encoding($false)
function Write-Json($value,$path) { [IO.File]::WriteAllText($path,($value|ConvertTo-Json -Depth 30),$utf8) }
function Hash($path) { (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant() }
if (Get-Process -Name 'decodium*' -ErrorAction SilentlyContinue) { throw 'Chiudi tutte le istanze Decodium prima del test.' }
$manifest=Get-Content -LiteralPath "$root\manifest.json" -Raw|ConvertFrom-Json
$appTimeoutSeconds=[Math]::Max(450,30+70*$manifest.fixtures.Count)
$appTimeoutMs=1000*$appTimeoutSeconds
if((Hash (Join-Path $root 'candidate.patch')) -ne $manifest.candidate_patch_sha256) { throw 'Patch candidata alterata' }
foreach($version in @('before','after')) {
    $build=$manifest.builds.$version
    $exe=Join-Path $root "versions\$version\decodium.exe"
    if (!$build.sha256 -or !(Test-Path -LiteralPath $exe)) {
        throw "Pacchetto incompleto: manca la build Windows verificata '$version'. Non usare una versione casuale."
    }
    foreach($file in $build.files.PSObject.Properties) {
        if ((Hash (Join-Path "$root\versions\$version" $file.Name)) -ne $file.Value) { throw "File build alterato: $version/$($file.Name)" }
    }
    if ((Hash $exe) -ne $build.sha256) { throw "Hash eseguibile errato: $version" }
}
foreach($file in $manifest.fixtures) {
    if ((Hash (Join-Path $root $file.file)) -ne $file.sha256) { throw "Campione audio alterato: $($file.file)" }
}
if ($manifest.builds.before.sha256 -eq $manifest.builds.after.sha256) { throw 'Le due build sono identiche.' }
$drive=Get-PSDrive -Name ([IO.Path]::GetPathRoot($root).TrimEnd('\').TrimEnd(':'))
if($drive.Free -lt 3GB) { throw 'Servono almeno 3 GB liberi sul disco del test.' }
$stamp=Get-Date -Format 'yyyyMMdd-HHmmss'
$output=Join-Path $root ("results\"+$stamp+'-'+[guid]::NewGuid().ToString('N').Substring(0,8))
New-Item -ItemType Directory -Path $output -Force|Out-Null
$machine=@{cpu=@(Get-CimInstance Win32_Processor|Select-Object Name,NumberOfCores,NumberOfLogicalProcessors);
    os=Get-CimInstance Win32_OperatingSystem|Select-Object Caption,Version,BuildNumber,TotalVisibleMemorySize,FreePhysicalMemory;
    threads=$Threads;lowCpu=[bool]$LowCpu;graphicsBackend=$GraphicsBackend;directGraphicsWorker=$true;manifest=$manifest;scope='File decode A/B, no RF or live-audio proof'}
Write-Json $machine "$output\machine.json"
# Qt 6 Windows ConfigLocation appends qttest and application name.
# MultiSettings uses applicationName=decodium4 and appends decodium4.ini.
$testConfig=Join-Path ([Environment]::GetFolderPath('LocalApplicationData')) 'qttest\decodium4'
$legacyFiles=@((Join-Path $testConfig 'decodium4.ini'),(Join-Path $testConfig 'ft2.ini'))
$backups=@{}
New-Item -ItemType Directory -Path "$output\restore-private" -Force|Out-Null
foreach($file in $legacyFiles) {
    $bytes=$null
    if(Test-Path -LiteralPath $file) { $bytes=[IO.File]::ReadAllBytes($file);[IO.File]::WriteAllBytes((Join-Path "$output\restore-private" ([IO.Path]::GetFileName($file))),$bytes) }
    $backups[$file]=@{existed=(Test-Path -LiteralPath $file);bytes=$bytes}
}
Write-Json @($backups.Keys|ForEach-Object {@{path=$_;existed=$backups[$_].existed}}) "$output\restore-private\restore.json"
$sinks=New-Object System.Collections.Generic.List[System.Net.Sockets.UdpClient]
$reports=New-Object System.Collections.Generic.List[object]
$process=$null
$processStarted=$false
try {
    foreach($port in @(2237,2239)) {
        $socket=New-Object System.Net.Sockets.UdpClient
        $socket.ExclusiveAddressUse=$true
        try { $socket.Client.Bind((New-Object System.Net.IPEndPoint([Net.IPAddress]::Loopback,$port))) }
        catch { $socket.Dispose(); throw "Porta UDP $port occupata. Chiudi i programmi collegati a Decodium." }
        $sinks.Add($socket)
    }
    $index=0
    foreach($version in @('before','after','after','before')) {
        $index++
        Write-Host "Prova $index/4: $version. Non interagire con la finestra."
        $run=Join-Path $output "$index-$version"
        New-Item -ItemType Directory -Path "$run\app","$run\settings\Decodium","$run\temp" -Force|Out-Null
        # Work on a private portable copy; installed programs are never edited.
        Copy-Item -Path "$root\versions\$version\*" -Destination "$run\app" -Recurse -Force
        $qml=Join-Path $run 'app\qml\decodium\Main.qml'
        $original=[IO.File]::ReadAllText($qml)
        if(([regex]::Matches($original,'GlassApplicationWindow \{')).Count -ne 1) { throw 'QML inatteso' }
        $fixtures=@($manifest.fixtures|ForEach-Object { @{id=$_.id;mode=$_.mode;path=(Join-Path $root $_.file).Replace('\','/')} })
        $probe=[IO.File]::ReadAllText("$root\Probe.qml.inc").Replace('__FIXTURES__',($fixtures|ConvertTo-Json -Depth 5 -Compress)).Replace('__THREADS__',[string]$Threads).Replace('__LOWCPU__',([string][bool]$LowCpu).ToLowerInvariant())
        [IO.File]::WriteAllText($qml,$original.Replace('GlassApplicationWindow {',('GlassApplicationWindow {'+"`n"+$probe)),$utf8)
        $rig='WinPerf'+[guid]::NewGuid().ToString('N').Substring(0,12)
        [IO.File]::WriteAllText("$run\settings\Decodium\Decodium3.ini",("[$rig]`n_ProfileSettingsInitialized=true`nftThreadsAuto=false`nftThreads=$Threads`nLowCpuMode="+([string][bool]$LowCpu).ToLowerInvariant()+"`n"),$utf8)
        New-Item -ItemType Directory -Path $testConfig -Force|Out-Null
        foreach($file in $legacyFiles) { [IO.File]::WriteAllText($file,'',$utf8) }
        $psi=New-Object System.Diagnostics.ProcessStartInfo
        $psi.FileName=Join-Path $run 'app\decodium.exe';$psi.WorkingDirectory=Join-Path $run 'app'
        $psi.Arguments="--decodium-graphics-worker --test-mode --rig-name $rig --lab-no-cat --lab-no-monitor --lab-mode FT8 --lab-callsign IU8LMC --lab-grid JM75 --lab-quit-ms $appTimeoutMs"
        $psi.UseShellExecute=$false;$psi.RedirectStandardOutput=$true;$psi.RedirectStandardError=$true
        foreach($key in @($psi.EnvironmentVariables.Keys)) { if($key -match '^(DECODIUM_|QT_|QSG_|QML_|OMP_)') { $psi.EnvironmentVariables.Remove($key) } }
        $psi.EnvironmentVariables['DECODIUM_LAB_SETTINGS_DIR']=Join-Path $run 'settings'
        $psi.EnvironmentVariables['DECODIUM_FORCE_STANDALONE_UDP']='1'
        $psi.EnvironmentVariables['DECODIUM_GRAPHICS_BACKEND']=$GraphicsBackend
        $psi.EnvironmentVariables['TEMP']=Join-Path $run 'temp'
        $psi.EnvironmentVariables['TMP']=Join-Path $run 'temp'
        $psi.EnvironmentVariables['DECODIUM_DISABLE_QML_CACHE']='1'
        $process=New-Object System.Diagnostics.Process;$process.StartInfo=$psi
        $timer=[Diagnostics.Stopwatch]::StartNew();$processStarted=$process.Start()
        if(!$processStarted) { throw 'Impossibile avviare Decodium.' }
        $stdout=$process.StandardOutput.ReadToEndAsync();$stderr=$process.StandardError.ReadToEndAsync()
        $samples=New-Object System.Collections.Generic.List[object]
        while(!$process.HasExited) {
            $process.Refresh()
            if($timer.Elapsed.TotalSeconds -gt ($appTimeoutSeconds+20)) { throw 'Timeout del programma; prova non valida.' }
            try { $samples.Add(@{seconds=$timer.Elapsed.TotalSeconds;cpuSeconds=$process.TotalProcessorTime.TotalSeconds;workingSetBytes=$process.WorkingSet64;privateBytes=$process.PrivateMemorySize64}) } catch { if(!$process.HasExited) { throw } }
            foreach($socket in $sinks) { while($socket.Available -gt 0) { $remote=New-Object Net.IPEndPoint([Net.IPAddress]::Any,0);[void]$socket.Receive([ref]$remote) } }
            Start-Sleep -Milliseconds 500
        }
        $process.WaitForExit();$log=$stdout.Result+"`n"+$stderr.Result
        [IO.File]::WriteAllText("$run\console.log",$log,$utf8)
        $events=@($log -split '\r?\n'|Where-Object { $_.Contains('[PERFTEST] ') }|ForEach-Object { ($_.Substring($_.IndexOf('[PERFTEST] ')+11))|ConvertFrom-Json })
        $starts=@($events|Where-Object {$_.event -eq 'start'})
        $cases=@($events|Where-Object {$_.event -eq 'result'})
        $done=@($events|Where-Object {$_.event -eq 'done'})
        $valid=$process.ExitCode -eq 0 -and $cases.Count -eq $manifest.fixtures.Count -and $starts.Count -eq $cases.Count -and $done.Count -eq 1 -and $log.Contains('[LAB] CAT auto-connect disabled') -and !$log.Contains('QNativeSocketEngine::writeDatagram() was called not in')
        $selected=if($GraphicsBackend -eq 'd3d11') {'Qt Quick graphics backend selected: d3d11'} else {'Qt Quick graphics: using the GPU-independent Qt software renderer'}
        $valid=$valid -and $log.Contains($selected) -and $log.Contains('[LAB] Early storage isolation active; normal migration skipped')
        foreach($start in $starts) { if(!$start.cleared -or $start.threads -ne $Threads -or $start.lowCpu -ne [bool]$LowCpu) { $valid=$false } }
        foreach($case in $cases) { if($case.timeout -or $case.tx -or $case.stillDecoding) { $valid=$false } }
        foreach($fixture in $manifest.fixtures) {
            $case=@($cases|Where-Object {$_.caseId -eq $fixture.id})
            if($case.Count -ne 1) { $valid=$false }
            elseif($fixture.expected -and @($case[0].rows|Where-Object {Test-ExpectedDecode -Mode $fixture.mode -Message $_.message -Expected $fixture.expected}).Count -eq 0) { $valid=$false }
        }
        $report=@{version=$version;index=$index;valid=$valid;exit=$process.ExitCode;events=$events;samples=@($samples.ToArray());binarySha256=Hash $psi.FileName}
        Write-Json $report "$run\result.json";$reports.Add($report)
        $process.Dispose();$process=$null;$processStarted=$false
        if(!$valid) { throw "Prova $index non valida. Conserva la cartella risultati." }
    }

    $comparisons=@(Compare-Decodes -Reports $reports.ToArray() -Fixtures $manifest.fixtures)
    Write-Json $comparisons "$output\comparison.json"
    if(@($comparisons|Where-Object {!$_.identical}).Count) { Write-Host 'DIFFERENZE nei decode: invia i risultati per analisi.' -ForegroundColor Yellow }
    else { Write-Host 'Messaggi identici nei confronti A/B eseguiti.' -ForegroundColor Green }
}
finally {
    # A failed Start has no associated process; cleanup must still restore settings.
    if($process -and $processStarted) {
        try {
            if(!$process.HasExited) { $process.Kill();$process.WaitForExit() }
        } catch { Write-Warning "Chiusura del processo: $_" }
    }
    foreach($socket in $sinks) { $socket.Dispose() }
    foreach($file in $legacyFiles) {
        if($backups[$file].existed) { [IO.File]::WriteAllBytes($file,$backups[$file].bytes) }
        elseif(Test-Path -LiteralPath $file) { Remove-Item -LiteralPath $file }
    }
    Remove-Item -LiteralPath "$output\restore-private" -Recurse
}
Write-Host "Risultati: $output"
