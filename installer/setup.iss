; ============================================================================
; VSEPR-SIM Windows Installer Script (Inno Setup)
; ============================================================================
;
; Prerequisites:
;   - Download and install Inno Setup 6+: https://jrsoftware.org/isinfo.php
;   - Build the project in Release mode (cmake --build build --config Release)
;   - Ensure resources\vsepr.ico exists
;
; Build Installer:
;   iscc installer\setup.iss
;
; What this installs:
;   - VSEPR-SIM kernel (vsepr.exe, vsepr-cli.exe, vsepr_batch.exe)
;   - VSIM parser + runtime headers (include\vsim\*)
;   - XYZ popup viewer (tools\vsepr_xyz_popup.pyw + open_xyz.cmd wrapper)
;   - .xyz and .vsxyz file associations → popup 3-D coordinate viewer
;   - Optional PATH entry for CLI use
;
; ============================================================================

#define MyAppName "VSEPR-SIM"
#define MyAppVersion "5.0.0"
#define MyAppPublisher "LMSM3"
#define MyAppURL "https://github.com/LMSM3/VSEPR-SIM"
#define MyAppExeName "vsepr.exe"
#define MyAppDescription "Atomistic simulation platform — VSEPR-SIM v5"

[Setup]
AppId={{8B5C2D3E-9F4A-4E2B-B8C1-7A6D5E3F2C1B}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppVerName={#MyAppName} {#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}/issues
AppUpdatesURL={#MyAppURL}/releases
AppComments={#MyAppDescription}

DefaultDirName={autopf}\{#MyAppName}
DefaultGroupName={#MyAppName}
DisableProgramGroupPage=yes

OutputDir=installer\output
OutputBaseFilename=vsepr-sim-{#MyAppVersion}-setup
SetupIconFile=resources\vsepr.ico
UninstallDisplayIcon={app}\bin\{#MyAppExeName}

Compression=lzma2/ultra64
SolidCompression=yes
LZMAUseSeparateProcess=yes
LZMANumBlockThreads=4

ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible

PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog

WizardStyle=modern

LicenseFile=LICENSE

VersionInfoVersion={#MyAppVersion}
VersionInfoCompany={#MyAppPublisher}
VersionInfoDescription={#MyAppDescription}
VersionInfoCopyright=Copyright (C) 2026 {#MyAppPublisher}
VersionInfoProductName={#MyAppName}
VersionInfoProductVersion={#MyAppVersion}

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon";  Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked
Name: "addtopath";    Description: "Add {#MyAppName} bin to PATH"; GroupDescription: "System Integration:"; Flags: unchecked
Name: "assoc_xyz";    Description: "Associate .xyz files with VSEPR-SIM viewer"; GroupDescription: "File Associations:"; Flags: unchecked
Name: "assoc_vsxyz";  Description: "Associate .vsxyz files with VSEPR-SIM viewer"; GroupDescription: "File Associations:"; Flags: unchecked
Name: "assoc_vsim";   Description: "Associate .vsim scripts with VSEPR-SIM"; GroupDescription: "File Associations:"; Flags: unchecked

[Files]
; --- Kernel executables ---
Source: "build\{#MyAppExeName}";      DestDir: "{app}\bin"; Flags: ignoreversion
Source: "build\vsepr-cli.exe";        DestDir: "{app}\bin"; Flags: ignoreversion
Source: "build\vsepr_batch.exe";      DestDir: "{app}\bin"; Flags: ignoreversion

; --- XYZ popup viewer ---
; The .pyw is launched via open_xyz.cmd which resolves pythonw.exe from PATH.
Source: "tools\vsepr_xyz_popup.pyw";  DestDir: "{app}\bin"; Flags: ignoreversion
Source: "installer\bin\open_xyz.cmd"; DestDir: "{app}\bin"; Flags: ignoreversion

; --- VSIM parser + runtime headers (for developers building against the SDK) ---
Source: "include\vsim\*"; DestDir: "{app}\include\vsim"; Flags: ignoreversion recursesubdirs createallsubdirs

; --- Data / registry files ---
Source: "data\*"; DestDir: "{app}\data"; Flags: ignoreversion recursesubdirs createallsubdirs

; --- Scripts ---
Source: "scripts\*"; DestDir: "{app}\scripts"; Flags: ignoreversion recursesubdirs createallsubdirs

; --- Documentation ---
Source: "README.md";      DestDir: "{app}"; Flags: ignoreversion isreadme
Source: "LICENSE";        DestDir: "{app}"; Flags: ignoreversion
Source: "VSIM_REFERENCE.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "docs\*";         DestDir: "{app}\docs"; Flags: ignoreversion recursesubdirs createallsubdirs

; --- Resources ---
Source: "resources\vsepr.ico"; DestDir: "{app}\resources"; Flags: ignoreversion

[Icons]
Name: "{group}\{#MyAppName}";                              Filename: "{app}\bin\{#MyAppExeName}"; WorkingDir: "{app}"; Comment: "{#MyAppDescription}"
Name: "{group}\{#MyAppName} CLI";                         Filename: "{app}\bin\vsepr-cli.exe";   WorkingDir: "{app}"; Comment: "VSIM command-line interface"
Name: "{group}\{cm:UninstallProgram,{#MyAppName}}";       Filename: "{uninstallexe}"
Name: "{autodesktop}\{#MyAppName}";                       Filename: "{app}\bin\{#MyAppExeName}"; WorkingDir: "{app}"; Tasks: desktopicon; Comment: "{#MyAppDescription}"

[Run]
Filename: "{app}\bin\{#MyAppExeName}"; Parameters: "--version"; Description: "Verify installation (--version)"; Flags: postinstall nowait skipifsilent unchecked

[Registry]
; --- .vsxyz file association (VSEPR native XYZ format) ---
Root: HKA; Subkey: "Software\Classes\.vsxyz";                        ValueType: string; ValueName: "";       ValueData: "VSEPRSim.VSXYZFile"; Flags: uninsdeletevalue; Tasks: assoc_vsxyz
Root: HKA; Subkey: "Software\Classes\VSEPRSim.VSXYZFile";            ValueType: string; ValueName: "";       ValueData: "VSEPR-SIM Coordinate File"; Flags: uninsdeletekey; Tasks: assoc_vsxyz
Root: HKA; Subkey: "Software\Classes\VSEPRSim.VSXYZFile\DefaultIcon"; ValueType: string; ValueName: "";      ValueData: "{app}\resources\vsepr.ico,0"; Tasks: assoc_vsxyz
Root: HKA; Subkey: "Software\Classes\VSEPRSim.VSXYZFile\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\bin\open_xyz.cmd"" ""%1"""; Tasks: assoc_vsxyz

; --- .xyz file association (plain XYZ, open with VSEPR-SIM viewer) ---
Root: HKA; Subkey: "Software\Classes\.xyz\OpenWithProgids";           ValueType: string; ValueName: "VSEPRSim.XYZFile"; ValueData: ""; Flags: uninsdeletevalue; Tasks: assoc_xyz
Root: HKA; Subkey: "Software\Classes\VSEPRSim.XYZFile";              ValueType: string; ValueName: "";       ValueData: "XYZ Molecule File"; Flags: uninsdeletekey; Tasks: assoc_xyz
Root: HKA; Subkey: "Software\Classes\VSEPRSim.XYZFile\DefaultIcon";  ValueType: string; ValueName: "";       ValueData: "{app}\resources\vsepr.ico,0"; Tasks: assoc_xyz
Root: HKA; Subkey: "Software\Classes\VSEPRSim.XYZFile\shell\open\command"; ValueType: string; ValueName: "";  ValueData: """{app}\bin\open_xyz.cmd"" ""%1"""; Tasks: assoc_xyz

; --- .vsim script association ---
Root: HKA; Subkey: "Software\Classes\.vsim";                         ValueType: string; ValueName: "";       ValueData: "VSEPRSim.VSIMScript"; Flags: uninsdeletevalue; Tasks: assoc_vsim
Root: HKA; Subkey: "Software\Classes\VSEPRSim.VSIMScript";           ValueType: string; ValueName: "";       ValueData: "VSEPR-SIM Script"; Flags: uninsdeletekey; Tasks: assoc_vsim
Root: HKA; Subkey: "Software\Classes\VSEPRSim.VSIMScript\DefaultIcon"; ValueType: string; ValueName: "";     ValueData: "{app}\resources\vsepr.ico,0"; Tasks: assoc_vsim
Root: HKA; Subkey: "Software\Classes\VSEPRSim.VSIMScript\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\bin\vsepr-cli.exe"" ""%1"""; Tasks: assoc_vsim

[Code]
const
  EnvironmentKey = 'SYSTEM\CurrentControlSet\Control\Session Manager\Environment';

// Broadcast WM_SETTINGCHANGE so shells pick up PATH immediately.
procedure BroadcastEnvChange();
var
  Dummy: DWORD;
begin
  SendMessageTimeout(HWND_BROADCAST, WM_SETTINGCHANGE, 0,
    PAnsiChar('Environment'), SMTO_ABORTIFHUNG, 2000, Dummy);
end;

procedure AddToPath();
var
  OldPath, NewPath, BinPath: string;
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
        BroadcastEnvChange();
      end;
    end;
  end;
end;

procedure RemoveFromPath();
var
  OldPath, NewPath, BinPath: string;
begin
  BinPath := ExpandConstant('{app}\bin');
  if RegQueryStringValue(HKEY_LOCAL_MACHINE, EnvironmentKey, 'Path', OldPath) then
  begin
    if Pos(BinPath, OldPath) > 0 then
    begin
      NewPath := OldPath;
      StringChangeEx(NewPath, ';' + BinPath, '', True);
      StringChangeEx(NewPath, BinPath + ';', '', True);
      StringChangeEx(NewPath, BinPath, '', True);
      if RegWriteStringValue(HKEY_LOCAL_MACHINE, EnvironmentKey, 'Path', NewPath) then
      begin
        Log('Removed from PATH: ' + BinPath);
        BroadcastEnvChange();
      end;
    end;
  end;
end;

procedure CurStepChanged(CurStep: TSetupStep);
begin
  if CurStep = ssPostInstall then
  begin
    if IsTaskSelected('addtopath') then
      AddToPath();
  end;
end;

procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
begin
  if CurUninstallStep = usPostUninstall then
    RemoveFromPath();
end;

function InitializeSetup(): Boolean;
begin
  Result := True;
  if not IsWin64() then
  begin
    MsgBox('VSEPR-SIM v5 requires a 64-bit version of Windows.', mbError, MB_OK);
    Result := False;
  end;
end;
