; Memoria by DSK - Inno Setup installer. Build: iscc /DAppVersion=1.0.0 /DArtDir=<artefacts\Release> installer\memoria.iss
#ifndef AppVersion
  #define AppVersion "0.0.0"
#endif
#ifndef ArtDir
  #define ArtDir "..\build\Memoria_artefacts\Release"
#endif

[Setup]
AppId={{56C3AF5F-46F0-490B-9B9C-5E8534604D35}
AppName=Memoria by DSK
AppVersion={#AppVersion}
AppPublisher=DSK Music
AppPublisherURL=https://dskmusic.com
DefaultDirName={autopf}\DSK Music\Memoria
DefaultGroupName=DSK Music
OutputBaseFilename=Memoria-by-DSK-win-x64-setup
OutputDir=..\dist
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
Compression=lzma2
SolidCompression=yes
LicenseFile=..\LICENSE
DisableProgramGroupPage=yes
SetupIconFile=..\branding\memoria.ico
UninstallDisplayIcon={app}\Memoria by DSK.exe

[Components]
Name: "vst3"; Description: "VST3 plugin"; Types: full compact custom; Flags: fixed
Name: "standalone"; Description: "Standalone application"; Types: full

[Files]
Source: "{#ArtDir}\VST3\Memoria by DSK.vst3\*"; DestDir: "{commoncf64}\VST3\Memoria by DSK.vst3"; Components: vst3; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#ArtDir}\Standalone\Memoria by DSK.exe"; DestDir: "{app}"; Components: standalone; Flags: ignoreversion

[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut"; Components: standalone; Flags: unchecked

[Icons]
Name: "{autodesktop}\Memoria by DSK"; Filename: "{app}\Memoria by DSK.exe"; Components: standalone; Tasks: desktopicon; IconFilename: "{app}\Memoria by DSK.exe"
Name: "{group}\Memoria by DSK"; Filename: "{app}\Memoria by DSK.exe"; Components: standalone
