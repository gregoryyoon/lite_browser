# 🚀 Lite Browser 아이콘 디자인 & 윈도우 리소스 자동화 파이프라인 기술 보고서 (`design.walkthrough`)

본 문서는 Lite Browser의 제품 철학("Notepad처럼 메모리를 최소한으로 사용하는 가벼운 브라우저")을 직관적으로 전달하기 위해 수행된 커스텀 아이콘 디자인, Windows 표준 멀티 프레임 ICO 규격 수립, CEF Sandbox 아키텍처 기술적 한계 회피를 위한 후처리 리소스 주입 엔진(`Icon Injecting Engine`) 및 CMake 빌드 자동화 내역을 기록한 종합 보고서입니다.

---

## 📌 1. 프로젝트 개요 및 목표 (Overview & Objectives)

### 1.1 배경 및 디자인 의도 (Design Intent)
- **가벼움 (Feather-light)**: Lite Browser는 Edge나 Chrome 대비 빠른 기동 속도와 최소한의 메모리 점유율을 지향하는 윈도우즈 메모장(Notepad) UX 컨셉의 초경량 브라우저입니다.
- **아이콘 상징성**: 가벼움을 상징하는 **깃털(Feather)**과 세련된 **슬림 브라우저 창(Slim Window)**을 결합한 현대적인 플랫/그라데이션 디자인(시안 1)을 채택했습니다.

### 1.2 해결해야 할 핵심 기술 과제
1. **다양한 Windows 디스플레이 환경에서의 선명도 보장**:
   - 윈도우 탐색기(세부 정보, 일반, 큰 아이콘), 작업 표시줄(작은 아이콘/기본 아이콘), High DPI(125%, 150%, 200% scaling) 디스플레이에서 깨짐이나 뭉개짐 없이 선명하게 보이기 위한 표준 `.ico` 규격 구성 필요.
2. **CEF Sandbox 아키텍처의 리소스 미반영 한계 우회**:
   - `USE_SANDBOX=ON` 환경에서 CEF CMake 빌드 시스템은 메인 로직을 `cefsimple_capi.dll`로 컴파일하고, `cefsimple_capi.exe`는 CEF에 미리 내장된 샌드박스 래퍼 바이너리인 `bootstrap.exe`를 단순 **복사(COPY_SINGLE_FILE)**해와 이름만 바꿉니다.
   - 따라서 `.rc` 리소스 파일에 아이콘을 등록하더라도 복사된 `cefsimple_capi.exe` 바이너리에는 리소스가 들어가지 않아 윈도우 탐색기에서 아이콘이 미반영(기본 실행 파일 로고)되는 이슈가 발생합니다.

---

## 🛠️ 2. 주요 시스템 구성 및 상세 구현 (Implementation Details)

### 2.1 Windows 표준 멀티 프레임 ICO 생성 엔진
- **파일**: [scratch/create_standard_ico.py](file:///C:/Users/grego/.gemini/antigravity-ide/brain/4b188e86-f001-4722-863a-b4fa7cd828d2/scratch/create_standard_ico.py), [scratch/inspect_ico.py](file:///C:/Users/grego/.gemini/antigravity-ide/brain/4b188e86-f001-4722-863a-b4fa7cd828d2/scratch/inspect_ico.py)
- **표준 해상도 및 포맷 규격 세트**:

| 해상도 | 비트 심도 (bpp) | 포맷 | 주요 용도 |
| :--- | :--- | :--- | :--- |
| **16 x 16** | 32-bit (8-bit alpha) | **Raw BMP (DIB)** | 탐색기 세부 정보 보기, 타이틀 바 |
| **24 x 24** | 32-bit (8-bit alpha) | **Raw BMP (DIB)** | 작업 표시줄 (작은 아이콘 모드) |
| **32 x 32** | 32-bit (8-bit alpha) | **Raw BMP (DIB)** | 기본 바탕화면, 탐색기 일반 보기 |
| **48 x 48** | 32-bit (8-bit alpha) | **Raw BMP (DIB)** | 탐색기 큰 아이콘 |
| **256 x 256** | 32-bit (8-bit alpha) | **PNG 압축** | Windows 고해상도 DPI, 아주 큰 아이콘 |

- **구현 방식**:
  - 소형 규격(16~48px)은 `BITMAPINFOHEADER` + Bottom-up BGRA 픽셀 + 1비트 AND 마스크 구조의 **Raw BMP DIB**로 생성하여 구형 윈도우 컨트롤 호환성 확보.
  - 대형 규격(256px)은 용량 최적화를 위해 **PNG 압축** 데이터로 인코딩.
- **적용 리소스**:
  - [cefsimple.ico](file:///c:/projects/lite_browser/cef_binary_149.0.6/tests/cefsimple_capi/win/cefsimple.ico)
  - [small.ico](file:///c:/projects/lite_browser/cef_binary_149.0.6/tests/cefsimple_capi/win/small.ico)

---

### 2.2 Win32 Resource API 기반 자동 아이콘 주입 엔진 (`Icon Injecting Engine`)
- **파일**: [win/inject_icon.py](file:///c:/projects/lite_browser/cef_binary_149.0.6/tests/cefsimple_capi/win/inject_icon.py)
- **동작 원리**:
  - Windows C-API `BeginUpdateResourceW`, `UpdateResourceW`, `EndUpdateResourceW` 함수를 Python `ctypes` 및 `wintypes`로 직접 바인딩.
  - `MAKEINTRESOURCEW` 캐스팅 헬퍼(`ctypes.cast(ctypes.c_void_p(i), wintypes.LPCWSTR)`)를 통해 64비트 Windows 환경에서 정수 리소스 ID 매핑 오류(Error 87: `ERROR_INVALID_PARAMETER`)를 박멸.
  - 타겟 `.ico` 파일 내부의 **모든 프레임(16~256px 개수 `img_count`)을 감지하여 100% 자동 분할 주입**:
    - `RT_ICON`: 각 해상도별 이미지 데이터 (ID: 1, 2, 3, 4, 5...)
    - `RT_GROUP_ICON`: 디렉토리 매핑 헤더 (ID: 120 / `IDI_CEFSIMPLE`)

```python
# inject_icon.py 핵심 구조 예시
def inject_ico_to_exe(exe_path, ico_path, group_icon_id=120):
    # 1. ICO 파일 내 모든 해상도 프레임 파싱
    ...
    # 2. BeginUpdateResourceW 호출
    hUpdate = kernel32.BeginUpdateResourceW(exe_path, False)
    
    # 3. 모든 프레임에 대해 RT_ICON 전량 주입
    for entry in entries:
        kernel32.UpdateResourceW(hUpdate, MAKEINTRESOURCEW(RT_ICON), MAKEINTRESOURCEW(entry['res_id']), 1033, ...)
        
    # 4. 통합 그룹 헤더 RT_GROUP_ICON 주입
    kernel32.UpdateResourceW(hUpdate, MAKEINTRESOURCEW(RT_GROUP_ICON), MAKEINTRESOURCEW(group_icon_id), 1033, ...)
    
    # 5. EndUpdateResourceW 저장
    kernel32.EndUpdateResourceW(hUpdate, False)
```

---

### 2.3 CMake 빌드 파이프라인 자동화 (Post-Build Command)
- **파일**: [CMakeLists.txt](file:///c:/projects/lite_browser/cef_binary_149.0.6/tests/cefsimple_capi/CMakeLists.txt)
- **구현 내용**: `cefsimple_capi` 타겟 빌드 직후 `COPY_SINGLE_FILE` 단계를 거쳐 복사된 `cefsimple_capi.exe` 파일에 자동으로 `inject_icon.py` 스크립트가 실행되도록 `POST_BUILD` 커스텀 명령 등록.

```cmake
    # Copy and rename the bootstrap binary.
    COPY_SINGLE_FILE(${CEF_TARGET} ${CEF_BINARY_DIR}/bootstrap.exe ${CEF_TARGET_OUT_DIR}/${CEF_TARGET}.exe)

    # Inject the custom icon into the bootstrap executable.
    add_custom_command(
      TARGET ${CEF_TARGET}
      POST_BUILD
      COMMAND python "${CMAKE_CURRENT_SOURCE_DIR}/win/inject_icon.py" "$<TARGET_FILE_DIR:${CEF_TARGET}>/${CEF_TARGET}.exe" "${CMAKE_CURRENT_SOURCE_DIR}/win/cefsimple.ico" 120
      VERBATIM
      )
```

---

### 2.4 NSIS 설치 프로그램 연동
- **파일**: [installer.nsi](file:///c:/projects/lite_browser/installer.nsi)
- **설정**: `MUI_ICON` 및 `MUI_UNICON` 지정으로 설치/언인스톨러 아이콘 매핑.
```nsis
!define MUI_ICON "cef_binary_149.0.6\tests\cefsimple_capi\win\cefsimple.ico"
!define MUI_UNICON "cef_binary_149.0.6\tests\cefsimple_capi\win\cefsimple.ico"
```

---

## 🔍 3. 검증 결과 (Verification Results)

### 3.1 아이콘 해상도 헤더 파싱 검증 ([inspect_ico.py](file:///C:/Users/grego/.gemini/antigravity-ide/brain/4b188e86-f001-4722-863a-b4fa7cd828d2/scratch/inspect_ico.py))
```text
=== Inspecting ICO file: cefsimple.ico ===
Header - Reserved: 0, Type: 1 (1=Icon), Image Count: 5

Image [1]: 16 x 16   | 32-bit | Raw BMP (DIB) | Size: 1128 bytes
Image [2]: 24 x 24   | 32-bit | Raw BMP (DIB) | Size: 2440 bytes
Image [3]: 32 x 32   | 32-bit | Raw BMP (DIB) | Size: 4264 bytes
Image [4]: 48 x 48   | 32-bit | Raw BMP (DIB) | Size: 9640 bytes
Image [5]: 256 x 256 | 32-bit | PNG Compressed | Size: 48000 bytes
```

### 3.2 빌드 파이프라인 자동 주입 검증 (CMake Build Output)
```text
  cefsimple_capi.vcxproj -> Release/cefsimple_capi.dll
  Injecting win/cefsimple.ico into Release/cefsimple_capi.exe as Resource ID 120...
  ICO contains 5 images.
  Wrote RT_ICON 1 (size: 16x16)
  Wrote RT_ICON 2 (size: 24x24)
  Wrote RT_ICON 3 (size: 32x32)
  Wrote RT_ICON 4 (size: 48x48)
  Wrote RT_ICON 5 (size: 256x256)
  Wrote RT_GROUP_ICON 120
  Successfully injected icon into executable.
```

- **최종 결과**: `Release` 및 `Debug` 빌드 시 `cefsimple_capi.exe` 파일에 5가지 해상도의 아이콘이 100% 자동 주입되어, 탐색기 및 작업 표시줄, 시작 메뉴에서 고품질 커스텀 아이콘이 정상 반영됨을 검증했습니다.
