import { keepOriginalFormat } from './keepFormat.js';

/**
 * tui.editor rewrites the note in its own style, normalizes to LF and drops the final newline.
 * Call this right after `fileText` is loaded into `editor`; the returned function gives the text to save:
 * the file's original text where it was not edited, with its line endings and final newline.
 * @param {{ getMarkdown(): string }} editor
 * @param {string} fileText
 * @returns {() => string}
 */
export function trackFileFormat(editor, fileText) {
  const eol = fileText.includes('\r\n') ? '\r\n' : '\n';
  const finalEol = fileText.endsWith('\n');
  const original = fileText.replace(/\r\n/g, '\n').replace(/\n$/, ''); // LF, without the final newline
  const baseline = editor.getMarkdown(); // tui's markdown for it right after loading
  return () => {
    const md = keepOriginalFormat(original, baseline, editor.getMarkdown()).replace(/\n/g, eol);
    return finalEol && !md.endsWith(eol) ? md + eol : md;
  };
}
