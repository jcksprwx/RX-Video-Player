; RX Media Player - NSIS 64-bit Modern Installer Script
; With full Windows "Open with" and Default Programs Registration

!define PRODUCT_NAME "RX Media Player"
!define PRODUCT_VERSION "2.7.0"
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
OutFile "..\dist_installer\RX_Media_Player_Setup_2.7.0.exe"
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
  WriteIniStr "$INSTDIR\${PRODUCT_NAME}.url" "InternetShortcut" "URL" "https://github.com/jcksprwx/RX-Video-Player"
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

  ; 1. Register Application: Software\Classes\Applications\rx_player.exe
  WriteRegStr HKLM "Software\Classes\Applications\rx_player.exe" "" "RX Media Player"
  WriteRegStr HKLM "Software\Classes\Applications\rx_player.exe" "FriendlyAppName" "RX Media Player"
  WriteRegStr HKLM "Software\Classes\Applications\rx_player.exe" "ApplicationCompany" "RX Media Player"
  WriteRegStr HKLM "Software\Classes\Applications\rx_player.exe\DefaultIcon" "" "$INSTDIR\rx_player.exe,0"
  WriteRegStr HKLM "Software\Classes\Applications\rx_player.exe\shell\open" "" "Play with &RX Media Player"
  WriteRegStr HKLM "Software\Classes\Applications\rx_player.exe\shell\open" "FriendlyAppName" "RX Media Player"
  WriteRegStr HKLM "Software\Classes\Applications\rx_player.exe\shell\open\command" "" '"$INSTDIR\rx_player.exe" "%1"'

  ; SupportedTypes under Applications\rx_player.exe
  WriteRegStr HKLM "Software\Classes\Applications\rx_player.exe\SupportedTypes" ".mkv" ""
  WriteRegStr HKLM "Software\Classes\Applications\rx_player.exe\SupportedTypes" ".mp4" ""
  WriteRegStr HKLM "Software\Classes\Applications\rx_player.exe\SupportedTypes" ".avi" ""
  WriteRegStr HKLM "Software\Classes\Applications\rx_player.exe\SupportedTypes" ".mov" ""
  WriteRegStr HKLM "Software\Classes\Applications\rx_player.exe\SupportedTypes" ".wmv" ""
  WriteRegStr HKLM "Software\Classes\Applications\rx_player.exe\SupportedTypes" ".webm" ""
  WriteRegStr HKLM "Software\Classes\Applications\rx_player.exe\SupportedTypes" ".m4v" ""
  WriteRegStr HKLM "Software\Classes\Applications\rx_player.exe\SupportedTypes" ".mpeg" ""
  WriteRegStr HKLM "Software\Classes\Applications\rx_player.exe\SupportedTypes" ".mpg" ""
  WriteRegStr HKLM "Software\Classes\Applications\rx_player.exe\SupportedTypes" ".ts" ""
  WriteRegStr HKLM "Software\Classes\Applications\rx_player.exe\SupportedTypes" ".m2ts" ""
  WriteRegStr HKLM "Software\Classes\Applications\rx_player.exe\SupportedTypes" ".flv" ""
  WriteRegStr HKLM "Software\Classes\Applications\rx_player.exe\SupportedTypes" ".mp3" ""
  WriteRegStr HKLM "Software\Classes\Applications\rx_player.exe\SupportedTypes" ".wav" ""
  WriteRegStr HKLM "Software\Classes\Applications\rx_player.exe\SupportedTypes" ".flac" ""
  WriteRegStr HKLM "Software\Classes\Applications\rx_player.exe\SupportedTypes" ".aac" ""
  WriteRegStr HKLM "Software\Classes\Applications\rx_player.exe\SupportedTypes" ".m4a" ""
  WriteRegStr HKLM "Software\Classes\Applications\rx_player.exe\SupportedTypes" ".ogg" ""
  WriteRegStr HKLM "Software\Classes\Applications\rx_player.exe\SupportedTypes" ".opus" ""
  WriteRegStr HKLM "Software\Classes\Applications\rx_player.exe\SupportedTypes" ".wma" ""

  ; 2. Register ProgID: Software\Classes\RXMediaPlayer.AssocFile
  WriteRegStr HKLM "Software\Classes\RXMediaPlayer.AssocFile" "" "RX Media Player Media File"
  WriteRegStr HKLM "Software\Classes\RXMediaPlayer.AssocFile" "FriendlyTypeName" "RX Media Player Media File"
  WriteRegStr HKLM "Software\Classes\RXMediaPlayer.AssocFile\DefaultIcon" "" "$INSTDIR\rx_player.exe,0"
  WriteRegStr HKLM "Software\Classes\RXMediaPlayer.AssocFile\shell\open" "" "Play with &RX Media Player"
  WriteRegStr HKLM "Software\Classes\RXMediaPlayer.AssocFile\shell\open\command" "" '"$INSTDIR\rx_player.exe" "%1"'

  ; 3. Register OpenWithProgids & OpenWithList for every supported format
  !macro RegisterExt EXT
    WriteRegStr HKLM "Software\Classes\${EXT}\OpenWithProgids" "RXMediaPlayer.AssocFile" ""
    WriteRegStr HKLM "Software\Classes\${EXT}\OpenWithList\rx_player.exe" "" ""
  !macroend

  !insertmacro RegisterExt ".mkv"
  !insertmacro RegisterExt ".mp4"
  !insertmacro RegisterExt ".avi"
  !insertmacro RegisterExt ".mov"
  !insertmacro RegisterExt ".wmv"
  !insertmacro RegisterExt ".webm"
  !insertmacro RegisterExt ".m4v"
  !insertmacro RegisterExt ".mpeg"
  !insertmacro RegisterExt ".mpg"
  !insertmacro RegisterExt ".ts"
  !insertmacro RegisterExt ".m2ts"
  !insertmacro RegisterExt ".flv"
  !insertmacro RegisterExt ".mp3"
  !insertmacro RegisterExt ".wav"
  !insertmacro RegisterExt ".flac"
  !insertmacro RegisterExt ".aac"
  !insertmacro RegisterExt ".m4a"
  !insertmacro RegisterExt ".ogg"
  !insertmacro RegisterExt ".opus"
  !insertmacro RegisterExt ".wma"

  ; 4. Register Capabilities for Windows Default Apps / RegisteredApplications
  WriteRegStr HKLM "Software\RX Media Player\Capabilities" "ApplicationName" "RX Media Player"
  WriteRegStr HKLM "Software\RX Media Player\Capabilities" "ApplicationDescription" "RX Media Player - Native 64-bit Multimedia Player"
  WriteRegStr HKLM "Software\RX Media Player\Capabilities" "ApplicationIcon" "$INSTDIR\rx_player.exe,0"

  !macro RegisterCapExt EXT
    WriteRegStr HKLM "Software\RX Media Player\Capabilities\FileAssociations" "${EXT}" "RXMediaPlayer.AssocFile"
  !macroend

  !insertmacro RegisterCapExt ".mkv"
  !insertmacro RegisterCapExt ".mp4"
  !insertmacro RegisterCapExt ".avi"
  !insertmacro RegisterCapExt ".mov"
  !insertmacro RegisterCapExt ".wmv"
  !insertmacro RegisterCapExt ".webm"
  !insertmacro RegisterCapExt ".m4v"
  !insertmacro RegisterCapExt ".mpeg"
  !insertmacro RegisterCapExt ".mpg"
  !insertmacro RegisterCapExt ".ts"
  !insertmacro RegisterCapExt ".m2ts"
  !insertmacro RegisterCapExt ".flv"
  !insertmacro RegisterCapExt ".mp3"
  !insertmacro RegisterCapExt ".wav"
  !insertmacro RegisterCapExt ".flac"
  !insertmacro RegisterCapExt ".aac"
  !insertmacro RegisterCapExt ".m4a"
  !insertmacro RegisterCapExt ".ogg"
  !insertmacro RegisterCapExt ".opus"
  !insertmacro RegisterCapExt ".wma"

  WriteRegStr HKLM "Software\RegisteredApplications" "RX Media Player" "Software\RX Media Player\Capabilities"

  ; 5. Notify Windows Shell of association change
  System::Call 'shell32.dll::SHChangeNotify(i 0x08000000, i 0, p 0, p 0)'
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

  ; Unregister OpenWithProgids & OpenWithList
  !macro UnregisterExt EXT
    DeleteRegValue HKLM "Software\Classes\${EXT}\OpenWithProgids" "RXMediaPlayer.AssocFile"
    DeleteRegKey HKLM "Software\Classes\${EXT}\OpenWithList\rx_player.exe"
  !macroend

  !insertmacro UnregisterExt ".mkv"
  !insertmacro UnregisterExt ".mp4"
  !insertmacro UnregisterExt ".avi"
  !insertmacro UnregisterExt ".mov"
  !insertmacro UnregisterExt ".wmv"
  !insertmacro UnregisterExt ".webm"
  !insertmacro UnregisterExt ".m4v"
  !insertmacro UnregisterExt ".mpeg"
  !insertmacro UnregisterExt ".mpg"
  !insertmacro UnregisterExt ".ts"
  !insertmacro UnregisterExt ".m2ts"
  !insertmacro UnregisterExt ".flv"
  !insertmacro UnregisterExt ".mp3"
  !insertmacro UnregisterExt ".wav"
  !insertmacro UnregisterExt ".flac"
  !insertmacro UnregisterExt ".aac"
  !insertmacro UnregisterExt ".m4a"
  !insertmacro UnregisterExt ".ogg"
  !insertmacro UnregisterExt ".opus"
  !insertmacro UnregisterExt ".wma"

  DeleteRegKey HKLM "Software\Classes\Applications\rx_player.exe"
  DeleteRegKey HKLM "Software\Classes\RXMediaPlayer.AssocFile"
  DeleteRegKey HKLM "Software\RX Media Player\Capabilities"
  DeleteRegValue HKLM "Software\RegisteredApplications" "RX Media Player"

  DeleteRegKey HKLM "${PRODUCT_UNINST_KEY}"
  DeleteRegKey HKLM "${PRODUCT_DIR_REGKEY}"

  System::Call 'shell32.dll::SHChangeNotify(i 0x08000000, i 0, p 0, p 0)'
  SetAutoClose true
SectionEnd
