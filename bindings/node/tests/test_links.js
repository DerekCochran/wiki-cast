#!/usr/bin/env node
'use strict';
// Parity test: internal links, files, and categories (stage 5 – parseLinks).
const { runTests } = require('./helpers');

runTests([
  // Simple internal link
  'See [[Main Page]] for details.',
  // Link with display text
  '[[Page|display text]]',
  // Link with colon prefix (forces link, not category/file)
  '[[:Category:Foo|label]]',
  // Namespace-prefixed link
  '[[Help:Contents]]',
  // Link with anchor
  '[[Page#Section|section link]]',
  // File / image embed (namespace 6)
  '[[File:Image.jpg]]',
  // File with options
  '[[File:Image.jpg|thumb|right|Caption text]]',
  '[[File:Image.jpg| thumb | upright=0.8 | Caption text]]',
  // File target should preserve/canonicalize first-letter case like JS parser
  '[[File:water_reflectivity.jpg]]',
  // File caption that is only a nested wikilink after thumb
  '[[File:Image.jpg|thumb|[[Pierre-Joseph Proudhon]]]]',
  // Category link
  '[[Category:Example]]',
  // Category link with sort key
  '[[Category:Example|sort key]]',
  // Self-link (anchor only)
  '[[#Section]]',
  // Interwiki-like (invalid, treated as plain text in standard config)
  '[[en:English article]]',
  // Link with template inside display text
  '[[Page|{{Template}}]]',
  // Multiple links in one sentence
  '[[Page A]] and [[Page B]] go here.',
  // Link immediately followed by letters (no space)
  '[[Page]]s',
  // Nested brackets that are NOT a link
  '[[invalid link]]s or just [[valid]]',

  // BEGIN: auto-generated parity sweep (links)
  "[[A|-{zh-hans:简;zh-hant:繁;}-]]",
  "[[File:water_reflectivity.jpg|thumb|RFC 2119]]",
  "[[File:water_reflectivity.jpg|thumb|https://example.org/a]]",
  "[[File:Mardi&nbsp;Gras&nbsp;Mobile&nbsp;Order of Inca.jpg|thumb|left|upright|Mobile is the birthplace of Mardi Gras in the U.S.]]",
  // END: auto-generated parity sweep (links)
], { name: 'links' });