; Decodium Installer Script per Inno Setup 6.x
; Pensato per essere pilotato sia localmente sia da GitHub Actions.

#ifndef AppName
  #define AppName "Decodium"
#endif
#ifndef AppVersion
  #define AppVersion "1.0.69"
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
AppPublisherURL=https://github.com/iu8lmc/Decodium-4.0-Core-Shannon
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

[InstallDelete]
; Pulisci cache QML compilata da versioni precedenti — Qt6 la rigenera
; automaticamente dal .qml sorgente aggiornato. Senza questo, il vecchio
; .qmlc può essere caricato al posto del .qml nuovo, causando bug fantasma.
Type: filesandordirs; Name: "{app}\qml\decodium\*.qmlc"
Type: filesandordirs; Name: "{app}\qml\decodium\components\*.qmlc"
Type: filesandordirs; Name: "{app}\qml\decodium\qmlcache"
Type: filesandordirs; Name: "{app}\qmlcache"
; Pulisci anche le vecchie cache Qt/QML utente condivise da installazioni
; precedenti. Decodium usa una cache isolata per versione+path.
Type: filesandordirs; Name: "{localappdata}\IU8LMC\Decodium\cache\qmlcache"
Type: filesandordirs; Name: "{localappdata}\IU8LMC\Decodium\qmlcache"
Type: filesandordirs; Name: "{localappdata}\Decodium\cache\qmlcache"
Type: filesandordirs; Name: "{localappdata}\decodium4\cache\qmlcache"

[Files]
; Copia l'intero bundle portabile già preparato da windeployqt.
; In questo modo installer e portable restano sempre allineati 1:1.
Source: "{#BuildDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

; File licenza fuori dal bundle
Source: "{#SourceRoot}\COPYING"; DestDir: "{app}"; DestName: "COPYING.txt"; Flags: ignoreversion skipifsourcedoesntexist

[Icons]
Name: "{group}\{#AppName}";              Filename: "{app}\{#AppExeName}"; IconFilename: "{app}\{#AppExeName}"
Name: "{group}\Disinstalla {#AppName}";  Filename: "{uninstallexe}"
Name: "{autodesktop}\{#AppName}";        Filename: "{app}\{#AppExeName}"; IconFilename: "{app}\{#AppExeName}"; Tasks: desktopicon

[Registry]
Root: HKCU; Subkey: "Software\{#AppPublisher}\{#AppName}"; ValueType: string; ValueName: "InstallPath"; ValueData: "{app}"; Flags: uninsdeletekey
Root: HKCU; Subkey: "Software\{#AppPublisher}\{#AppName}"; ValueType: string; ValueName: "Version"; ValueData: "{#AppVersion}"

[Run]
Filename: "{app}\{#AppExeName}"; Description: "{cm:LaunchProgram,{#StringChange(AppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent
