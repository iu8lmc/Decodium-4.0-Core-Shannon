#define MyAppName "Decodium 3.0 FT2 Raptor x86"
#define MyAppVersion "3.0"
#define MyAppPublisher "IU8LMC"
#define MyAppExeName "decodium.exe"
#define MyBuildTag "2603182239"
#define DistDir "dist_32bit"

[Setup]
AppId={{F9B8D4E2-5C3E-4F7A-8B2C-4D6E8F9A1B3C}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppVerName={#MyAppName} {#MyBuildTag}
AppPublisher={#MyAppPublisher}
AppPublisherURL=https://www.qrz.com/db/IU8LMC
DefaultDirName={autopf}\Decodium_3.0_FT2_x86
DefaultGroupName=Decodium 3.0 FT2
OutputDir=.
OutputBaseFilename=Decodium_3.0_{#MyBuildTag}_FT2_x86_Setup
SetupIconFile=icons\windows-icons\decodium.ico
Compression=lzma2/ultra64
SolidCompression=yes
WizardStyle=modern
UninstallDisplayIcon={app}\decodium.exe
PrivilegesRequired=lowest
; SignTool and SignedUninstaller require IDE config; exes are already signed

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"
Name: "italian"; MessagesFile: "compiler:Languages\Italian.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"

[Files]
; Main executables
Source: "{#DistDir}\decodium.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#DistDir}\jt9.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#DistDir}\message_aggregator.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#DistDir}\wsprd.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#DistDir}\rigctl-decodium.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#DistDir}\rigctld-decodium.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#DistDir}\rigctlcom-decodium.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#DistDir}\udp_daemon.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#DistDir}\ChronoGPS.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#DistDir}\chronogps_icon.ico"; DestDir: "{app}"; Flags: ignoreversion

; DLLs
Source: "{#DistDir}\*.dll"; DestDir: "{app}"; Flags: ignoreversion

; Data files
Source: "{#DistDir}\cty.dat"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#DistDir}\ALLCALL7.TXT"; DestDir: "{app}"; Flags: onlyifdoesntexist
Source: "{#DistDir}\ft2logo.png"; DestDir: "{app}"; Flags: ignoreversion

; Qt plugins
Source: "{#DistDir}\platforms\*"; DestDir: "{app}\platforms"; Flags: ignoreversion recursesubdirs
Source: "{#DistDir}\audio\*"; DestDir: "{app}\audio"; Flags: ignoreversion recursesubdirs
Source: "{#DistDir}\imageformats\*"; DestDir: "{app}\imageformats"; Flags: ignoreversion recursesubdirs
Source: "{#DistDir}\sqldrivers\*"; DestDir: "{app}\sqldrivers"; Flags: ignoreversion recursesubdirs
Source: "{#DistDir}\bearer\*"; DestDir: "{app}\bearer"; Flags: ignoreversion recursesubdirs
Source: "{#DistDir}\styles\*"; DestDir: "{app}\styles"; Flags: ignoreversion recursesubdirs
Source: "{#DistDir}\mediaservice\*"; DestDir: "{app}\mediaservice"; Flags: ignoreversion recursesubdirs

; Note: printsupport not available on x86 build

; Sounds and palettes
Source: "{#DistDir}\sounds\*"; DestDir: "{app}\sounds"; Flags: ignoreversion recursesubdirs
Source: "{#DistDir}\Palettes\*"; DestDir: "{app}\Palettes"; Flags: ignoreversion recursesubdirs

[Icons]
Name: "{group}\Decodium 3.0 FT2"; Filename: "{app}\{#MyAppExeName}"; IconFilename: "{app}\{#MyAppExeName}"
Name: "{group}\{cm:UninstallProgram,Decodium 3.0 FT2}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\Decodium 3.0 FT2 x86"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,Decodium 3.0 FT2}"; Flags: nowait postinstall skipifsilent
