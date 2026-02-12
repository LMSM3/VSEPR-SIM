; ============================================================================
; VSEPR-Sim Windows Installer Script (Inno Setup)
; ============================================================================
; 
; Prerequisites:
;   - Download and install Inno Setup: https://jrsoftware.org/isinfo.php
;   - Build the project in Release mode
;   - Generate the icon (resources/vsepr.ico)
;
; Build Installer:
;   iscc installer\setup.iss
;
; ============================================================================

#define MyAppName "VSEPR-Sim"
#define MyAppVersion "2.0.0"
#define MyAppPublisher "VSEPR-Sim Project"
#define MyAppURL "https://github.com/yourusername/vsepr-sim"
#define MyAppExeName "vsepr.exe"
#define MyAppDescription "Molecular Geometry Simulator using VSEPR Theory"

[Setup]
; App Information
AppId={{8B5C2D3E-9F4A-4E2B-B8C1-7A6D5E3F2C1B}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppVerName={#MyAppName} {#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
AppUpdatesURL={#MyAppURL}
AppComments={#MyAppDescription}

; Installation Directories
DefaultDirName={autopf}\{#MyAppName}
DefaultGroupName={#MyAppName}
DisableProgramGroupPage=yes

; Output Configuration
OutputDir=installer\output
OutputBaseFilename=vsepr-sim-{#MyAppVersion}-setup
SetupIconFile=resources\vsepr.ico
UninstallDisplayIcon={app}\{#MyAppExeName}

; Compression
Compression=lzma2/ultra64
SolidCompression=yes
LZMAUseSeparateProcess=yes
LZMANumBlockThreads=4

; Architecture
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible

; Privileges
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog

; UI
WizardStyle=modern
WizardImageFile=resources\installer_banner.bmp
WizardSmallImageFile=resources\installer_icon.bmp

; License
LicenseFile=LICENSE

; Version Information
VersionInfoVersion={#MyAppVersion}
VersionInfoCompany={#MyAppPublisher}
VersionInfoDescription={#MyAppDescription}
VersionInfoCopyright=Copyright (C) 2026 {#MyAppPublisher}
VersionInfoProductName={#MyAppName}
VersionInfoProductVersion={#MyAppVersion}

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked
Name: "quicklaunchicon"; Description: "{cm:CreateQuickLaunchIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked; OnlyBelowVersion: 6.1; Check: not IsAdminInstallMode
Name: "addtopath"; Description: "Add to PATH environment variable"; GroupDescription: "System Integration:"; Flags: unchecked

[Files]
; Main Executable
Source: "build\bin\{#MyAppExeName}"; DestDir: "{app}\bin"; Flags: ignoreversion

; Additional Executables
Source: "build\bin\vsepr_batch.exe"; DestDir: "{app}\bin"; Flags: ignoreversion
Source: "build\bin\md_demo.exe"; DestDir: "{app}\bin"; Flags: ignoreversion

; Data Files
Source: "data\*"; DestDir: "{app}\data"; Flags: ignoreversion recursesubdirs createallsubdirs

; Documentation
Source: "README.md"; DestDir: "{app}"; Flags: ignoreversion isreadme
Source: "LICENSE"; DestDir: "{app}"; Flags: ignoreversion
Source: "CHANGELOG.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "docs\*"; DestDir: "{app}\docs"; Flags: ignoreversion recursesubdirs createallsubdirs

; Examples
Source: "*.xyz"; DestDir: "{app}\examples"; Flags: ignoreversion

; Launcher Script
Source: "vsepr.bat"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
; Start Menu
Name: "{group}\{#MyAppName}"; Filename: "{app}\bin\{#MyAppExeName}"; WorkingDir: "{app}"; Comment: "{#MyAppDescription}"
Name: "{group}\{#MyAppName} Documentation"; Filename: "{app}\docs\QUICKSTART.md"; Comment: "Getting Started Guide"
Name: "{group}\{cm:UninstallProgram,{#MyAppName}}"; Filename: "{uninstallexe}"

; Desktop Icon (optional)
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\bin\{#MyAppExeName}"; WorkingDir: "{app}"; Tasks: desktopicon; Comment: "{#MyAppDescription}"

; Quick Launch (optional)
Name: "{userappdata}\Microsoft\Internet Explorer\Quick Launch\{#MyAppName}"; Filename: "{app}\bin\{#MyAppExeName}"; Tasks: quicklaunchicon; WorkingDir: "{app}"

[Run]
Filename: "{app}\docs\QUICKSTART.md"; Description: "{cm:LaunchProgram,View Quick Start Guide}"; Flags: postinstall shellexec skipifsilent unchecked
Filename: "{app}\bin\{#MyAppExeName}"; Parameters: "--version"; Description: "{cm:LaunchProgram,Test Installation}"; Flags: postinstall nowait skipifsilent unchecked

[Registry]
; File Association for .xyz files (optional)
Root: HKA; Subkey: "Software\Classes\.xyz\OpenWithProgids"; ValueType: string; ValueName: "VSEPRSim.XYZFile"; ValueData: ""; Flags: uninsdeletevalue
Root: HKA; Subkey: "Software\Classes\VSEPRSim.XYZFile"; ValueType: string; ValueName: ""; ValueData: "XYZ Molecule File"; Flags: uninsdeletekey
Root: HKA; Subkey: "Software\Classes\VSEPRSim.XYZFile\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\bin\{#MyAppExeName},0"
Root: HKA; Subkey: "Software\Classes\VSEPRSim.XYZFile\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\bin\{#MyAppExeName}"" ""%1"""

[Code]
const
  EnvironmentKey = 'SYSTEM\CurrentControlSet\Control\Session Manager\Environment';

procedure AddToPath();
var
  OldPath: string;
  NewPath: string;
  BinPath: string;
begin
  BinPath := ExpandConstant('{app}\bin');
  
  if RegQueryStringValue(HKEY_LOCAL_MACHINE, EnvironmentKey, 'Path', OldPath) then
  begin
    if Pos(BinPath, OldPath) = 0 then
    begin
      NewPath := OldPath + ';' + BinPath;
      if RegWriteStringValue(HKEY_LOCAL_MACHINE, EnvironmentKey, 'Path', NewPath) then
      begin
        Log('Added to PATH: ' + BinPath);
      end;
    end;
  end;
end;

procedure RemoveFromPath();
var
  OldPath: string;
  NewPath: string;
  BinPath: string;
  PathPos: Integer;
begin
  BinPath := ExpandConstant('{app}\bin');
  
  if RegQueryStringValue(HKEY_LOCAL_MACHINE, EnvironmentKey, 'Path', OldPath) then
  begin
    PathPos := Pos(BinPath, OldPath);
    if PathPos > 0 then
    begin
      NewPath := OldPath;
      StringChangeEx(NewPath, ';' + BinPath, '', True);
      StringChangeEx(NewPath, BinPath + ';', '', True);
      StringChangeEx(NewPath, BinPath, '', True);
      
      if RegWriteStringValue(HKEY_LOCAL_MACHINE, EnvironmentKey, 'Path', NewPath) then
      begin
        Log('Removed from PATH: ' + BinPath);
      end;
    end;
  end;
end;

procedure CurStepChanged(CurStep: TSetupStep);
begin
  if CurStep = ssPostInstall then
  begin
    if IsTaskSelected('addtopath') then
    begin
      AddToPath();
    end;
  end;
end;

procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
begin
  if CurUninstallStep = usPostUninstall then
  begin
    RemoveFromPath();
  end;
end;

function InitializeSetup(): Boolean;
var
  Version: TWindowsVersion;
begin
  Result := True;
  
  GetWindowsVersionEx(Version);
  
  // Require Windows 10 or later
  if Version.Major < 10 then
  begin
    MsgBox('This application requires Windows 10 or later.', mbError, MB_OK);
    Result := False;
  end;
end;
