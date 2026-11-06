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
  ${IfNot} ${RunningX64}
    MessageBox MB_ICONSTOP "AHR-Decoder requires Windows x64."
    Abort
  ${EndIf}
FunctionEnd

Section "AHR-Decoder plugin" SecPlugin
  InitPluginsDir
  SetOutPath "$PLUGINSDIR"
  File /oname=ahrwwvtimedecoder.dll "${STAGE}\ahrwwvtimedecoder.dll"
  File /oname=approved-host.txt "${STAGE}\approved-host.txt"
  File /oname=install-plugin.ps1 "${STAGE}\install-plugin.ps1"
  DetailPrint "Checking approved official SDRangel 8.0.0 host and plugin SHA-256..."
  nsExec::ExecToStack 'powershell.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File "$PLUGINSDIR\install-plugin.ps1" -Action Install -HostFolder "$INSTDIR" -PayloadDll "$PLUGINSDIR\ahrwwvtimedecoder.dll" -ApprovedHostHash "$PLUGINSDIR\approved-host.txt" -PayloadHash "${DLLHASH}" -Version "${APPVERSION}" -BuildCommit "${BUILDCOMMIT}" -BuildRun "42299074954"'
  Pop $0
  Pop $1
  StrCmp $0 "0" safe
  DetailPrint "$1"
  MessageBox MB_ICONSTOP "AHR-Decoder did not install. SDRangel may be open, unsupported, or contain an unrecognized plugin. Details: $1"
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
    MessageBox MB_ICONSTOP "No registered AHR-Decoder install path; uninstall is blocked."
    Abort
FunctionEnd

Section "Uninstall"
  nsExec::ExecToStack 'powershell.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File "$PROGRAMFILES64\AHR-Decoder\install-plugin.ps1" -Action Uninstall -HostFolder "$INSTDIR"'
  Pop $0
  Pop $1
  StrCmp $0 "0" unSafe
  DetailPrint "$1"
  MessageBox MB_ICONSTOP "Uninstall blocked to protect an unrecognized or modified DLL. Details: $1"
  Abort
unSafe:
  DeleteRegKey HKLM "${KEY}"
  Delete "$PROGRAMFILES64\AHR-Decoder\install-plugin.ps1"
  Delete "$PROGRAMFILES64\AHR-Decoder\LICENSE"
  Delete "$PROGRAMFILES64\AHR-Decoder\Uninstall.exe"
  RMDir "$PROGRAMFILES64\AHR-Decoder"
  DetailPrint "Removed only the AHR-owned decoder DLL. SDRangel remains unchanged."
SectionEnd
