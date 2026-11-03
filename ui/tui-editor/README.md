# tui-editor-extras

[TOAST UI Editor](https://ui.toast.com/tui-editor) 3.2.2에 붙여 쓰는 기능 모음입니다. 빌드 도구 없이 HTML에서 바로 불러 씁니다.
`D:\my_home`(마크다운 폴더 웹 앱)의 `client/src/`에서 복사해 일반 JS로 옮긴 것입니다. 에디터 원본은 수정하지 않습니다.

## 기능

| 파일 | 기능 |
|---|---|
| `fileFormat.js` (+ `keepFormat.js`, `markdownStyle.js`) | **원본 서식 보존.** tui는 저장할 때 문서 전체를 자기 방식(`*` 글머리표, 4칸 들여쓰기, `***`, `\#` 같은 이스케이프)으로 바꾸고, 줄바꿈을 LF로 통일하며, 마지막 줄바꿈을 지웁니다. 사용자가 고치지 않은 부분은 원래 텍스트 그대로 두고, 새로 쓴 부분에는 그 문서가 쓰던 글머리표·구분선·들여쓰기를 적용합니다. 원래 줄바꿈 문자(CRLF/LF)와 마지막 줄바꿈도 그대로 둡니다. |
| `mermaidPlugin.js` (+ `mermaid.js`) | **Mermaid 다이어그램.** 미리보기에서는 ` ```mermaid ` 코드 블록 대신 다이어그램을 보여주고, WYSIWYG에서는 코드 블록 바로 아래에 다이어그램을 그립니다. mermaid 라이브러리는 다이어그램이 있는 문서를 열 때만 불러옵니다. |
| `undoHistory.js` | **실행 취소 기록 초기화.** 파일을 불러온 직후 Ctrl+Z를 누르면 빈 문서로 돌아가는 문제를 막습니다. |
| `toolbarButton.js` | **SVG 아이콘 툴바 버튼.** `insertToolbarItem`에 넣을 버튼을 만듭니다. |
| `tui-extras.css` | 다이어그램과 툴바 버튼의 스타일입니다. |
| `example.html` | 위 기능을 모두 쓰는 예제 페이지입니다. |

## 사용법

1. 이 폴더의 파일을 프로젝트에 복사합니다. `README.md`와 `example.html`은 빼도 됩니다.
2. HTML에 아래 내용을 넣습니다. **import map은 모든 `<script type="module">`보다 앞에 있어야 합니다.**

```html
<link rel="stylesheet" href="https://uicdn.toast.com/editor/3.2.2/toastui-editor.min.css">
<link rel="stylesheet" href="tui-extras.css">
<script type="importmap">
  {
    "imports": {
      "diff": "https://cdn.jsdelivr.net/npm/diff@9.0.0/+esm",
      "mermaid": "https://cdn.jsdelivr.net/npm/mermaid@12.2.0/dist/mermaid.esm.min.mjs"
    }
  }
</script>
<script src="https://uicdn.toast.com/editor/3.2.2/toastui-editor-all.min.js"></script>

<script type="module">
  import { mermaidPlugin } from './mermaidPlugin.js';
  import { trackFileFormat } from './fileFormat.js';
  import { clearUndoHistory } from './undoHistory.js';

  const editor = new toastui.Editor({
    el: document.querySelector('#editor'),
    initialValue: fileText,
    plugins: [mermaidPlugin(() => false)], // 인자: 다크 테마인지 알려주는 함수
  });
  const contentToSave = trackFileFormat(editor, fileText); // 파일 내용을 넣은 직후에 호출
  clearUndoHistory(editor);

  // 저장할 때: editor.getMarkdown() 대신 contentToSave()
</script>
```

자세한 내용은 `example.html`을 보세요. `file://`로 열면 브라우저가 모듈 로딩을 막으므로, 웹 서버로 열어야 합니다.

## 주의할 점

- **파일 내용을 다시 넣을 때:** `editor.setMarkdown(새 내용)`으로 내용을 바꿨다면 `trackFileFormat(editor, 새 내용)`과 `clearUndoHistory(editor)`를 다시 호출하세요. 서식 보존은 "불러온 직후의 상태"를 기준으로 비교하기 때문입니다.
- **`events` 옵션을 쓸 때:** tui는 `initialValue`를 그린 **뒤에** `events`를 연결합니다. 그래서 `beforePreviewRender` 같은 이벤트가 첫 내용에는 적용되지 않습니다. 에디터를 만든 직후 `editor.setMarkdown(fileText, false)`를 한 번 더 호출하고, 그다음에 `trackFileFormat`을 호출하세요.
- **에디터를 닫을 때:** tui 툴바는 창 크기 변경 후 200ms 뒤에 다시 그려지는데, 그 전에 `destroy()`하면 오류가 납니다. `setTimeout(() => editor.destroy(), 300)`처럼 조금 늦춰서 닫으세요.
- **다크 테마:** 에디터 요소에 `toastui-editor-dark` 클래스를 붙이고(`toastui-editor-dark.min.css` 필요) `redrawDiagrams(요소, true)`(`mermaid.js`)를 호출하면 다이어그램도 다크 테마로 다시 그려집니다.
- **tui 버전은 3.2.2로 고정해서 쓰세요.** `undoHistory.js`는 tui의 내부 구조(`mdEditor.view`, `wwEditor.view`)를 쓰고, `markdownStyle.js`는 tui가 만드는 마크다운 형식을 전제로 합니다. 다른 버전에서는 동작이 깨질 수 있습니다.
- 원본(`D:\my_home\client\src\`)을 고치면 여기는 자동으로 바뀌지 않습니다. 필요하면 다시 복사해야 합니다.
