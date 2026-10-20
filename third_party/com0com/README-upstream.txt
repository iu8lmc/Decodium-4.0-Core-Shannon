You can use the signed or unsigned version of com0com.

The unsigned (test-signed) x64 version of com0com will not load by default on x64 Windows.

You will need to enable test signing
  - îpen the Windows command prompt as "Run as Administrator"
  - enter commands
      bcdedit -set loadoptions DISABLE_INTEGRITY_CHECKS
      bcdedit -set TESTSIGNING ON
  - reboot the computer.

NOTE: Enabling test signing will impair computer security.
