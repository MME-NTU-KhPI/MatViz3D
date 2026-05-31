#define MyAppName "MatViz3D"
#define MyAppVersion "2.01"
#define MyAppPublisher "MME-NTU-KhPI"
#define MyAppExeName "MatViz3D.exe"

[Setup]
AppId={{B8E5E0F2-3E4E-4C50-B9D0-7E6A51A7C123}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
DefaultDirName={autopf}\{#MyAppName}
DefaultGroupName={#MyAppName}
OutputDir=packaging\windows\Output
OutputBaseFilename=MatViz3D-Setup
Compression=lzma
SolidCompression=yes
WizardStyle=modern
SetupIconFile="{app}\Plugin icon - 1icon.ico"
UninstallDisplayIcon={app}\{#MyAppExeName}

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut"; GroupDescription: "Additional icons:"

[Files]
Source: "dist\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "Launch {#MyAppName}"; Flags: nowait postinstall skipifsilent
