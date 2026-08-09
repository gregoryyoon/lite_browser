# 🌐 Lite Browser Windows OS 기본 언어 자동 설정 (Locale & Accept-Language System) 개발 완료 보고서

본 문서는 Lite Browser에 구현된 Windows OS 기본 설정 언어 자동 감지, CEF Global Settings (`settings.locale`, `settings.accept_language_list`) 연동, `--lang` 커맨드라인 스위치 주입 및 내장 Chrome UI/웹페이지 언어 맞춤 기능에 대한 종합 개발 보고서입니다.

---

## 📌 1. 프로젝트 개요 및 목표 (Overview & Objectives)

### 1.1 기존 언어 설정의 문제점 (Pain Points)
1. **Chromium 기본값(`en-US`) 고착화**: CEF 초기화 시 `settings.locale` 및 `settings.accept_language_list`가 빈 값으로 설정되어 Chromium 기본 설정인 영어(`en-US`)로 작동함.
2. **웹사이트 접속 시 영어 페이지 자동 로딩**: 대한항공(`https://www.koreanair.com/`) 등 다국어 지원 웹사이트 접속 시 HTTP `Accept-Language` 헤더가 `en-US`로 전달되어 한국어 사용 환경임에도 페이지가 영어로 기본 제공되는 문제 발생.
3. **내장 Chrome UI 언어 불일치**: Chrome 패스워드 매니저, 인증 다이얼로그, 내부 팝업 및 `navigator.language` 자바스크립트 속성이 영어로 노출되는 현상.

### 1.2 차세대 언어 동기화 혁신 목표
- **OS 언어 100% 자동 감지**: Win32 API를 사용하여 현재 Windows OS의 기본 설정 언어(User Default Locale) 및 선호 UI 언어 목록(Preferred UI Languages)을 동적 탐색.
- **HTTP `Accept-Language` 자동 주입**: 웹페이지 접속 시 사용자 환경에 맞춘 헤더(`ko-KR,ko,en-US,en`)를 자동 전송하여 첫 접속 시에도 한글 페이지 로딩 지원.
- **내장 Chrome UI & 서브프로세스 언어 일치**: 패스워드 매니저, internal page 및 모든 CEF 렌더러 서브프로세스까지 OS 언어를 일관되게 주입.

---

## 🛠️ 2. 주요 시스템 구성 요소 및 상세 구현

### 2.1 OS 기본 언어 동적 감지 엔진 (`ConfigureSystemLocale`)
- **파일**: [cefsimple_win.c](file:///c:/projects/lite_browser/cef_binary_149.0.6/tests/cefsimple_capi/cefsimple_win.c)
- **동작 원리**:
  - `GetUserDefaultLocaleName`: Windows OS의 기본 UI Locale Name(예: `L"ko-KR"`)을 감지하여 `cef_string_wide_to_utf16`을 통해 `settings.locale`에 주입.
  - `GetUserPreferredUILanguages`: 사용자가 설정한 UI 선호 언어 목록(예: `L"ko-KR\0en-US\0\0"`)을 multi-string으로 읽어와 Primary Tag(예: `ko`, `en`)를 포함한 `Accept-Language` 콤마 구분 규격 문자열(`ko-KR,ko,en-US,en`)로 파싱 후 `settings.accept_language_list`에 주입.
- **메모리 안전 관리**:
  - `cef_initialize` 내부 복사가 완료된 직후 `cef_string_clear`를 호출하여 동적 할당된 CEF String 리소스를 안전하게 해제.

### 2.2 서브프로세스 렌더링 언어 스위치 주입
- **파일**: [simple_app.c](file:///c:/projects/lite_browser/cef_binary_149.0.6/tests/cefsimple_capi/simple_app.c)
- **`simple_app_on_before_command_line_processing`**:
  - 커맨드라인 인자에 `--lang` 스위치가 없을 경우 OS 기본 Locale(예: `ko-KR`)을 감지하여 `append_switch_with_value`로 자동 주입.
  - 렌더러(Render), GPU, 유틸리티 등 CEF의 모든 독립 서브프로세스 언어 설정 동기화 보장.

### 2.3 프로젝트 중앙 기술 문서 갱신
- **파일**: [docs/prompt.md](file:///c:/projects/lite_browser/docs/prompt.md)
- `Windows OS 기본 설정 언어 자동 감지 및 CEF Locale/Accept-Language 설정` 섹션을 추가하여 아키텍처 원인 분석 및 해결 방안 명시.

---

## 🧪 3. 검증 및 결과 (Verification & Results)

1. **자동화 빌드 검증**:
   - Visual Studio 2022/2026 + CMake (`cmake --build . --config Debug`) 빌드 결과 C 컴파일 및 링크 에러 0건, 정상 executable 생성 확인.
2. **웹 요청 및 UI 검증**:
   - 대한항공(`https://www.koreanair.com/`) 접속 시 한국어 브라우저로 감지되어 한국어 메인 페이지로 자동 정률 반환.
   - JavaScript `navigator.language` 가 `"ko-KR"`, `navigator.languages` 가 `["ko-KR", "ko", "en-US", "en"]` 로 정상 출력됨을 확인.
   - 패스워드 매니저 및 내장 Chrome UI가 OS 설정 언어와 완전히 연동되어 동작함.
