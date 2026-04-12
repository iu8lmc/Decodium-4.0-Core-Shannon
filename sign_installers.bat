@echo off
set SIGNTOOL="C:\Program Files (x86)\Windows Kits\10\bin\10.0.22621.0\x64\signtool.exe"
set PFX=C:\Users\IU8LMC\decodium_codesign.pfx
set PASS=Decodium2601!
set SRC=C:\Users\IU8LMC\Downloads\WSJTX_3.0_Source

%SIGNTOOL% sign /f %PFX% /p %PASS% /fd SHA256 /tr http://timestamp.digicert.com /td SHA256 "%SRC%\Decodium_3.0_2602281900_FT2_x64_Setup.exe"
%SIGNTOOL% sign /f %PFX% /p %PASS% /fd SHA256 /tr http://timestamp.digicert.com /td SHA256 "%SRC%\Decodium_3.0_2602281900_FT2_x86_Setup.exe"

echo === Done signing installers ===
