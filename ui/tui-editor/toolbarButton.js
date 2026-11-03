/**
 * A toolbar button with an SVG icon (instead of tui's icon sprite), for editor.insertToolbarItem.
 * Styled by `.tui-extras-button` in tui-extras.css.
 * @param {{ label: string, svg: string, onClick: () => void }} options
 * @returns {HTMLButtonElement}
 */
export function toolbarButton({ label, svg, onClick }) {
  const button = document.createElement('button');
  button.type = 'button';
  button.className = 'toastui-editor-toolbar-icons tui-extras-button';
  button.setAttribute('aria-label', label);
  button.innerHTML = svg;
  button.addEventListener('click', onClick);
  return button;
}
