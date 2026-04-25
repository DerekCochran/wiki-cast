#!/usr/bin/env node
'use strict';
// Parity test: list items (stage 9 – parseList).
const { runTests } = require('./helpers');

runTests([
  // Unordered list
  '* Item 1\n* Item 2\n* Item 3',
  // Ordered list
  '# One\n# Two\n# Three',
  // Description list – term
  '; Term : Definition',
  // Description list – definition only
  ': just a definition',
  // Mixed list types
  '* Bullet\n# Number\n* Bullet again',
  // Nested unordered
  '* Level 1\n** Level 2\n** Level 2 again\n* Level 1 again',
  // Nested ordered
  '# First\n## Sub-first\n## Sub-second\n# Second',
  // Mixed nesting
  '* Bullet\n*# Numbered sub-item\n* Bullet again',
  // List item with inline markup
  "* ''italic'' item",
  // List item with a link
  '* [[Main Page|home page]] item',
  // Indented block (: prefix) that is NOT a definition list term
  ': indented paragraph',
  // Deep nesting
  '**** fourth level',
  // List after a blank line (new list context)
  '* First list\n\n* Second list',
  // Plain text line between list items
  '* item 1\nplain text\n* item 2',

  // BEGIN: auto-generated parity sweep (list)
  "* -{zh-hans:简;zh-hant:繁;}-",
  "* {{T|v=RFC 2119}}",
  // END: auto-generated parity sweep (list)
], { name: 'list' });