#!/usr/bin/env node
'use strict';
// Parity test: language-variant converter (stage 10 – parseConverter).
// The converter only activates when config.variants is non-empty.
const { runTests } = require('./helpers');

runTests([
  // Simple converter with no flags
  '-{text}-',
  // Converter with a single rule
  '-{zh-hans:简体;zh-hant:繁體}-',
  // Converter with multiple rules
  '-{zh:漢字;zh-hans:汉字;zh-hant:漢字}-',
  // Converter with flags
  '-{A|zh-hans:简体;zh-hant:繁體}-',
  // Converter with raw flag (output as-is in all variants)
  '-{R|raw text}-',
  // Nested converters
  '-{zh-hans:-{inner}-;zh-hant:outer}-',
  // Converter inside a template argument
  '{{Template|-{zh-hans:简;zh-hant:繁}-}}',
  // Plain text – no converter (no-op)
  'plain text without converter',
  // Converter adjacent to other markup
  "''italic'' -{ zh-hans:汉字 }- text",
  // Empty converter body
  '-{}-',
  // Converter with a pipe but no flags
  '-{|zh-hans:简体;zh-hant:繁體}-',
], { name: 'converter' });