@echo off
setlocal
set PATH=C:\msys64\mingw64\bin;C:\msys64\usr\bin;C:\Windows\System32;C:\Windows
set ROOT=C:\Users\IU8LMC\Downloads\Decodium3
set CMAKE=E:\wsjt_env\Lib\site-packages\cmake\data\bin\cmake.exe

echo [1/3] Riconfigurazione cmake (applica -Wl,-s)...
%CMAKE% -S "%ROOT%" -B "%ROOT%\build_mingw64" -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release > "%ROOT%\reconfigure_out.txt" 2>&1
if errorlevel 1 (
    echo ERRORE riconfigurazione! Vedi reconfigure_out.txt
    type "%ROOT%\reconfigure_out.txt"
    goto :end
)
echo     OK — riconfigurato

echo [2/3] Compilazione...
%CMAKE% --build "%ROOT%\build_mingw64" --target decodium_qml -j4 >> "%ROOT%\reconfigure_out.txt" 2>&1
if errorlevel 1 (
    echo ERRORE compilazione! Vedi reconfigure_out.txt
    goto :end
)
echo     OK — compilato

for %%f in ("%ROOT%\build_mingw64\decodium.exe") do echo     decodium.exe: %%~zf bytes (%%~tf)

echo [3/3] Packaging dist_64bit...
set DIST=%ROOT%\dist_64bit
set BUILD=%ROOT%\build_mingw64
set SRC_DIST=C:\Users\IU8LMC\Downloads\WSJTX_3.0_Source\dist_64bit

if not exist "%DIST%" mkdir "%DIST%"

for %%f in (jt9.exe message_aggregator.exe wsprd.exe udp_daemon.exe rigctl-decodium.exe rigctld-decodium.exe rigctlcom-decodium.exe cty.dat ALLCALL7.TXT JPLEPH ft2logo.png) do (
    if exist "%SRC_DIST%\%%f" copy /Y "%SRC_DIST%\%%f" "%DIST%\%%f" > nul
)
for %%d in (sounds Palettes sqldrivers audio bearer mediaservice printsupport) do (
    if exist "%SRC_DIST%\%%d\" xcopy /E /Y /Q "%SRC_DIST%\%%d\*" "%DIST%\%%d\" > nul 2>&1
)
copy /Y "%BUILD%\decodium.exe" "%DIST%\decodium.exe" > nul
for %%f in ("%BUILD%\*.dll") do copy /Y "%%f" "%DIST%\" > nul 2>&1
for %%d in (platforms imageformats styles) do (
    if exist "%BUILD%\%%d\" xcopy /E /Y /Q "%BUILD%\%%d\*" "%DIST%\%%d\" > nul 2>&1
)
if exist "%BUILD%\qml\" xcopy /E /Y /Q "%BUILD%\qml\*" "%DIST%\qml\" > nul 2>&1
if exist "%ROOT%\icons\windows-icons\decodium.ico" copy /Y "%ROOT%\icons\windows-icons\decodium.ico" "%DIST%\decodium.ico" > nul

echo     dist_64bit aggiornata

echo [Installer] Inno Setup...
cd /d "%ROOT%"
"C:\Program Files (x86)\Inno Setup 6\ISCC.exe" decodium_x64.iss > "%ROOT%\build_installer_out.txt" 2>&1
if errorlevel 1 (
    echo ERRORE installer! Vedi build_installer_out.txt
    goto :end
)

echo.
echo =============================================
for %%f in ("%ROOT%\Decodium4_*_x64_Setup.exe") do (
    echo INSTALLER: %%f
    echo DIMENSIONE: %%~zf bytes
)
echo =============================================

:end
endlocal
