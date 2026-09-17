# Windows 11 IME 관련 이슈 조사 정리

## 요약

Windows 11 환경에서 IME(Input Method Editor) 관련 이슈는 크게 두 갈래로 확인된다.

1. **Chromium/Chrome/Edge 계열의 Windows IME 처리 버그**
   - 특정 Chromium 버전에서 중국어/일본어 IME 입력이 누락되거나 첫 입력이 사라지는 문제가 있었다.
   - Chromium 쪽에서는 `TSFTextStore`의 autocorrect 처리 로직이 IME 입력을 잘못 되돌리는 문제로 확인되었고, 수정이 반영되었다.

2. **Windows 11 자체 IME/CTF Loader/사용자 프로필 문제**
   - 모든 앱에서 중국어/일본어 IME 전환 또는 입력 시 수 초 지연이 발생하는 사례가 있다.
   - `ctfmon.exe` CPU 사용률 상승, 사용자 프로필별 IME 설정 손상 등이 원인으로 의심된다.
   - 레지스트리 초기화, IME 이전 버전 사용, 언어팩 재설치 등이 해결책으로 제시되었다.

---

## 1. Chromium 149+ Windows IME 회귀 이슈

### 증상

- Windows 11에서 Chrome/Edge 149+ 사용 시 `contenteditable` 기반 에디터에서 첫 IME 조합 입력 또는 중국어 구두점 입력이 사라짐.
- 예: 중국어 IME에서 `.` 키를 눌러 `。`를 입력하려 해도 첫 입력이 무시되고, 두 번째 입력부터 반영됨.
- CodeMirror 6, React/Next.js 환경 등에서 보고되었지만, 원인은 애플리케이션 레벨이 아니라 Chromium 쪽 회귀로 정리됨.

### 영향 범위

- OS: Windows 11 Version 25H2 등
- 브라우저: Google Chrome 149.0.7827.102+, Microsoft Edge 149.0.4022.62+ 등
- Firefox 및 이전 Chromium 버전은 영향을 받지 않는 것으로 보고됨.

### 상태

- Chromium 이슈는 **Fixed** 상태.
- 수정 검증 버전:
  - Chrome 149.0.7827.158
  - Chrome 150.0.7871.27

### 참고 이슈

- Chromium Issue 523134891: Regression: First IME composition input/punctuation is lost on Windows in Chromium 149+
  - https://issues.chromium.org/issues/523134891

---

## 2. Japanese IME 전각 스페이스 / hidden textarea 입력 이벤트 누락

### 증상

- Chrome 149에서 Windows Microsoft Japanese IME 사용 시 전각 스페이스(U+3000) 입력이 누락됨.
- 특히 off-screen, zero-opacity, hidden textarea 같은 숨겨진 입력 요소에서 `input` 이벤트가 발생하지 않는 문제가 보고됨.
- Fabric.js 등 캔버스 기반 텍스트 편집에서 hidden textarea를 사용하는 경우 재현 가능.

### 원인

Chromium의 Windows TSF(Text Services Framework) 처리 코드에서 `autocorrect="off"` 필드를 다룰 때, 정상적인 IME 입력을 Windows 터치 키보드 autocorrect 결과로 잘못 판단하고 되돌리는 문제가 있었다.

문제의 핵심:

- Japanese IME는 전각 스페이스를 composition 없이 삽입할 수 있음.
- Chromium의 `TSFTextStore` autocorrect 방지 로직이 이를 잘못 감지함.
- 결과적으로 정상 IME 입력이 silently reverted 됨.

### 해결 방법

Chromium 코드에서 IME가 활성화된 상태라면 autocorrect detection을 수행하지 않도록 수정했다.

핵심 수정:

```cpp
!IsInputIME()
```

즉, `TSFTextStore`에서 현재 입력이 IME 입력이면 touch keyboard autocorrect rollback 대상으로 보지 않는다.

### 관련 파일

- `ui/base/ime/win/tsf_text_store.cc`
- `ui/base/ime/win/tsf_text_store.h`
- `ui/base/ime/win/tsf_text_store_unittest.cc`

### 참고 이슈 및 CL

- Chromium Issue 521644696: Windows IME: Missing 'input' event on hidden textarea when typing full-width space in Chrome 149
  - https://issues.chromium.org/issues/521644696
- Chromium Issue 521205128: Simplified Chinese input in contenteditable requires two key presses to insert punctuation
  - https://issues.chromium.org/issues/521205128
- Gerrit CL: Skip autocorrect detection when IME is active in TSFTextStore
  - https://chromium-review.googlesource.com/c/chromium/src/+/7917332

---

## 3. Windows 11 중국어/일본어 IME 입력 지연 이슈

### 증상

- 영어에서 중국어 또는 일본어 IME로 전환할 때 앱이 몇 초간 멈춤.
- Teams, Chrome, WeChat, Notepad, Word 등 특정 앱이 아니라 시스템 전체에서 발생하는 사례가 보고됨.
- 문제가 발생하는 동안 `CTF Loader(ctfmon.exe)`가 20~30% 수준의 CPU를 사용하는 사례가 있음.
- 새 Windows 사용자 프로필에서는 문제가 사라지는 사례가 있어, 사용자 프로필 내 IME 설정 손상이 원인일 가능성이 제기됨.

### 해결 방법 후보

#### 1. IME Platform Registry 초기화

1. `Win + R` 실행
2. `regedit` 입력
3. 아래 경로로 이동

```text
HKEY_CURRENT_USER\Software\Microsoft\Input
```

4. `Input` 키를 삭제하지 말고 `Input.old` 등으로 이름 변경
5. 작업 관리자에서 `CTF Loader(ctfmon.exe)` 종료
6. 자동 재시작 후 IME 전환 테스트

효과:

- Windows가 입력 서비스 설정을 새로 생성함.
- 프로필 내 손상된 IME 설정으로 인한 지연이 해결될 수 있음.

주의:

- 레지스트리 변경 전 백업 권장.
- 삭제 대신 rename으로 보관하는 방식이 안전함.

#### 2. Chinese/Japanese 언어팩 재설치

1. 설정 > 시간 및 언어 > 언어 및 지역
2. 중국어 또는 일본어 언어팩 제거
3. 재부팅
4. 언어팩 다시 추가
5. IME 전환 및 입력 테스트

#### 3. CTF 관련 DLL 재등록

관리자 권한 명령 프롬프트에서 아래 명령 실행:

```cmd
regsvr32.exe msctf.dll
regsvr32.exe msimtf.dll
```

일부 사용자는 이 방법으로 IME 지연 문제가 해결되었다고 보고함.

#### 4. 새 Windows 사용자 프로필 생성

새 프로필에서는 문제가 발생하지 않는다면 기존 프로필의 IME 설정 또는 AppData 쪽 손상 가능성이 높다.

주의:

- 새 프로필로 이전할 때 AppData 전체를 복사하면 손상된 설정까지 옮길 수 있으므로 주의.

---

## 4. Microsoft 공식 우회책: 이전 버전 IME 사용

Microsoft는 현재 IME에서 문제가 발생할 경우 임시 우회책으로 이전 버전 IME 사용을 안내한다.

### 설정 경로 예시

#### Japanese IME

1. 설정 > 시간 및 언어 > 언어 및 지역
2. 일본어 > 언어 옵션
3. Microsoft IME > 키보드 옵션
4. 일반
5. **Use previous version of Microsoft IME** 활성화

#### Chinese Microsoft Pinyin

1. 설정 > 시간 및 언어 > 언어 및 지역
2. Chinese > 언어 옵션
3. Microsoft Pinyin > 키보드 옵션
4. 일반
5. **Use previous version of Microsoft Pinyin** 활성화

### 주의

- Microsoft는 이 설정을 장기 해결책이 아니라 임시 workaround로 설명한다.
- 가능하면 Windows/브라우저 업데이트로 근본 해결을 확인하는 것이 좋다.

---

## 권장 대응 순서

### Chrome/Edge에서만 문제가 발생하는 경우

1. Chrome/Edge를 최신 버전으로 업데이트
2. 문제가 Chromium 149 계열 회귀와 유사한지 확인
3. `contenteditable`, hidden textarea, CodeMirror, Fabric.js 등 특정 입력 구현에서만 발생하는지 확인
4. 가능하다면 Chrome 149.0.7827.158 이상 또는 150.0.7871.27 이상에서 재검증

### 모든 앱에서 IME 지연이 발생하는 경우

1. Windows Update 및 언어/IME 업데이트 확인
2. IME 학습 기록 및 사용자 사전 초기화
3. 이전 버전 Microsoft IME 임시 사용
4. `ctfmon.exe` CPU 사용률 확인
5. `HKCU\Software\Microsoft\Input`을 `Input.old`로 rename 후 CTF Loader 재시작
6. 언어팩 제거 후 재설치
7. 그래도 해결되지 않으면 새 Windows 사용자 프로필에서 재현 여부 확인

---

## 결론

- Windows 11에서 IME 관련 이슈는 실제로 존재하며, Chromium 계열 브라우저의 버그와 Windows 사용자 프로필/IME 설정 문제를 구분해서 접근해야 한다.
- Chromium의 대표적인 IME 입력 누락 문제는 `TSFTextStore`의 autocorrect 처리 로직 수정으로 해결되었다.
- 시스템 전체 IME 지연 문제는 Windows IME 설정 손상, CTF Loader, 사용자 프로필 문제일 가능성이 크며, 레지스트리 초기화나 이전 버전 IME 사용이 실질적인 우회책으로 확인된다.

---

## 참고 자료

- Chromium Issue 523134891  
  https://issues.chromium.org/issues/523134891
- Chromium Issue 523453997  
  https://issues.chromium.org/issues/523453997
- Chromium Issue 521644696  
  https://issues.chromium.org/issues/521644696
- Chromium Issue 521205128  
  https://issues.chromium.org/issues/521205128
- Chromium Gerrit CL 7917332  
  https://chromium-review.googlesource.com/c/chromium/src/+/7917332
- Microsoft Support: Revert to a previous version of an IME  
  https://support.microsoft.com/en-us/windows/hardware/input-devices/revert-to-a-previous-version-of-an-input-method-editor-ime
- Microsoft Japanese IME  
  https://support.microsoft.com/en-us/windows/hardware/input-devices/microsoft-japanese-ime
- Microsoft Q&A: Windows 11 Chinese IME Severe Input Lag Issue  
  https://learn.microsoft.com/en-us/answers/questions/5513255/windows-11-chinese-ime-severe-input-lag-issue
