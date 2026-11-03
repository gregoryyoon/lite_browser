import { drawDiagram } from './mermaid.js';

const REDRAW_DELAY_MS = 500;
const isMermaid = (language) => typeof language === 'string' && language.trim().toLowerCase() === 'mermaid';
// tui writes attribute values verbatim. Brackets are escaped too so a [[wikilink]] pass leaves
// mermaid's own [[...]] shapes alone; the browser decodes all of it back in `dataset.code`.
const escapeAttr = (s) =>
  s.replace(/&/g, '&amp;').replace(/"/g, '&quot;').replace(/</g, '&lt;').replace(/>/g, '&gt;').replace(/\[/g, '&#91;').replace(/\]/g, '&#93;');

/**
 * tui.editor plugin that draws ```mermaid code blocks as diagrams:
 * - markdown preview: the block is replaced by the diagram;
 * - WYSIWYG: the code block stays editable and the diagram is shown right below it.
 * @param {() => boolean} isDark whether diagrams are drawn with mermaid's dark theme
 */
export function mermaidPlugin(isDark) {
  return (context) => {
    const { Plugin } = context.pmState;
    const { Decoration, DecorationSet } = context.pmView;

    // Preview: the renderer leaves empty boxes; fill them after each preview update.
    context.eventEmitter.listen('afterPreviewRender', (preview) => {
      preview.previewContent.querySelectorAll('.mermaid-diagram:not([data-source])').forEach((el) => {
        void drawDiagram(el, el.dataset.code ?? '', isDark());
      });
    });

    // WYSIWYG: one box per mermaid block (by order), reused across edits so the diagram doesn't flicker;
    // a changed source is redrawn after a short pause in typing.
    const boxes = [];
    const timers = new Map();
    const boxFor = (index, source) => {
      let box = boxes[index];
      if (!box) {
        box = boxes[index] = document.createElement('div');
        box.className = 'mermaid-diagram';
        box.contentEditable = 'false';
      }
      if (box.dataset.pending !== source) {
        box.dataset.pending = source;
        const first = box.dataset.source === undefined;
        clearTimeout(timers.get(box));
        const target = box;
        timers.set(box, setTimeout(() => void drawDiagram(target, source, isDark()), first ? 0 : REDRAW_DELAY_MS));
      }
      return box;
    };

    const decorations = (state) => {
      const widgets = [];
      state.doc.descendants((node, pos) => {
        if (node.type.name === 'codeBlock' && isMermaid(node.attrs.language)) {
          const index = widgets.length;
          const box = boxFor(index, node.textContent);
          widgets.push(Decoration.widget(pos + node.nodeSize, () => box, { key: `mermaid-${index}`, side: -1, ignoreSelection: true }));
        }
        return !node.isTextblock; // code blocks have no nested blocks
      });
      return DecorationSet.create(state.doc, widgets);
    };

    return {
      toHTMLRenderers: {
        codeBlock(node, ctx) {
          const { info, literal } = node;
          if (!isMermaid(info)) return ctx.origin();
          // Tag tokens (not raw html) so tui adds data-nodeid, which its incremental preview updates rely on.
          return [
            { type: 'openTag', tagName: 'div', outerNewLine: true, classNames: ['mermaid-diagram'], attributes: { 'data-code': escapeAttr(literal ?? '') } },
            { type: 'closeTag', tagName: 'div', outerNewLine: true },
          ];
        },
      },
      wysiwygPlugins: [() => new Plugin({ props: { decorations } })],
    };
  };
}
