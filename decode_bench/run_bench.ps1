<#
  run_bench.ps1 — wrapper Windows per decode_bench/bench.py

  Imposta il PATH con le DLL MinGW/Qt (necessarie a ft8_stage_compare.exe),
  individua il binario del decoder e lancia bench.py inoltrando tutti gli
  argomenti extra.

  Esempi:
    .\run_bench.ps1 -Profile harvest -Snr "-16:-26:-1" -Trials 30 -WithJt9 -Label baseline
    .\run_bench.ps1 -Quick -WithJt9 -Label smoke
    .\run_bench.ps1 -Profile prod -Extra "--fdop 1.0 --delay 2.0 --label fading"
#>
param(
  [switch]$Multi,                       # bench_multi.py (banco multi-segnale)
  [switch]$Real,                        # bench_real.py (catture reali off-air)
  [string]$Wavs = "",                   # bench_real.py: cartella/glob dei .wav
  [string]$Profile = "deep",            # bench.py: profilo singolo
  [string]$Profiles = "deep,harvest",   # bench_multi/real: profili a confronto
  [string]$Snr = "-16:-26:-1",
  [int]$Trials = 30,
  [int]$Limit = 50,                     # bench_real.py: max file
  [switch]$WithJt9,
  [switch]$Quick,
  [string]$Label = "run",
  [string]$Decodium = "",
  [string]$MingwBin = "C:\msys64\mingw64\bin",
  [string]$WsjtBin = "C:\WSJT\wsjtx\bin",
  [string]$Extra = ""
)

$ErrorActionPreference = "Stop"
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$root = Split-Path -Parent $here

if ([string]::IsNullOrEmpty($Decodium)) {
  $Decodium = Join-Path $root "build_mingw64\tests\ft8_stage_compare.exe"
}
if (-not (Test-Path $Decodium)) {
  Write-Error "ft8_stage_compare.exe non trovato: $Decodium`nCompila con: cmake --build build_mingw64 --target ft8_stage_compare"
  exit 1
}

# DLL runtime (MinGW + Qt deployate nel build dir) sul PATH
$env:PATH = "$MingwBin;$env:PATH"

# Individua python
$py = (Get-Command py -ErrorAction SilentlyContinue)
if ($py) { $pyExe = "py" } else {
  $py = (Get-Command python -ErrorAction SilentlyContinue)
  if (-not $py) { Write-Error "python non trovato sul PATH"; exit 1 }
  $pyExe = "python"
}

$script = if ($Real) { "bench_real.py" } elseif ($Multi) { "bench_multi.py" } else { "bench.py" }
$argList = @(
  (Join-Path $here $script),
  "--decodium", $Decodium,
  "--label", $Label,
  "--jt9", (Join-Path $WsjtBin "jt9.exe")
)
if ($Real) {
  if ([string]::IsNullOrWhiteSpace($Wavs)) { Write-Error "-Real richiede -Wavs <cartella o glob>"; exit 1 }
  $argList += @("--wavs", $Wavs, "--profiles", $Profiles, "--limit", $Limit)
} elseif ($Multi) {
  $argList += @("--snr", $Snr, "--trials", $Trials, "--profiles", $Profiles,
                "--ft8sim", (Join-Path $WsjtBin "ft8sim.exe"))
} else {
  $argList += @("--snr", $Snr, "--trials", $Trials, "--profile", $Profile,
                "--ft8sim", (Join-Path $WsjtBin "ft8sim.exe"))
  if ($Quick) { $argList += "--quick" }
}
if ($WithJt9) { $argList += "--with-jt9" }
if (-not [string]::IsNullOrWhiteSpace($Extra)) { $argList += $Extra.Split(" ") }

Write-Host "==> $pyExe $($argList -join ' ')" -ForegroundColor Cyan
& $pyExe @argList
exit $LASTEXITCODE
