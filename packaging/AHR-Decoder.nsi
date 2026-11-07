Unicode true
!include "MUI2.nsh"
!include "x64.nsh"
!ifndef STAGE
  !error "Pass /DSTAGE=... to makensis"
!endif
!ifndef OUTPUT
  !error "Pass /DOUTPUT=... to makensis"
!endif
!ifndef DLLHASH
  !error "Pass /DDLLHASH=... to makensis"
!endif
!ifndef BUILDCOMMIT
  !error "Pass /DBUILDCOMMIT=... to makensis"
!endif
!define PRODUCT "AHR-Decoder"
!ifndef APPVERSION
  !error "Pass /DAPPVERSION=... from packaging/VERSION.txt"
!endif
!define KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\AHR-Decoder"
Name "AHR-Decoder ${APPVERSION} — WWV/WWVH time decoder for SDRangel 8.0.0"
OutFile "${OUTPUT}"
InstallDir "$PROGRAMFILES64\SDRangel"
InstallDirRegKey HKLM "${KEY}" "HostDir"
RequestExecutionLevel admin
SetCompressor /SOLID lzma
ShowInstDetails show
ShowUnInstDetails show
!define MUI_ABORTWARNING
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "${STAGE}\LICENSE"
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE "English"

Function .onInit
  SetRegView 64
  FileOpen $R9 "$TEMP\\AHR-Decoder-NSIS-Trace.log" w
  FileWrite $R9 "AHR-Decoder setup init (before version/preflight) $\r$\n"
  FileClose $R9
  ${IfNot} ${RunningX64}
    MessageBox MB_ICONSTOP "AHR-Decoder requires Windows x64." /SD IDOK
    Abort
  ${EndIf}
FunctionEnd

Section "AHR-Decoder plugin" SecPlugin
  FileOpen $R9 "$TEMP\\AHR-Decoder-NSIS-Trace.log" a
  FileWrite $R9 "Entering install section: $INSTDIR $\r$\n"
  FileClose $R9
  InitPluginsDir
  FileOpen $R9 "$TEMP\\AHR-Decoder-NSIS-Trace.log" a
  FileWrite $R9 "Plugin temp initialized: $PLUGINSDIR $\r$\n"
  FileClose $R9
  SetOutPath "$PLUGINSDIR"
  File /oname=ahrwwvtimedecoder.dll "${STAGE}\ahrwwvtimedecoder.dll"
  File /oname=approved-host.txt "${STAGE}\approved-host.txt"
  File /oname=install-plugin.ps1 "${STAGE}\install-plugin.ps1"
  FileOpen $R9 "$TEMP\\AHR-Decoder-NSIS-Trace.log" a
  FileWrite $R9 "Payload files extracted; invoking host compatibility check $\r$\n"
  FileClose $R9
  DetailPrint "Checking approved official SDRangel 8.0.0 host and plugin SHA-256..."
  nsExec::ExecToStack 'powershell.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File "$PLUGINSDIR\install-plugin.ps1" -Action Install -HostFolder "$INSTDIR" -PayloadDll "$PLUGINSDIR\ahrwwvtimedecoder.dll" -ApprovedHostHash "$PLUGINSDIR\approved-host.txt" -PayloadHash "${DLLHASH}" -Version "${APPVERSION}" -BuildCommit "${BUILDCOMMIT}" -BuildRun "42320214210"'
  Pop $0
  Pop $1
  FileOpen $R9 "$TEMP\\AHR-Decoder-NSIS-Trace.log" a
  FileWrite $R9 "PowerShell host check returned: $0; output: $1 $\r$\n"
  FileClose $R9
  StrCmp $0 "0" safe
  DetailPrint "$1"
  MessageBox MB_ICONSTOP "AHR-Decoder did not install. SDRangel may be open, unsupported, or contain an unrecognized plugin. Details: $1" /SD IDOK
  Abort
safe:
  CreateDirectory "$PROGRAMFILES64\AHR-Decoder"
  SetOutPath "$PROGRAMFILES64\AHR-Decoder"
  File /oname=install-plugin.ps1 "${STAGE}\install-plugin.ps1"
  File /oname=LICENSE "${STAGE}\LICENSE"
  WriteUninstaller "$PROGRAMFILES64\AHR-Decoder\Uninstall.exe"
  WriteRegStr HKLM "${KEY}" "DisplayName" "AHR-Decoder ${APPVERSION} (SDRangel 8.0.0)"
  WriteRegStr HKLM "${KEY}" "DisplayVersion" "${APPVERSION}"
  WriteRegStr HKLM "${KEY}" "Publisher" "Alaska HamRadio"
  WriteRegStr HKLM "${KEY}" "URLInfoAbout" "https://github.com/KL5NE/AHR-WWV-Decoder"
  WriteRegStr HKLM "${KEY}" "InstallLocation" "$INSTDIR"
  WriteRegStr HKLM "${KEY}" "HostDir" "$INSTDIR"
  WriteRegStr HKLM "${KEY}" "UninstallString" '"$PROGRAMFILES64\AHR-Decoder\Uninstall.exe"'
  WriteRegDWORD HKLM "${KEY}" "NoModify" 1
  WriteRegDWORD HKLM "${KEY}" "NoRepair" 1
  DetailPrint "Installed plugin only. No SDRangel or runtime files replaced."
SectionEnd

Function un.onInit
  SetRegView 64
  ReadRegStr $INSTDIR HKLM "${KEY}" "HostDir"
  StrCmp $INSTDIR "" 0 +3
    MessageBox MB_ICONSTOP "No registered AHR-Decoder install path; uninstall is blocked." /SD IDOK
    SetErrorLevel 1
    Abort
FunctionEnd

Section "Uninstall"
  FileOpen $R9 "$TEMP\\AHR-Decoder-Uninstall-Trace.log" w
  FileWrite $R9 "Uninstall section entered; host=$INSTDIR $\r$\n"
  FileClose $R9
  nsExec::ExecToStack 'powershell.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File "$PROGRAMFILES64\AHR-Decoder\install-plugin.ps1" -Action Uninstall -HostFolder "$INSTDIR"'
  Pop $0
  Pop $1
  FileOpen $R9 "$TEMP\\AHR-Decoder-Uninstall-Trace.log" a
  FileWrite $R9 "Helper exit=$0; output=$1 $\r$\n"
  FileClose $R9
  StrCmp $0 "0" unSafe
  DetailPrint "$1"
  MessageBox MB_ICONSTOP "Uninstall blocked to protect an unrecognized or modified DLL. Details: $1" /SD IDOK
  SetErrorLevel 1
  Abort
unSafe:
  ; A successful helper exit is not sufficient: verify both removal postconditions
  ; before deleting registration or the uninstaller itself.
  IfFileExists "$INSTDIR\plugins\ahrwwvtimedecoder.dll" 0 +4
    DetailPrint "Uninstall blocked: decoder DLL still exists after helper returned success."
    SetErrorLevel 1
    Abort
  IfFileExists "$COMMONAPPDATA\AHR-Decoder\installed.json" 0 +4
    DetailPrint "Uninstall blocked: ownership record still exists after helper returned success."
    SetErrorLevel 1
    Abort
  DeleteRegKey HKLM "${KEY}"
  Delete "$PROGRAMFILES64\AHR-Decoder\install-plugin.ps1"
  Delete "$PROGRAMFILES64\AHR-Decoder\LICENSE"
  Delete "$PROGRAMFILES64\AHR-Decoder\Uninstall.exe"
  RMDir "$PROGRAMFILES64\AHR-Decoder"
  DetailPrint "Removed only the AHR-owned decoder DLL. SDRangel remains unchanged."
SectionEnd
