#!/usr/bin/env node
'use strict';
// Parity test: horizontal rules and double-underscore keywords (stage 4).
const { runTests } = require('./helpers');

runTests([
  // Horizontal rule (4 dashes minimum)
  'Line above\n----\nLine below',
  // More than 4 dashes
  'Line above\n-----------\nLine below',
  // Fewer than 4 dashes – not an HR
  'Line above\n---\nLine below',
  // __NOTOC__ keyword
  '__NOTOC__ in text',
  // __TOC__ keyword
  '==Section==\n__TOC__\nContent',
  // __FORCETOC__ keyword
  'Start\n__FORCETOC__\nEnd',
  // __NOEDITSECTION__ keyword
  '__NOEDITSECTION__\n== Section ==',
  // __NEWSECTIONLINK__ keyword
  '__NEWSECTIONLINK__',
  // Alias keywords should canonicalize token name via config maps
  '__NOTC__',
  '__nOtC__',
  '__NOCC__',
  '__DISAMBIG__',
  '__EXPECTED_UNCONNECTED_PAGE__',
  // Double underscore that is NOT a keyword – left as-is
  '__NOTAKEYWORD__',
  // Section heading finalization (detected in stage 1, finalized in stage 4)
  '== Section Title ==\nContent here.',
  '=== Level 3 ===',
  // HR inside a table is still valid
  '{|\n|-\n| before\n----\nafter\n|}',

  // Heading trail must be a text node, not a bare string
  '== Before ==\n\n',
  '== Before ==\n\n----\n\n== After ==',

  // BEGIN: auto-generated parity sweep (hr_and_double_underscore)
  "== RFC 2119 ==",
  "== https://example.org/a ==",
  "== -{zh-hans:简;zh-hant:繁;}- ==",
  // END: auto-generated parity sweep (hr_and_double_underscore)
], { name: 'hr_and_double_underscore' });