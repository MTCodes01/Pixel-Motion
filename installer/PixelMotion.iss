#define MyAppName "Pixel Motion"
#define MyAppVersion "1.3.7"
#define MyAppPublisher "Pixel Motion Project"
#define MyAppURL "https://github.com/MTCodes01/Pixel-Motion"
#define MyAppExeName "PixelMotion.exe"

[Setup]
; Unique AppId for Pixel Motion
AppId={{8B1D4C2F-9E5A-4B6C-8F7E-3D9A1B2C3D4E}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
AppUpdatesURL={#MyAppURL}/releases
DefaultDirName={autopf}\{#MyAppName}
DefaultGroupName={#MyAppName}
AllowNoIcons=yes
; Preserve previous settings and directories across updates without prompting
UsePreviousAppDir=yes
UsePreviousTasks=yes
UsePreviousPrivileges=yes
; PrivilegesRequired=lowest allows smooth install in user space without UAC prompts
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
OutputDir=..\build\bin
OutputBaseFilename=PixelMotion-Setup
SetupIconFile=..\src\ui\PixelMotion.ico
Compression=lzma2/ultra64
SolidCompression=yes
WizardStyle=modern
DisableProgramGroupPage=yes
ArchitecturesInstallIn64BitMode=x64compatible
ArchitecturesAllowed=x64compatible
CloseApplications=force
RestartApplications=no
UninstallDisplayIcon={app}\{#MyAppExeName}
VersionInfoCompany={#MyAppPublisher}
VersionInfoDescription=Pixel Motion Installer
VersionInfoVersion={#MyAppVersion}.0
VersionInfoProductName={#MyAppName}
VersionInfoProductVersion={#MyAppVersion}
VersionInfoCopyright=Copyright (C) 2026 Pixel Motion Project

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked
Name: "autostart"; Description: "Start Pixel Motion automatically when Windows starts"; GroupDescription: "Startup:"; Flags: unchecked

[Files]
Source: "..\build\bin\Release\PixelMotion.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\build\bin\Release\logo.png"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\build\bin\Release\avcodec-62.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\build\bin\Release\avformat-62.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\build\bin\Release\avutil-60.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\build\bin\Release\swresample-6.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\build\bin\Release\swscale-9.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\build\bin\Release\shaders\*"; DestDir: "{app}\shaders"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "..\cert\PixelMotion.cer"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{autoprograms}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; IconFilename: "{app}\{#MyAppExeName}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; IconFilename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Registry]
Root: HKCU; Subkey: "Software\Microsoft\Windows\CurrentVersion\Run"; ValueType: string; ValueName: "PixelMotion"; ValueData: """{app}\{#MyAppExeName}"""; Flags: uninsdeletevalue; Tasks: autostart
Root: HKCU; Subkey: "Software\Microsoft\Windows\CurrentVersion\Explorer\StartupApproved\Run"; ValueType: binary; ValueName: "PixelMotion"; ValueData: "02 00 00 00 00 00 00 00 00 00 00 00"; Flags: uninsdeletevalue; Tasks: autostart

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#StringChange(MyAppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent; Check: not WizardSilent
Filename: "{app}\{#MyAppExeName}"; WorkingDir: "{app}"; Flags: nowait runasoriginaluser; Check: WizardSilent

[Code]
var
  IsUpgradeInstall: Boolean;

function InitializeSetup(): Boolean;
var
  UninstallKey: String;
begin
  Result := True;
  UninstallKey := 'Software\Microsoft\Windows\CurrentVersion\Uninstall\{#SetupSetting("AppId")}_is1';
  IsUpgradeInstall := RegKeyExists(HKCU, UninstallKey) or RegKeyExists(HKLM, UninstallKey);
end;

function ShouldSkipPage(PageID: Integer): Boolean;
begin
  // When upgrading an existing installation, automatically preserve options and skip wizard setup pages
  if IsUpgradeInstall then
  begin
    if (PageID = wpSelectDir) or (PageID = wpSelectProgramGroup) or (PageID = wpSelectTasks) or (PageID = wpReady) then
      Result := True
    else
      Result := False;
  end
  else
    Result := False;
end;

procedure CurStepChanged(CurStep: TSetupStep);
var
  AppDir: String;
  ResultCode: Integer;
begin
  if CurStep = ssPostInstall then
  begin
    AppDir := ExpandConstant('{app}');
    // Strip Zone.Identifier (Mark of the Web) so Windows Defender SmartScreen never flags installed binaries
    Exec('powershell.exe', '-NoProfile -NonInteractive -WindowStyle Hidden -Command "Get-ChildItem -Path ''' + AppDir + ''' -Recurse | Unblock-File -ErrorAction SilentlyContinue"', '', SW_HIDE, ewWaitUntilTerminated, ResultCode);
  end;
end;
