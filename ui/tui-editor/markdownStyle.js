// Fenced code blocks: written by the user, never part of the note's list/rule style.
const CODE = /(```[\s\S]*?```|~~~[\s\S]*?~~~)/;
const RULE = /^ {0,3}([-*_])( *\1){2,} *$/;

const outsideCode = (md) => md.split(CODE).filter((_part, i) => i % 2 === 0).join('\n');

/** The key that occurs most in `counts`; the first key wins ties. */
const mostUsed = (counts) => Object.entries(counts).reduce((best, e) => (e[1] > best[1] ? e : best))[0];

/**
 * @typedef {object} NoteStyle
 * @property {string} bullet
 * @property {string} rule
 * @property {'marker' | 'tab' | 'four'} indent Nested list indentation: the parent's marker width ("- " → 2, "1. " → 3), a tab per level, or 4 spaces per level.
 */

/**
 * The list bullet ("-", "*" or "+"), thematic break ("---", "***" or "___") and nested list indentation
 * a note mostly uses; "-", "---" and marker width if it has none.
 * @param {string} md
 * @returns {NoteStyle}
 */
export function noteStyle(md) {
  const bullets = { '-': 0, '*': 0, '+': 0 };
  const rules = { '---': 0, '***': 0, ___: 0 };
  let minIndent = Infinity; // of nested list items indented with spaces
  let tabs = false;
  const lines = outsideCode(md.replace(/\r\n/g, '\n')).split('\n');
  lines.forEach((line, i) => {
    const rule = RULE.exec(line);
    // "---" under text is a heading underline, and on the first line it opens front matter.
    if (rule) {
      if (rule[1] !== '-' || (i > 0 && lines[i - 1].trim() === '')) rules[rule[1].repeat(3)]++;
      return;
    }
    const bullet = /^[ \t]*([*+-])[ \t]+\S/.exec(line);
    if (bullet) bullets[bullet[1]]++;
    const nested = /^([ \t]+)(?:[*+-]|\d+[.)])[ \t]+\S/.exec(line);
    if (nested?.[1].includes('\t')) tabs = true;
    else if (nested) minIndent = Math.min(minIndent, nested[1].length);
  });
  const indent = tabs ? 'tab' : minIndent >= 4 && minIndent !== Infinity ? 'four' : 'marker';
  return { bullet: mostUsed(bullets), rule: mostUsed(rules), indent };
}

const FENCE = /^ *(```|~~~)/;
const LIST_ITEM = /^( *)([-*+]|\d+[.)]) /;

/**
 * Rewrites markdown written by tui.editor ("*" bullets, "***" breaks, 4 spaces per list level) in the
 * note's style. Code is left alone; lists not indented the tui way (typed in markdown mode) are kept as they are.
 * @param {string} md
 * @param {NoteStyle} style
 * @returns {string}
 */
export function applyNoteStyle(md, style) {
  // levels[d]: indentation of list level d in the note's style; null when the list is not in tui's layout.
  let levels = [''];
  let fenceLevel = -1; // list level of the open code fence, -1 outside code
  const reindent = (line, level) => (levels && style.indent !== 'four' ? levels[level] + line.slice(4 * level) : line);
  return md
    .split('\n')
    .map((line) => {
      const lead = /^ */.exec(line)[0].length;
      if (fenceLevel >= 0) {
        const at = fenceLevel;
        if (FENCE.test(line)) fenceLevel = -1;
        return at === 0 ? line : reindent(line, Math.min(at, Math.floor(lead / 4)));
      }
      if (line.trim() === '') return line;
      if (lead < 4 && !LIST_ITEM.test(line)) levels = ['']; // the list ended
      const level = Math.floor(lead / 4);
      if (FENCE.test(line)) {
        fenceLevel = levels ? Math.min(level, levels.length - 1) : 0;
        return reindent(line, fenceLevel);
      }
      const item = LIST_ITEM.exec(line);
      if (!item) {
        if (line === '***') return style.rule;
        return levels ? reindent(line, Math.min(level, levels.length - 1)) : line;
      }
      if (lead === 0) levels = [''];
      const marker = item[2] === '*' ? style.bullet : item[2];
      if (!levels || lead % 4 !== 0 || level >= levels.length) {
        levels = null;
        return line.replace(LIST_ITEM, `$1${marker} `);
      }
      levels.length = level + 1;
      const child = style.indent === 'tab' ? '\t' : ' '.repeat(marker.length + 1);
      levels.push(levels[level] + child);
      return reindent(`${line.slice(0, lead)}${marker} ${line.slice(item[0].length)}`, level);
    })
    .join('\n');
}
