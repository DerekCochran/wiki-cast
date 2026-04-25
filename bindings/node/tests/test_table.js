#!/usr/bin/env node
'use strict';
// Parity test: wikitext tables (stage 3 – parseTable).
const { runTests } = require('./helpers');

runTests([
  // Minimal table
  '{|\n|-\n| cell\n|}',
  // Table with caption
  '{| class="wikitable"\n|+ Caption\n|-\n| A || B\n|-\n| C || D\n|}',
  // Multiple cells on one row (|| separator)
  '{|\n| R1C1 || R1C2 || R1C3\n|-\n| R2C1 || R2C2 || R2C3\n|}',
  // Header cells (! syntax)
  '{|\n! Header1 !! Header2\n|-\n| data1 || data2\n|}',
  // Table with row attributes
  '{|\n|- class="odd"\n| cell\n|}',
  // Nested table
  '{|\n| outer || {|\n| inner\n|}\n|}',
  // Text before and after table
  'prefix\n{|\n| cell\n|}\nsuffix',
  // Cell with attributes
  '{|\n| style="color:red" | red text\n|}',
  // Empty table
  '{|\n|}',
  // Table with indented start (dd prefix)
  ':{|\n| cell\n|}',
  // Closing delimiter followed by more text on same line
  '{|\n| cell\n|} trailing text',
  // Table caption with attributes
  '{|\n|+ style="font-weight:bold" | Important\n|-\n| data\n|}',

  // Wikitext repro: continuation line after <br /> inside a table cell must stay in the same td-inner
  "{| class=\"wikitable\"\n|-\n|Μῆνιν ἄειδε θεὰ Πηληιάδεω Ἀχιλῆος<br />\nοὐλομένην, ἣ μυρί' Ἀχαιοῖς ἄλγε' ἔθηκε, [...]\n|Sing, Goddess, of the rage of Peleus' son Achilles,<br />\nthe accursed rage that brought great suffering to the Achaeans, [...]\n|}",

  // BEGIN: auto-generated parity sweep (table)
  "{|\n| RFC 2119\n|}",
  "{|\n| https://example.org/a\n|}",
  "{|\n| -{zh-hans:简;zh-hant:繁;}-\n|}",
  "{|\n| {{T|v=RFC 2119}}\n|}",
  // END: auto-generated parity sweep (table)
], { name: 'table' });