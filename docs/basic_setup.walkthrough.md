# LiteBrowser 기본 언어 설정 (OS Default Language Sync)

## 개요
LiteBrowser 실행 시 Windows OS의 사용자 기본 로캘(UI Language)을 자동 감지하여 CEF의 내장 UI 및 웹페이지 언어 설정에 동적으로 반영하는 기능을 구현했습니다.

---

## 주요 구현 내용

### 1. Windows OS 기본 언어 감지 (`GetUserDefaultLocaleName`)
- Win32 API `GetUserDefaultLocaleName`을 호출하여 사용자 시스템의 기본 로캘(예: `ko-KR`)을 추출합니다.
- 추출 실패 시 기본 fallback으로 `ko-KR`을 적용합니다.

### 2. CEF 내장 UI 언어 설정 (`settings.locale`)
- `settings.locale`에 OS 기본 로캘 문자열(예: `ko-KR`)을 설정합니다.
- 패스워드 매니저, 컨텍스트 메뉴, DevTools, 경고창, PDF 뷰어 등 모든 CEF 내장 UI의 표기 언어가 시스템 언어로 자동 설정됩니다.

### 3. 웹 요청 및 JS 객체 언어 설정 (`settings.accept_language_list`)
- HTTP Request Header의 `Accept-Language` 및 JavaScript `navigator.language` / `navigator.languages`에 언어 우선순위 목록(예: `ko-KR,ko,en-US,en`)을 전달합니다.
- 대한항공(`https://www.koreanair.com/`) 등 다국어 지원 웹사이트에 최초 접속 시 한국어 메인 페이지가 자동으로 노출됩니다.

### 4. 메모리 관리
- CEF 브라우저 프로세스 초기화(`cef_initialize`) 직후 동적 생성된 CEF 문자열 리소스(`settings.locale`, `settings.accept_language_list`)를 `cef_string_clear`로 안전하게 해제합니다.

---

## 관련 소스 코드
- [`cef_binary_149.0.6/tests/cefsimple_capi/cefsimple_win.c`](file:///c:/projects/lite_browser/cef_binary_149.0.6/tests/cefsimple_capi/cefsimple_win.c)

---

## 검증 결과
1. **빌드 검증**: `cmake --build . --config Debug --target cefsimple_capi` 정상 빌드 완료.
2. **동작 검증**:
   - 대한항공(`https://www.koreanair.com/`) 접속 시 한국어 페이지 기본 노출.
   - DevTools 콘솔 `navigator.language` -> `"ko-KR"` 확인.
   - 내장 패스워드 매니저 및 팝업 UI 언어가 한국어로 표기됨을 확인.
