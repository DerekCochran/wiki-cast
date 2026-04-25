#!/usr/bin/env node
'use strict';
// Parity test: templates, arguments, section headings (stage 1 – parseBraces).
const { runTests } = require('./helpers');

runTests([
  // Basic template
  '{{Template}}',
  // Template with one positional argument
  '{{Template|arg}}',
  // Template with named argument
  '{{Template|key=value}}',
  // Template with multiple arguments
  '{{Template|a|b|c}}',
  // Nested templates
  '{{Outer|{{Inner}}}}',
  // Triple-brace argument
  '{{{arg}}}',
  // Triple-brace argument with default
  '{{{arg|default}}}',
  // {{!}} pipe placeholder
  '{{!}}',
  // {{=}} equals placeholder
  '{{=}}',
  // Mixed text and template
  'before {{Template}} after',
  // Multiple templates on one line
  '{{A}} and {{B}} end',
  // Template name with spaces (trimmed)
  '{{ Template }}',
  // Template with empty argument
  '{{Template|}}',
  // ISBN should be a template in this config, not a magic word
  '{{ISBN|9781583228947}}',
  // Section heading level 1
  '= Heading =',
  // Section heading level 2
  '== Section ==',
  // Section heading level 6
  '====== Deep ======',
  // Heading with trailing space in marker
  '== Section ==   ',
  // Heading with template inside
  '== {{Template}} ==',
  // Internal link (parsed in stage 5 but brace stage still sees [[)
  '[[Main Page]]',
  // Language converter (parsed at stage 10)
  '-{zh:漢字;zh-hans:汉字}-',
  // Multiline template with newline before first parameter
  '{{Navboxes\n|list=\n{{Libertarian socialism}}\n{{Libertarianism}}\n}}',

  // BEGIN: auto-generated parity sweep (braces)
  "{{T|v=RFC 2119}}",
  "{{T|v=__NOTOC__}}",
  "{{T|v=-{zh-hans:简;zh-hant:繁;}-}}",
  // END: auto-generated parity sweep (braces)
], { name: 'braces' });