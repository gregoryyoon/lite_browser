Unicode true
!include "MUI2.nsh"

Name "Lite Browser"
OutFile "LiteBrowserInstaller.exe"
InstallDir "$PROGRAMFILES64\LiteBrowser"

; Request application privileges for Windows Vista and higher
RequestExecutionLevel admin

; Interface Settings
!define MUI_ABORTWARNING

; Pages
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_WELCOME
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_UNPAGE_FINISH

; Languages
!insertmacro MUI_LANGUAGE "English"
!insertmacro MUI_LANGUAGE "Korean"

Section "Install"
  SetOutPath "$INSTDIR"
  
  ; Executables and DLLs
  File "c:\projects\lite_browser\cef_binary_149.0.6\build\tests\cefsimple_capi\Release\cefsimple_capi.exe"
  File /nonfatal "c:\projects\lite_browser\cef_binary_149.0.6\build\tests\cefsimple_capi\Release\cefsimple_capi.dll"
  File "c:\projects\lite_browser\cef_binary_149.0.6\build\tests\cefsimple_capi\Release\libcef.dll"
  File "c:\projects\lite_browser\cef_binary_149.0.6\build\tests\cefsimple_capi\Release\chrome_elf.dll"
  File "c:\projects\lite_browser\cef_binary_149.0.6\build\tests\cefsimple_capi\Release\d3dcompiler_47.dll"
  File "c:\projects\lite_browser\cef_binary_149.0.6\build\tests\cefsimple_capi\Release\dxcompiler.dll"
  File "c:\projects\lite_browser\cef_binary_149.0.6\build\tests\cefsimple_capi\Release\dxil.dll"
  File "c:\projects\lite_browser\cef_binary_149.0.6\build\tests\cefsimple_capi\Release\icudtl.dat"
  File "c:\projects\lite_browser\cef_binary_149.0.6\build\tests\cefsimple_capi\Release\libEGL.dll"
  File "c:\projects\lite_browser\cef_binary_149.0.6\build\tests\cefsimple_capi\Release\libGLESv2.dll"
  File "c:\projects\lite_browser\cef_binary_149.0.6\build\tests\cefsimple_capi\Release\v8_context_snapshot.bin"
  File "c:\projects\lite_browser\cef_binary_149.0.6\build\tests\cefsimple_capi\Release\vk_swiftshader.dll"
  File "c:\projects\lite_browser\cef_binary_149.0.6\build\tests\cefsimple_capi\Release\vk_swiftshader_icd.json"
  File "c:\projects\lite_browser\cef_binary_149.0.6\build\tests\cefsimple_capi\Release\vulkan-1.dll"
  
  ; Resource files
  File "c:\projects\lite_browser\cef_binary_149.0.6\build\tests\cefsimple_capi\Release\chrome_100_percent.pak"
  File "c:\projects\lite_browser\cef_binary_149.0.6\build\tests\cefsimple_capi\Release\chrome_200_percent.pak"
  File "c:\projects\lite_browser\cef_binary_149.0.6\build\tests\cefsimple_capi\Release\resources.pak"
  
  ; Locales folder
  SetOutPath "$INSTDIR\locales"
  File "c:\projects\lite_browser\cef_binary_149.0.6\build\tests\cefsimple_capi\Release\locales\*.*"
  
  ; UI folder (Main UI & MD Editor)
  SetOutPath "$INSTDIR\ui"
  File "c:\projects\lite_browser\ui\index.html"
  File "c:\projects\lite_browser\ui\style.css"
  File "c:\projects\lite_browser\ui\app.js"
  File "c:\projects\lite_browser\ui\editor.html"
  
  SetOutPath "$INSTDIR"
  WriteUninstaller "$INSTDIR\uninstall.exe"
  
  ; Shortcuts
  CreateShortcut "$SMPROGRAMS\Lite Browser.lnk" "$INSTDIR\cefsimple_capi.exe"
  CreateShortcut "$DESKTOP\Lite Browser.lnk" "$INSTDIR\cefsimple_capi.exe"
  
  ; Add uninstall information to Add/Remove Programs
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\LiteBrowser" \
                  "DisplayName" "Lite Browser"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\LiteBrowser" \
                  "UninstallString" "$\"$INSTDIR\uninstall.exe$\""
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\LiteBrowser" \
                  "QuietUninstallString" "$\"$INSTDIR\uninstall.exe$\" /S"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\LiteBrowser" \
                  "InstallLocation" "$\"$INSTDIR$\""
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\LiteBrowser" \
                  "Publisher" "Lite Browser Developer"
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\LiteBrowser" \
                  "NoModify" 1
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\LiteBrowser" \
                  "NoRepair" 1
SectionEnd

Section "Uninstall"
  Delete "$DESKTOP\Lite Browser.lnk"
  Delete "$SMPROGRAMS\Lite Browser.lnk"
  
  RMDir /r "$INSTDIR\locales"
  RMDir /r "$INSTDIR\ui"
  
  Delete "$INSTDIR\cefsimple_capi.exe"
  Delete "$INSTDIR\cefsimple_capi.dll"
  Delete "$INSTDIR\libcef.dll"
  Delete "$INSTDIR\chrome_elf.dll"
  Delete "$INSTDIR\d3dcompiler_47.dll"
  Delete "$INSTDIR\dxcompiler.dll"
  Delete "$INSTDIR\dxil.dll"
  Delete "$INSTDIR\icudtl.dat"
  Delete "$INSTDIR\libEGL.dll"
  Delete "$INSTDIR\libGLESv2.dll"
  Delete "$INSTDIR\v8_context_snapshot.bin"
  Delete "$INSTDIR\vk_swiftshader.dll"
  Delete "$INSTDIR\vk_swiftshader_icd.json"
  Delete "$INSTDIR\vulkan-1.dll"
  Delete "$INSTDIR\chrome_100_percent.pak"
  Delete "$INSTDIR\chrome_200_percent.pak"
  Delete "$INSTDIR\resources.pak"
  Delete "$INSTDIR\uninstall.exe"
  
  RMDir "$INSTDIR"
  
  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\LiteBrowser"
SectionEnd
