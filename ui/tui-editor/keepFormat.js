import { diffArrays, diffChars } from 'diff';
import { applyNoteStyle, noteStyle } from './markdownStyle.js';

/**
 * A run of non-blank lines and the blank lines before it.
 * @typedef {{ blanks: string[], lines: string[] }} Block
 */

/** The note as blocks, plus the blank lines at its end. */
function toBlocks(text) {
  const blocks = [];
  let blanks = [];
  let current = null;
  for (const line of text.split('\n')) {
    if (line.trim() === '') {
      current = null;
      blanks.push(line);
    } else if (current) {
      current.lines.push(line);
    } else {
      blocks.push((current = { blanks, lines: [line] }));
      blanks = [];
    }
  }
  return { blocks, trailing: blanks };
}

/**
 * An edited line: the user's changes (baseline line → current line) applied to the original line,
 * so the characters around them keep the original's text instead of tui's escapes ("\#", "\_").
 */
function editLine(original, baseline, current) {
  // at[i]: position in `original` of baseline character i (tui's added escapes map to the next original character).
  const at = [];
  let o = 0;
  for (const part of diffChars(original, baseline)) {
    if (part.removed) o += part.value.length;
    else for (const _ch of part.value) at.push(part.added ? o : o++);
  }
  at.push(original.length);
  let out = '';
  let b = 0;
  for (const part of diffChars(baseline, current)) {
    if (part.added) out += part.value;
    else if (part.removed) b += part.value.length;
    else out += original.slice(at[b], at[(b += part.value.length)]);
  }
  return out;
}

/**
 * Original blocks and the baseline blocks [start, end) tui made of them: equal blocks one by one, rewritten stretches as a whole.
 * @typedef {{ original: Block[], start: number, end: number }} Segment
 */

const key = (b) => b.lines.join('\n');
// Pairs original text with tui's rewrite of it when they differ only in escapes and spacing ("## 3." / "## 3\.").
const loose = (text) => text.replace(/\\(?=[!-/:-@[-`{-~])/g, '').replace(/\s+/g, '');
const looseKey = (b) => loose(key(b));

/** @returns {Segment[]} */
function align(original, baseline) {
  const segments = [];
  let pos = 0;
  let o = 0;
  let rewritten = null;
  for (const part of diffArrays(original.map(looseKey), baseline.map(looseKey))) {
    const n = part.value.length;
    if (part.added || part.removed) {
      if (!rewritten) segments.push((rewritten = { original: [], start: pos, end: pos }));
      if (part.removed) rewritten.original.push(...original.slice(o, (o += n)));
      else rewritten.end = pos += n;
    } else {
      rewritten = null;
      for (let k = 0; k < n; k++) segments.push({ original: [original[o++]], start: pos, end: ++pos });
    }
  }
  return segments;
}

/**
 * tui.editor rewrites the whole note in its own markdown style ("*" bullets, 4-space nesting, "***",
 * escapes, spacing) on every save. This keeps every block (run of non-blank lines) the user did not
 * touch exactly as it was in the file, blank lines before it included. In edited blocks, unchanged lines
 * and the text around each change stay as in the file, and new text gets the note's own bullet, list
 * indentation and thematic break. `original` is the file text, `baseline` tui's markdown right after
 * loading it, `current` its markdown now.
 * @param {string} original
 * @param {string} baseline
 * @param {string} current
 * @returns {string}
 */
export function keepOriginalFormat(original, baseline, current) {
  if (current === baseline) return original;
  const style = noteStyle(original);
  const orig = toBlocks(original);
  const base = toBlocks(applyNoteStyle(baseline, style)).blocks;
  const cur = toBlocks(applyNoteStyle(current, style)).blocks;
  const segments = align(orig.blocks, base);
  const segmentOf = [];
  segments.forEach((s, i) => {
    for (let k = s.start; k < s.end; k++) segmentOf[k] = i;
  });
  // For each baseline block, the current block that is still the same, or -1.
  const currentOf = base.map(() => -1);
  // Edited blocks that took the place of a baseline block (as many edited as replaced): current index → baseline index.
  const replaced = new Map();
  let c = 0;
  let b = 0;
  let removed = 0;
  for (const part of diffArrays(base.map(key), cur.map(key))) {
    const n = part.value.length;
    if (part.removed) {
      b += removed = n;
    } else if (part.added) {
      if (removed === n) for (let x = 0; x < n; x++) replaced.set(c + x, b - n + x);
      c += n;
      removed = 0;
    } else {
      removed = 0;
      for (let k = 0; k < n; k++) currentOf[b++] = c++;
    }
  }
  const baseOf = cur.map(() => -1);
  currentOf.forEach((ci, bi) => {
    if (ci !== -1) baseOf[ci] = bi;
  });
  // A segment is kept when all of its baseline blocks are still there, in a row.
  const kept = (s) => {
    for (let k = s.start; k < s.end; k++) {
      if (currentOf[k] === -1 || (k > s.start && currentOf[k] !== currentOf[k - 1] + 1)) return false;
    }
    return s.end > s.start;
  };

  // An edited block keeps its original spacing and the original of each line the user did not change,
  // when tui's version of the block matches the original line for line (apart from escapes).
  const edited = (i) => {
    const k = replaced.get(i);
    const segment = k === undefined ? undefined : segments[segmentOf[k]];
    const o = segment && segment.end - segment.start === 1 && segment.original.length === 1 ? segment.original[0] : undefined;
    if (k === undefined || !o || o.lines.length !== base[k].lines.length || o.lines.some((line, x) => loose(line) !== loose(base[k].lines[x]))) {
      return [...cur[i].blanks, ...cur[i].lines];
    }
    const lines = [];
    let at = 0;
    let removed = [];
    for (const part of diffArrays(base[k].lines, cur[i].lines)) {
      if (part.removed) {
        removed = part.value;
        at += removed.length;
      } else if (part.added) {
        // Lines changed in place are edited character by character.
        const from = at - removed.length;
        lines.push(...part.value.map((line, x) => (removed.length === part.value.length ? editLine(o.lines[from + x], removed[x], line) : line)));
        removed = [];
      } else {
        removed = [];
        for (const _line of part.value) lines.push(o.lines[at++]);
      }
    }
    return [...(i === 0 && k !== 0 ? cur[i].blanks : o.blanks), ...lines];
  };

  const out = [];
  // Segments before `next` are written or replaced. Blocks tui dropped were never shown,
  // so the user could not have deleted them: they are written back in place.
  let next = 0;
  const writeDroppedBefore = (segment) => {
    for (; next < segment; next++) {
      const s = segments[next];
      if (s.start === s.end) for (const block of s.original) out.push(...block.blanks, ...block.lines);
    }
  };
  for (let i = 0; i < cur.length; ) {
    const s = baseOf[i] === -1 ? -1 : segmentOf[baseOf[i]];
    if (s !== -1) writeDroppedBefore(s);
    if (s !== -1 && kept(segments[s]) && currentOf[segments[s].start] === i) {
      // Original spacing, except when the blocks before were deleted and this one now starts the note.
      const nowFirst = i === 0 && segments[s].start !== 0;
      segments[s].original.forEach((block, k) => out.push(...(k === 0 && nowFirst ? cur[i].blanks : block.blanks), ...block.lines));
      next = s + 1;
      i += segments[s].end - segments[s].start;
    } else {
      if (s !== -1) next = Math.max(next, s + 1);
      const k = replaced.get(i);
      if (k !== undefined) {
        writeDroppedBefore(segmentOf[k]);
        next = Math.max(next, segmentOf[k] + 1);
      }
      out.push(...edited(i));
      i++;
    }
  }
  writeDroppedBefore(segments.length);
  out.push(...orig.trailing);
  return out.join('\n');
}
