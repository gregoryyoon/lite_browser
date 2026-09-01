---
name: cef-upgrade
description: >-
  Step-by-step runbook for upgrading the Chromium Embedded Framework (CEF) binary distribution in LiteBrowser,
  migrating custom C API sources, generating CMake solution, compiling Debug/Release targets, and packaging NSIS installer.
  Use this skill whenever the user asks to upgrade CEF version, update Chromium binary distribution, or re-package LiteBrowser for a new CEF release.
---

# CEF Binary Version Upgrade Runbook for LiteBrowser

This runbook guides the seamless upgrade of the CEF binary distribution in LiteBrowser from `c:\projects\cef-latest\cef_binary` to a dedicated workspace folder `cef_binary_<version>`.

## Upgrade Workflow Steps

### Step 1: Copy New CEF Binary Distribution
Copy the raw CEF binary distribution from `c:\projects\cef-latest\cef_binary` to the new versioned directory `c:\projects\lite_browser\cef_binary_<new_version>`:
```powershell
robocopy "c:\projects\cef-latest\cef_binary" "c:\projects\lite_browser\cef_binary_<new_version>" /E /NFL /NDL /NJH /NJS
if ($LASTEXITCODE -le 7) { exit 0 } else { exit $LASTEXITCODE }
```

### Step 2: Migrate LiteBrowser Custom C API Sources & Assets
Copy all custom C implementation files, headers, CMake build configuration, and icon injection resources from the previous CEF directory:
```powershell
robocopy "c:\projects\lite_browser\cef_binary_<old_version>\tests\cefsimple_capi" "c:\projects\lite_browser\cef_binary_<new_version>\tests\cefsimple_capi" /E /NFL /NDL /NJH /NJS
if ($LASTEXITCODE -le 7) { exit 0 } else { exit $LASTEXITCODE }
```
**Key Custom Files to Ensure Migrated:**
- Core: `cefsimple_win.c`, `simple_app.c`, `simple_app.h`, `simple_handler.c`, `simple_handler.h`, `simple_life_span_handler.c`, `simple_load_handler.c`, `simple_display_handler.c`, `browser_context.h`
- Subsystems: `simple_auth.c`, `simple_auth.h`, `simple_download_handler.c`, `simple_download_handler.h`, `simple_vault.c`, `simple_vault.h`
- Build Config: `CMakeLists.txt` (`lite_browser` output target, `crypt32.lib`, `/experimental:c11atomics` flag)
- Resources: `win/cefsimple.ico`, `win/inject_icon.py`, `win/cefsimple.rc`, `win/cefsimple.exe.manifest`

### Step 3: Configure CMake with Visual Studio 18 2026
In the newly created `cef_binary_<new_version>` directory:
```powershell
cd c:\projects\lite_browser\cef_binary_<new_version>
if (Test-Path build) { Remove-Item -Recurse -Force build }
cmake -B build -G "Visual Studio 18 2026" -A x64
```

### Step 4: Build Debug & Release Configurations
1. **Debug Build & C API Signature Verification**:
   ```powershell
   cmake --build build --config Debug --target cefsimple_capi
   ```
   *Verify exit code 0. If CEF C API signatures changed in the new release, adjust function pointer mappings.*
2. **Release Build**:
   ```powershell
   cmake --build build --config Release --target cefsimple_capi
   ```
   *Verify exit code 0 and generation of `build/tests/cefsimple_capi/Release/lite_browser.exe`.*

### Step 5: Update Packaging & Project Configurations
1. **`.gitignore`**: Add ignore patterns for the new `cef_binary_<new_version>` build/binary directories:
   ```gitignore
   cef_binary_<new_version>/build/
   cef_binary_<new_version>/Debug/
   cef_binary_<new_version>/Release/
   cef_binary_<new_version>/Resources/
   cef_binary_<new_version>/include/
   cef_binary_<new_version>/libcef_dll/
   cef_binary_<new_version>/tests/cefclient/
   cef_binary_<new_version>/tests/cefsimple/
   cef_binary_<new_version>/tests/ceftests/
   cef_binary_<new_version>/tests/gmock/
   cef_binary_<new_version>/tests/gtest/
   cef_binary_<new_version>/tests/shared/
   cef_binary_<new_version>/bazel/
   cef_binary_<new_version>/*.lib
   cef_binary_<new_version>/*.dll
   cef_binary_<new_version>/*.exe
   cef_binary_<new_version>/*.pak
   cef_binary_<new_version>/*.bin
   cef_binary_<new_version>/*.dat
   cef_binary_<new_version>/locales/
   cef_binary_<new_version>/swiftshader/
   ```
2. **`installer.nsi`**: Update source path prefixes from `<old_version>` to `<new_version>` and compile the installer:
   ```powershell
   & "C:\Program Files (x86)\NSIS\makensis.exe" c:\projects\lite_browser\installer.nsi
   ```
   *Verify generation of `LiteBrowserInstaller.exe`.*
3. **Documentation**: Update `docs/prompt.md` and append release notes in `docs/walkthrough.md`.
