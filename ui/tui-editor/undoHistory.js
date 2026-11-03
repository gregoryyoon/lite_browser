/**
 * Drops the undo history of both editing modes. Without this, Ctrl+Z right after loading a file
 * (or replacing the content with setMarkdown) steps back to the empty/old document.
 * tui.editor has no public API for this, so it reaches its ProseMirror views.
 * @param {object} editor a toastui.Editor
 */
export function clearUndoHistory(editor) {
  for (const view of [editor.mdEditor.view, editor.wwEditor.view]) {
    const { doc, selection, plugins } = view.state;
    // view.state.constructor is tui's own EditorState class, so no separate prosemirror-state is needed.
    view.updateState(view.state.constructor.create({ doc, selection, plugins }));
  }
}
