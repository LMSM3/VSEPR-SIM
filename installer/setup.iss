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
; Architecture:
;   File associations are owned by a single canonical script:
;     installer\register-file-associations.ps1
;   This installer packages that script and runs it post-install.
;   The [Registry] section is intentionally empty — the PS script
;   writes all HKCU entries so no admin rights are needed.
;
;   Universal file opener (all VSIM/XYZ types):
;     installer\bin\open_vsim_file.cmd
;   Priority: vsepr-sim.exe -> vsepr.exe -> pythonw vsepr_xyz_popup.pyw
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
Name: "fileassoc";    Description: "Register file associations (.vsim, .xyz, .xyza, .xyzc, .xyzf, .xyzfull, .vsxyz)"; GroupDescription: "System Integration:"; Flags: unchecked

[Files]
; --- Kernel executables ---
Source: "build\{#MyAppExeName}";       DestDir: "{app}\bin"; Flags: ignoreversion
Source: "build\vsepr-sim.exe";         DestDir: "{app}\bin"; Flags: ignoreversion skipifsourcedoesntexist
Source: "build\vsepr-cli.exe";         DestDir: "{app}\bin"; Flags: ignoreversion
Source: "build\vsepr_batch.exe";       DestDir: "{app}\bin"; Flags: ignoreversion

; --- Universal file opener + Python popup viewer ---
; open_vsim_file.cmd: priority-ordered handler for all VSIM/XYZ types
;   1. vsepr-sim.exe open (3-D viewer)  2. vsepr.exe open (CLI)  3. pythonw popup
Source: "tools\vsepr_xyz_popup.pyw";   DestDir: "{app}\bin"; Flags: ignoreversion
Source: "installer\bin\open_vsim_file.cmd"; DestDir: "{app}\bin"; Flags: ignoreversion

; --- File association script (canonical registry writer — HKCU, no admin) ---
Source: "installer\register-file-associations.ps1"; DestDir: "{app}\installer"; Flags: ignoreversion

; --- VSIM parser + runtime headers (SDK) ---
Source: "include\vsim\*"; DestDir: "{app}\include\vsim"; Flags: ignoreversion recursesubdirs createallsubdirs

; --- Data ---
Source: "data\*"; DestDir: "{app}\data"; Flags: ignoreversion recursesubdirs createallsubdirs

; --- Scripts ---
Source: "scripts\*"; DestDir: "{app}\scripts"; Flags: ignoreversion recursesubdirs createallsubdirs

; --- Documentation ---
Source: "README.md";         DestDir: "{app}"; Flags: ignoreversion isreadme
Source: "LICENSE";           DestDir: "{app}"; Flags: ignoreversion
Source: "VSIM_REFERENCE.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "docs\*";            DestDir: "{app}\docs"; Flags: ignoreversion recursesubdirs createallsubdirs

; --- Resources ---
Source: "resources\vsepr.ico"; DestDir: "{app}\resources"; Flags: ignoreversion

[Icons]
Name: "{group}\{#MyAppName}";                              Filename: "{app}\bin\{#MyAppExeName}"; WorkingDir: "{app}"; Comment: "{#MyAppDescription}"
Name: "{group}\{#MyAppName} CLI";                         Filename: "{app}\bin\vsepr-cli.exe";   WorkingDir: "{app}"; Comment: "VSIM command-line interface"
Name: "{group}\{cm:UninstallProgram,{#MyAppName}}";       Filename: "{uninstallexe}"
Name: "{autodesktop}\{#MyAppName}";                       Filename: "{app}\bin\{#MyAppExeName}"; WorkingDir: "{app}"; Tasks: desktopicon; Comment: "{#MyAppDescription}"

[Run]
Filename: "{app}\bin\{#MyAppExeName}"; Parameters: "--version"; Description: "Verify installation (--version)"; Flags: postinstall nowait skipifsilent unchecked
; Run the canonical association script post-install (HKCU, no admin needed)
Filename: "powershell.exe"; Parameters: "-ExecutionPolicy Bypass -File ""{app}\installer\register-file-associations.ps1"" -BinaryPath ""{app}\bin\vsepr.exe"""; Description: "Register file associations (.vsim, .xyz, .xyza, .xyzc, .xyzf, .xyzfull, .vsxyz)"; Flags: postinstall nowait skipifsilent unchecked; Tasks: fileassoc

[UninstallRun]
Filename: "powershell.exe"; Parameters: "-ExecutionPolicy Bypass -File ""{app}\installer\register-file-associations.ps1"" -Unregister"; Flags: nowait

; [Registry] block intentionally empty.
; All file-type registry entries are written by register-file-associations.ps1
; under HKCU — no admin rights required, clean uninstall guaranteed.
[Registry]

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
