@echo off
set SIGNTOOL="C:\Program Files (x86)\Windows Kits\10\bin\10.0.22621.0\x64\signtool.exe"
set PFX=C:\Users\IU8LMC\decodium_codesign.pfx
set PASS=Decodium2601!
set SRC=C:\Users\IU8LMC\Downloads\WSJTX_3.0_Source

echo === Signing x64 executables ===
for %%f in (%SRC%\dist_64bit\wsjtx.exe %SRC%\dist_64bit\jt9.exe %SRC%\dist_64bit\message_aggregator.exe %SRC%\dist_64bit\wsprd.exe %SRC%\dist_64bit\udp_daemon.exe) do (
    %SIGNTOOL% sign /f %PFX% /p %PASS% /fd SHA256 /tr http://timestamp.digicert.com /td SHA256 "%%f" 2>nul
    if errorlevel 1 echo Warning: could not sign %%f
)

echo === Signing x86 executables ===
for %%f in (%SRC%\dist_32bit\wsjtx.exe %SRC%\dist_32bit\jt9.exe %SRC%\dist_32bit\message_aggregator.exe %SRC%\dist_32bit\wsprd.exe %SRC%\dist_32bit\udp_daemon.exe) do (
    %SIGNTOOL% sign /f %PFX% /p %PASS% /fd SHA256 /tr http://timestamp.digicert.com /td SHA256 "%%f" 2>nul
    if errorlevel 1 echo Warning: could not sign %%f
)

echo === Done signing ===
