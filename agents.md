### Refer [**UsingTheCAPI.md**](C:\projects\cef-latest\cef_source\docs\using_the_capi.md) - Complete CEF C API guide for tests\cefsimple_capi project

* Base structure types (ref-counted vs scoped)
* Reference counting rules and patterns
* String handling
* Thread safety and atomic operations
* Complete working examples

### 빌드룰
- 코드 수정 및 빌드는 cef_binary_151.3.24 폴더만 해줘
- 빌드는 디버그 모드만 해줘
- git push 및 문서 업데이트 및 릴리즈 모드 빌드는 요청할때만 해줘

### 테마 및 아이콘 가이드
- 웹 UI 아이콘은 외부 CDN 없이 인라인 Lucide SVG(`viewBox="0 0 24 24"`, `stroke="currentColor"`)를 사용하며, 이모지 문자 배제
- 모든 네이티브 윈도우(`CreateWindowEx`)는 다크/라이트 모드별 브러시 분기, `WM_ERASEBKGND` 처리, DWM 다크 타이틀바 동기화 필수
- 테이블 및 리스트 컴포넌트는 단일행 말줄임표(`nowrap` + `ellipsis`) 및 주요 컬럼 고정 너비 레이아웃 원칙 준수
 



