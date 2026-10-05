; Stable per-user identity: do not change AppId between upgrades.
#ifndef PayloadDir
  #error PayloadDir must point to a verified payload from build-installer.ps1
#endif
#ifndef OutputPath
  #error OutputPath is required
#endif
#ifndef AppVersion
  #define AppVersion "0.45.1"
#endif

[Setup]
AppId=FEC-RTTY-M0NXD
AppName=FEC-RTTY - M0NXD
AppVersion={#AppVersion}
AppPublisher=M0NXD
AppVerName=FEC-RTTY - M0NXD {#AppVersion}
DefaultDirName={localappdata}\Programs\FEC-RTTY
DefaultGroupName=FEC-RTTY - M0NXD
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
ArchitecturesAllowed=x64os
ArchitecturesInstallIn64BitMode=x64os
MinVersion=10.0.17763
OutputDir={#OutputPath}
OutputBaseFilename=FEC-RTTY-{#AppVersion}-Setup-x64
VersionInfoVersion={#AppVersion}.2
VersionInfoDescription=FEC-RTTY - M0NXD offline installer
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
WizardSizePercent=110
SetupLogging=yes
Uninstallable=yes
CreateUninstallRegKey=yes
UninstallDisplayName=FEC-RTTY - M0NXD
UninstallDisplayIcon={app}\gui\fectty-gui.exe
CloseApplications=yes
RestartApplications=no
InfoBeforeFile=resources\BEFORE_INSTALL.txt

[Tasks]
Name: desktopicon; Description: "Create a desktop shortcut"; GroupDescription: "Shortcuts:"; Flags: unchecked

[Files]
Source: "{#PayloadDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\FEC-RTTY - M0NXD"; Filename: "{app}\gui\fectty-gui.exe"; WorkingDir: "{app}\gui"; AppUserModelID: "M0NXD.FEC-RTTY"
Name: "{group}\Getting started and documentation"; Filename: "{app}\index.html"
Name: "{group}\Uninstall FEC-RTTY"; Filename: "{uninstallexe}"
Name: "{autodesktop}\FEC-RTTY - M0NXD"; Filename: "{app}\gui\fectty-gui.exe"; WorkingDir: "{app}\gui"; Tasks: desktopicon; AppUserModelID: "M0NXD.FEC-RTTY"

[Run]
Filename: "{app}\gui\fectty-gui.exe"; WorkingDir: "{app}\gui"; Description: "Launch FEC-RTTY - M0NXD"; Flags: nowait postinstall skipifsilent
Filename: "{app}\index.html"; Description: "Read the getting-started guide"; Flags: shellexec postinstall skipifsilent unchecked

; No UninstallDelete or UninstallRun entries: remove only installed files,
; never recursively delete user settings/logs or uninstall a shared driver.
[Code]
function PrepareToInstall(var NeedsRestart: Boolean): String;
var
  ExistingInstall: String;
  Item: TFindRec;
  HasContents: Boolean;
begin
  Result := '';
  HasContents := False;
  if FindFirst(AddBackslash(ExpandConstant('{app}')) + '*', Item) then
  begin
    try
      repeat
        if (Item.Name <> '.') and (Item.Name <> '..') then
          HasContents := True;
      until not FindNext(Item);
    finally
      FindClose(Item);
    end;
  end;
  if HasContents then
  begin
    if not RegQueryStringValue(HKCU64,
      'Software\Microsoft\Windows\CurrentVersion\Uninstall\FEC-RTTY-M0NXD_is1',
      'InstallLocation', ExistingInstall) then
      ExistingInstall := '';
    if (CompareText(RemoveBackslashUnlessRoot(ExistingInstall),
        RemoveBackslashUnlessRoot(ExpandConstant('{app}'))) <> 0) or
       not FileExists(ExpandConstant('{app}\installation-manifest.json')) or
       not FileExists(ExpandConstant('{app}\unins000.exe')) then
      Result := 'Choose an empty installation folder. Setup will not overwrite ' +
        'an unrelated folder or a source checkout. Existing FEC-RTTY installations ' +
        'can be repaired in their registered installation folder.';
  end;
end;
