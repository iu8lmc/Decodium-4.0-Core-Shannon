; Decodium Installer Script per Inno Setup 6.x
; Pensato per essere pilotato sia localmente sia da GitHub Actions.

#ifndef AppName
  #define AppName "Decodium"
#endif
#ifndef AppVersion
  #define AppVersion "1.0.4"
#endif
#ifndef AppPublisher
  #define AppPublisher "IU8LMC"
#endif
#ifndef AppExeName
  #define AppExeName "decodium.exe"
#endif
#ifndef BuildDir
  #define BuildDir "..\build_mingw64"
#endif
#ifndef SourceRoot
  #define SourceRoot ".."
#endif
#ifndef OutputDir
  #define OutputDir ".\output"
#endif
#ifndef OutputBaseFilename
  #define OutputBaseFilename "Decodium_" + AppVersion + "_Setup_x64"
#endif

[Setup]
AppId={{D3C0D1A4-4000-4A10-8C64-6F7C3A6F4000}
AppName={#AppName}
AppVersion={#AppVersion}
AppVerName={#AppName} {#AppVersion}
AppPublisher={#AppPublisher}
AppPublisherURL=https://github.com/elisir80/Decodium-4.0-Core-Shannon
DefaultDirName={autopf}\{#AppName}
DefaultGroupName={#AppName}
AllowNoIcons=yes
OutputDir={#OutputDir}
OutputBaseFilename={#OutputBaseFilename}
SetupIconFile={#SourceRoot}\icons\windows-icons\decodium.ico
WizardImageFile=compiler:WizClassicImage-IS.bmp
WizardSmallImageFile=compiler:WizClassicSmallImage-IS.bmp
Compression=lzma2/ultra64
SolidCompression=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
UninstallDisplayIcon={app}\{#AppExeName}
ShowLanguageDialog=auto
WizardStyle=modern
LicenseFile={#SourceRoot}\COPYING

[Languages]
Name: "italian"; MessagesFile: "compiler:Languages\Italian.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
; Eseguibile principale
Source: "{#BuildDir}\{#AppExeName}"; DestDir: "{app}"; Flags: ignoreversion

; Tutte le DLL nella root del build dir (Qt6, MinGW runtime, lib*)
Source: "{#BuildDir}\*.dll"; DestDir: "{app}"; Flags: ignoreversion

; File dati principali
Source: "{#BuildDir}\cty.dat"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist
Source: "{#BuildDir}\ALLCALL7.TXT"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist
Source: "{#SourceRoot}\COPYING"; DestDir: "{app}"; DestName: "COPYING.txt"; Flags: ignoreversion skipifsourcedoesntexist

; Plugin Qt — platforms (qwindows.dll, ecc.)
Source: "{#BuildDir}\platforms\*"; DestDir: "{app}\platforms"; Flags: ignoreversion recursesubdirs createallsubdirs skipifsourcedoesntexist

; Plugin Qt — stili
Source: "{#BuildDir}\styles\*"; DestDir: "{app}\styles"; Flags: ignoreversion recursesubdirs createallsubdirs skipifsourcedoesntexist

; Plugin Qt — imageformats
Source: "{#BuildDir}\imageformats\*"; DestDir: "{app}\imageformats"; Flags: ignoreversion recursesubdirs createallsubdirs skipifsourcedoesntexist

; Plugin Qt — tls (OpenSSL)
Source: "{#BuildDir}\tls\*"; DestDir: "{app}\tls"; Flags: ignoreversion recursesubdirs createallsubdirs skipifsourcedoesntexist

; Plugin Qt — networkinformation
Source: "{#BuildDir}\networkinformation\*"; DestDir: "{app}\networkinformation"; Flags: ignoreversion recursesubdirs createallsubdirs skipifsourcedoesntexist

; Plugin Qt — generic
Source: "{#BuildDir}\generic\*"; DestDir: "{app}\generic"; Flags: ignoreversion recursesubdirs createallsubdirs skipifsourcedoesntexist

; Plugin Qt — qmltooling
Source: "{#BuildDir}\qmltooling\*"; DestDir: "{app}\qmltooling"; Flags: ignoreversion recursesubdirs createallsubdirs skipifsourcedoesntexist

; Moduli QML (mshv, QtQuick, QtQml, ecc.)
Source: "{#BuildDir}\qml\*"; DestDir: "{app}\qml"; Flags: ignoreversion recursesubdirs createallsubdirs

; Qt QML modules folder
Source: "{#BuildDir}\Qt\*"; DestDir: "{app}\Qt"; Flags: ignoreversion recursesubdirs createallsubdirs skipifsourcedoesntexist

; Traduzioni
Source: "{#BuildDir}\*.qm"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist

; Risorse runtime
Source: "{#BuildDir}\sounds\*"; DestDir: "{app}\sounds"; Flags: ignoreversion recursesubdirs createallsubdirs skipifsourcedoesntexist
Source: "{#BuildDir}\Palettes\*"; DestDir: "{app}\Palettes"; Flags: ignoreversion recursesubdirs createallsubdirs skipifsourcedoesntexist

[Icons]
Name: "{group}\{#AppName}";              Filename: "{app}\{#AppExeName}"; IconFilename: "{app}\{#AppExeName}"
Name: "{group}\Disinstalla {#AppName}";  Filename: "{uninstallexe}"
Name: "{autodesktop}\{#AppName}";        Filename: "{app}\{#AppExeName}"; IconFilename: "{app}\{#AppExeName}"; Tasks: desktopicon

[Registry]
Root: HKCU; Subkey: "Software\{#AppPublisher}\{#AppName}"; ValueType: string; ValueName: "InstallPath"; ValueData: "{app}"; Flags: uninsdeletekey
Root: HKCU; Subkey: "Software\{#AppPublisher}\{#AppName}"; ValueType: string; ValueName: "Version"; ValueData: "{#AppVersion}"

[Run]
Filename: "{app}\{#AppExeName}"; Description: "{cm:LaunchProgram,{#StringChange(AppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent
