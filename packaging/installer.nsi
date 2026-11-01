; RX Media Player - NSIS 64-bit Modern Installer Script

!define PRODUCT_NAME "RX Media Player"
!define PRODUCT_VERSION "2.4.1"
!define PRODUCT_PUBLISHER "RX Media Player"
!define PRODUCT_DIR_REGKEY "Software\Microsoft\Windows\CurrentVersion\App Paths\rx_player.exe"
!define PRODUCT_UNINST_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NAME}"

SetCompressor /SOLID lzma

!include "MUI2.nsh"

; MUI Settings
!define MUI_ABORTWARNING
!define MUI_ICON "..\assets\icons\app_icon.ico"
!define MUI_UNICON "..\assets\icons\app_icon.ico"

; Pages
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!define MUI_FINISHPAGE_RUN "$INSTDIR\rx_player.exe"
!insertmacro MUI_PAGE_FINISH

; Uninstaller Pages
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_LANGUAGE "English"

Name "${PRODUCT_NAME} ${PRODUCT_VERSION}"
OutFile "..\dist_installer\RX_Media_Player_Setup_2.4.1.exe"
InstallDir "$PROGRAMFILES64\RX Media Player"
InstallDirRegKey HKLM "${PRODUCT_DIR_REGKEY}" ""
ShowInstDetails show
ShowUnInstDetails show

Section "MainSection" SEC01
  SetOutPath "$INSTDIR"
  SetOverwrite ifnewer
  File "..\dist\rx_player.exe"
  File "..\dist\libmpv-2.dll"
  File "..\dist\mpv-2.dll"

  CreateDirectory "$SMPROGRAMS\RX Media Player"
  CreateShortcut "$SMPROGRAMS\RX Media Player\RX Media Player.lnk" "$INSTDIR\rx_player.exe"
  CreateShortcut "$DESKTOP\RX Media Player.lnk" "$INSTDIR\rx_player.exe"
SectionEnd

Section -AdditionalIcons
  WriteIniStr "$INSTDIR\${PRODUCT_NAME}.url" "InternetShortcut" "URL" "https://github.com/rxmediaplayer"
  CreateShortcut "$SMPROGRAMS\RX Media Player\Website.lnk" "$INSTDIR\${PRODUCT_NAME}.url"
  CreateShortcut "$SMPROGRAMS\RX Media Player\Uninstall.lnk" "$INSTDIR\Uninstall.exe"
SectionEnd

Section -Post
  WriteUninstaller "$INSTDIR\Uninstall.exe"
  WriteRegStr HKLM "${PRODUCT_DIR_REGKEY}" "" "$INSTDIR\rx_player.exe"
  WriteRegStr HKLM "${PRODUCT_UNINST_KEY}" "DisplayName" "$(^Name)"
  WriteRegStr HKLM "${PRODUCT_UNINST_KEY}" "UninstallString" "$INSTDIR\Uninstall.exe"
  WriteRegStr HKLM "${PRODUCT_UNINST_KEY}" "DisplayIcon" "$INSTDIR\rx_player.exe"
  WriteRegStr HKLM "${PRODUCT_UNINST_KEY}" "DisplayVersion" "${PRODUCT_VERSION}"
  WriteRegStr HKLM "${PRODUCT_UNINST_KEY}" "Publisher" "${PRODUCT_PUBLISHER}"
SectionEnd

Section Uninstall
  Delete "$DESKTOP\RX Media Player.lnk"
  Delete "$SMPROGRAMS\RX Media Player\Uninstall.lnk"
  Delete "$SMPROGRAMS\RX Media Player\Website.lnk"
  Delete "$SMPROGRAMS\RX Media Player\RX Media Player.lnk"
  RMDir "$SMPROGRAMS\RX Media Player"

  Delete "$INSTDIR\${PRODUCT_NAME}.url"
  Delete "$INSTDIR\Uninstall.exe"
  Delete "$INSTDIR\mpv-2.dll"
  Delete "$INSTDIR\libmpv-2.dll"
  Delete "$INSTDIR\rx_player.exe"
  RMDir "$INSTDIR"

  DeleteRegKey HKLM "${PRODUCT_UNINST_KEY}"
  DeleteRegKey HKLM "${PRODUCT_DIR_REGKEY}"
  SetAutoClose true
SectionEnd
