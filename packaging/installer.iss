; RX Media Player - Inno Setup 6 64-bit Installer Script
; High-performance Windows multimedia application

#define MyAppName "RX Media Player"
#define MyAppVersion "2.4.0"
#define MyAppPublisher "Rituraj"
#define MyAppURL "https://github.com/jcksprwx/RX-Video-Player"
#define MyAppExeName "rx_player.exe"

[Setup]
AppId={{5E28B02A-8B6F-42E1-A873-6A103FE8372A}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
AppUpdatesURL={#MyAppURL}
DefaultDirName={autopf64}\{#MyAppName}
DefaultGroupName={#MyAppName}
AllowNoIcons=yes
OutputDir=..\dist_installer
OutputBaseFilename=RX_Media_Player_Setup_x64
Compression=lzma2/ultra64
SolidCompression=yes
SetupIconFile=..\assets\icons\app_icon.ico
WizardStyle=modern
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
DisableProgramGroupPage=auto
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
UninstallDisplayIcon={app}\{#MyAppExeName}

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked
Name: "associate_video"; Description: "Associate with common video formats (.mp4, .mkv, .avi, .mov, .webm)"; GroupDescription: "File Associations:"; Flags: unchecked
Name: "associate_audio"; Description: "Associate with common audio formats (.mp3, .flac, .wav, .aac, .ogg)"; GroupDescription: "File Associations:"; Flags: unchecked

[Files]
Source: "..\dist\{#MyAppExeName}"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\dist\libmpv-2.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\dist\mpv-2.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\assets\icons\*"; DestDir: "{app}\assets\icons"; Flags: ignoreversion recursesubdirs createallsubdirs; Tasks: desktopicon

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{group}\{cm:UninstallProgram,{#MyAppName}}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Registry]
; Video associations
Root: HKA; Subkey: "Software\Classes\.mp4\OpenWithProgids"; ValueType: string; ValueName: "RXMediaPlayer.Assoc"; Flags: uninsdeletevalue; Tasks: associate_video
Root: HKA; Subkey: "Software\Classes\.mkv\OpenWithProgids"; ValueType: string; ValueName: "RXMediaPlayer.Assoc"; Flags: uninsdeletevalue; Tasks: associate_video
Root: HKA; Subkey: "Software\Classes\.avi\OpenWithProgids"; ValueType: string; ValueName: "RXMediaPlayer.Assoc"; Flags: uninsdeletevalue; Tasks: associate_video
Root: HKA; Subkey: "Software\Classes\.mov\OpenWithProgids"; ValueType: string; ValueName: "RXMediaPlayer.Assoc"; Flags: uninsdeletevalue; Tasks: associate_video
Root: HKA; Subkey: "Software\Classes\.webm\OpenWithProgids"; ValueType: string; ValueName: "RXMediaPlayer.Assoc"; Flags: uninsdeletevalue; Tasks: associate_video

; Audio associations
Root: HKA; Subkey: "Software\Classes\.mp3\OpenWithProgids"; ValueType: string; ValueName: "RXMediaPlayer.Assoc"; Flags: uninsdeletevalue; Tasks: associate_audio
Root: HKA; Subkey: "Software\Classes\.flac\OpenWithProgids"; ValueType: string; ValueName: "RXMediaPlayer.Assoc"; Flags: uninsdeletevalue; Tasks: associate_audio
Root: HKA; Subkey: "Software\Classes\.wav\OpenWithProgids"; ValueType: string; ValueName: "RXMediaPlayer.Assoc"; Flags: uninsdeletevalue; Tasks: associate_audio

; Application registration
Root: HKA; Subkey: "Software\Classes\RXMediaPlayer.Assoc"; ValueType: string; ValueName: ""; ValueData: "RX Media Player Media File"; Flags: uninsdeletekey
Root: HKA; Subkey: "Software\Classes\RXMediaPlayer.Assoc\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\{#MyAppExeName},0"
Root: HKA; Subkey: "Software\Classes\RXMediaPlayer.Assoc\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#StringChange(MyAppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent
