#!/usr/bin/env node
'use strict';
// Parity test: free URLs, RFC, PMID, ISBN (stage 8 – parseMagicLinks).
const { runTests } = require('./helpers');

runTests([
  // Free HTTP URL
  'An autolink: http://example.com in text.',
  // Free HTTPS URL
  'Secure: https://secure.example.org/path here.',
  // URL with path and query
  'See http://example.com/path?q=1&r=2 for details.',
  // URL followed by punctuation (punctuation stripped from URL)
  'Visit http://example.com. End.',
  // URL with parentheses – trailing ) stripped unless ( appears in URL
  'See http://example.com/foo(bar) here.',
  // Free FTP URL
  'Download at ftp://files.example.com/file.tar.gz end.',
  // RFC magic link
  'See RFC 2119 for definitions.',
  // RFC lowercase
  'see rfc 2119 here.',
  // PMID magic link
  'Reference PMID 12345678 here.',
  // ISBN magic link (10-digit)
  'Book ISBN 0-306-40615-2 here.',
  // ISBN magic link (13-digit)
  'Book ISBN 978-3-16-148410-0 here.',
  // ISBN not preceded by a word character
  '(ISBN 0-306-40615-2)',
  // URL preceded by word character – should NOT autolink
  'wordhttps://example.com not linked.',
  // URL with &lt; entity in it – truncated at entity
  'http://example.com/a&lt;b here.',

  // BEGIN: auto-generated parity sweep (magic_links)
  "<ref>RFC 2119</ref>",
  "<ref>https://example.org/a</ref>",
  "{{T|v=RFC 2119}}",
  "{|\n| RFC 2119\n|}",
  // END: auto-generated parity sweep (magic_links)
], { name: 'magic_links' });