#!/usr/bin/env node
'use strict';
// Parity test: redirect detection (stage 0a).
const { runTests } = require('./helpers');

runTests([
  // Standard ASCII keyword, plain title
  '#REDIRECT [[Target Page]]',
  // Lowercase
  '#redirect [[Main Page]]',
  // Mixed case
  '#Redirect [[Article]]',
  // Explicit colon separator
  '#REDIRECT: [[Target Page]]',
  // Leading whitespace
  '   #REDIRECT [[Target]]',
  // Trailing whitespace after ]]
  '#REDIRECT [[Target]]   ',
  // Trailing content is preserved
  '#REDIRECT [[Target]]\nSome trailing text',
  // CJK redirection keyword
  '#重定向 [[中文页面]]',
  // Not a redirect – plain text (no-op)
  'This is not a redirect',
  // Newline inside title – invalid, should not redirect
  '#REDIRECT [[Target\nBroken]]',
  // Empty target
  '#REDIRECT [[]]',
  // Pipe variant – extra display text after target
  '#REDIRECT [[Target|ignored display]]',
  // Namespace-prefixed target
  '#REDIRECT [[Help:Contents]]',
  // Anchor in target
  '#REDIRECT [[Page#Section]]',
  // No space before [[
  '#REDIRECT[[Target]]',

  // BEGIN: auto-generated parity sweep (redirect)
  "#REDIRECT [[Target<!--c-->]]",
  // END: auto-generated parity sweep (redirect)
], { name: 'redirect' });