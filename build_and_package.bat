@echo off
setlocal enabledelayedexpansion

set ROOT=C:\Users\IU8LMC\Downloads\Decodium3
set BUILD=%ROOT%\build_mingw64
set DIST=%ROOT%\dist_64bit
set SRC_DIST=C:\Users\IU8LMC\Downloads\WSJTX_3.0_Source\dist_64bit
set ISS=%ROOT%\installer\decodium_setup.iss
set ISCC="C:\Program Files (x86)\Inno Setup 6\ISCC.exe"
set PATH=C:\msys64\mingw64\bin;C:\msys64\usr\bin;C:\Windows\System32;C:\Windows;C:\Windows\System32\WindowsPowerShell\v1.0
set CMAKE=C:\msys64\mingw64\bin\cmake.exe

echo ============================================================
echo  Decodium4 — Compile + Package (build_mingw64)
echo ============================================================
echo.

:: ── Step 1: Compila in build_mingw64 ────────────────────────
echo [1/4] Compilazione (build_mingw64)...
%CMAKE% --build "%BUILD%" --target decodium_qml -j4 > "%ROOT%\build_ps_out.txt" 2>&1
if errorlevel 1 (
    echo ERRORE: compilazione fallita! Vedi build_ps_out.txt
    goto :error
)
if not exist "%BUILD%\decodium.exe" (
    echo ERRORE: decodium.exe non trovato in build_mingw64!
    goto :error
)
for %%f in ("%BUILD%\decodium.exe") do echo      OK — decodium.exe compilato (%%~tf, %%~zf bytes)

:: ── Step 2: Prepara dist_64bit dentro Decodium3 ──────────────
echo [2/4] Preparazione dist_64bit...

if not exist "%DIST%" mkdir "%DIST%"

:: File dati e exe di supporto da WSJTX_3.0_Source
for %%f in (jt9.exe message_aggregator.exe wsprd.exe udp_daemon.exe
            rigctl-decodium.exe rigctld-decodium.exe rigctlcom-decodium.exe
            cty.dat ALLCALL7.TXT JPLEPH ft2logo.png) do (
    if exist "%SRC_DIST%\%%f" copy /Y "%SRC_DIST%\%%f" "%DIST%\%%f" > nul
)

:: Cartelle statiche
for %%d in (sounds Palettes sqldrivers audio bearer mediaservice printsupport) do (
    if exist "%SRC_DIST%\%%d\" xcopy /E /Y /Q "%SRC_DIST%\%%d\*" "%DIST%\%%d\" > nul 2>&1
)

:: Exe principale appena compilato (diretto, senza strip)
copy /Y "%BUILD%\decodium.exe" "%DIST%\decodium.exe" > nul
echo      OK — decodium.exe copiato in dist_64bit

:: DLL Qt6 e runtime mingw64
for %%f in ("%BUILD%\*.dll") do copy /Y "%%f" "%DIST%\" > nul 2>&1

:: Qt plugins
for %%d in (platforms imageformats styles) do (
    if exist "%BUILD%\%%d\" xcopy /E /Y /Q "%BUILD%\%%d\*" "%DIST%\%%d\" > nul 2>&1
)

:: QML files (caricati da filesystem a runtime)
if exist "%BUILD%\qml\" xcopy /E /Y /Q "%BUILD%\qml\*" "%DIST%\qml\" > nul 2>&1

:: Icona desktop
if exist "%ROOT%\icons\windows-icons\decodium.ico" (
    copy /Y "%ROOT%\icons\windows-icons\decodium.ico" "%DIST%\decodium.ico" > nul
)

echo      OK — dist_64bit pronta

:: ── Step 3: Build installer con Inno Setup ───────────────────
echo [3/4] Build installer x64...
cd /d "%ROOT%"
%ISCC% "%ISS%" > "%ROOT%\build_installer_out.txt" 2>&1
if errorlevel 1 (
    echo ERRORE: installer fallito! Vedi build_installer_out.txt
    goto :error
)
echo      OK — installer creato

:: ── Risultato ────────────────────────────────────────────────
echo.
echo ============================================================
echo  BUILD COMPLETATO!
for %%f in ("%ROOT%\installer\output\Decodium_*_Setup_x64.exe") do (
    echo  Installer: %%f
    echo  Dimensione: %%~zf bytes
)
echo ============================================================
goto :end

:error
echo.
echo BUILD FALLITA — controlla i log in %ROOT%
exit /b 1

:end
endlocal
