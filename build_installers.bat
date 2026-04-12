@echo off
setlocal enabledelayedexpansion

set SIGNTOOL="C:\Program Files (x86)\Windows Kits\10\bin\10.0.22621.0\x64\signtool.exe"
set ISCC="C:\Program Files (x86)\Inno Setup 6\ISCC.exe"
set PFX=C:\Users\IU8LMC\decodium_codesign.pfx
set PASS=Decodium3!
set SRC=C:\Users\IU8LMC\Downloads\WSJTX_3.0_Source
set BUILD=2603182239

echo ============================================
echo  Decodium 3.0 FT2 Raptor - Build %BUILD% Installers
echo ============================================
echo.

echo === Step 1: Signing x64 executables ===
for %%f in (wsjtx.exe jt9.exe message_aggregator.exe wsprd.exe udp_daemon.exe rigctl-wsjtx.exe rigctld-wsjtx.exe rigctlcom-wsjtx.exe) do (
    if exist "%SRC%\dist_64bit\%%f" (
        echo   Signing x64\%%f...
        %SIGNTOOL% sign /f %PFX% /p %PASS% /fd SHA256 /tr http://timestamp.digicert.com /td SHA256 "%SRC%\dist_64bit\%%f"
        if errorlevel 1 echo   WARNING: Failed to sign %%f
    ) else (
        echo   SKIP: %%f not found
    )
)
echo.

echo === Step 2: Signing x86 executables ===
for %%f in (wsjtx.exe jt9.exe message_aggregator.exe wsprd.exe udp_daemon.exe rigctl-wsjtx.exe rigctld-wsjtx.exe rigctlcom-wsjtx.exe) do (
    if exist "%SRC%\dist_32bit\%%f" (
        echo   Signing x86\%%f...
        %SIGNTOOL% sign /f %PFX% /p %PASS% /fd SHA256 /tr http://timestamp.digicert.com /td SHA256 "%SRC%\dist_32bit\%%f"
        if errorlevel 1 echo   WARNING: Failed to sign %%f
    ) else (
        echo   SKIP: %%f not found
    )
)
echo.

echo === Step 3: Building x64 installer ===
%ISCC% "%SRC%\decodium_x64.iss"
if errorlevel 1 (
    echo ERROR: x64 installer build failed!
    goto :error
)
echo.

echo === Step 4: Building x86 installer ===
%ISCC% "%SRC%\decodium_x86.iss"
if errorlevel 1 (
    echo ERROR: x86 installer build failed!
    goto :error
)
echo.

echo === Step 5: Signing installers ===
for %%f in (Decodium_3.0_%BUILD%_FT2_x64_Setup.exe Decodium_3.0_%BUILD%_FT2_x86_Setup.exe) do (
    if exist "%SRC%\%%f" (
        echo   Signing %%f...
        %SIGNTOOL% sign /f %PFX% /p %PASS% /fd SHA256 /tr http://timestamp.digicert.com /td SHA256 "%SRC%\%%f"
        if errorlevel 1 echo   WARNING: Failed to sign %%f
    ) else (
        echo   ERROR: %%f not found!
    )
)
echo.

echo ============================================
echo  BUILD COMPLETE!
echo  Installers:
echo    %SRC%\Decodium_3.0_%BUILD%_FT2_x64_Setup.exe
echo    %SRC%\Decodium_3.0_%BUILD%_FT2_x86_Setup.exe
echo ============================================
goto :end

:error
echo.
echo BUILD FAILED - check errors above
exit /b 1

:end
endlocal
