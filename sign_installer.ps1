$ErrorActionPreference = "Stop"

# Create self-signed code signing certificate
$cert = New-SelfSignedCertificate -Type CodeSigningCert `
    -Subject "CN=IU8LMC Decodium, O=IU8LMC, C=IT" `
    -FriendlyName "Decodium Code Signing" `
    -CertStoreLocation "Cert:\CurrentUser\My" `
    -NotAfter (Get-Date).AddYears(5) `
    -KeyAlgorithm RSA -KeyLength 2048 -HashAlgorithm SHA256

Write-Output "Certificate created: $($cert.Thumbprint)"

# Export to PFX
$pwd = ConvertTo-SecureString -String "Decodium2026!" -Force -AsPlainText
Export-PfxCertificate -Cert $cert -FilePath "C:\Users\IU8LMC\decodium_codesign_new.pfx" -Password $pwd | Out-Null
Write-Output "PFX exported to decodium_codesign_new.pfx"

# Sign the installer
$signtool = "C:\Program Files (x86)\Windows Kits\10\bin\10.0.22621.0\x64\signtool.exe"
$installer = "C:\Users\IU8LMC\Downloads\WSJTX_3.0_Source\Decodium_3.0_2602281659_FT2_x64_Setup.exe"

& $signtool sign /f "C:\Users\IU8LMC\decodium_codesign_new.pfx" /p "Decodium2026!" /fd SHA256 /tr http://timestamp.digicert.com /td SHA256 $installer

if ($LASTEXITCODE -eq 0) {
    Write-Output "Installer signed successfully!"
    & $signtool verify /pa $installer
} else {
    Write-Output "Signing failed with exit code $LASTEXITCODE"
}
