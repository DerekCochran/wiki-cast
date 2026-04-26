#!/usr/bin/env node
'use strict';
// Parity test: bracketed external links (stage 7 – parseExternalLinks).
const { runTests } = require('./helpers');

runTests([
  // URL-only external link
  '[http://example.com]',
  // External link with label
  '[http://example.com Example site]',
  // https
  '[https://secure.example.org/path Secure site]',
  // ftp
  '[ftp://files.example.com FTP link]',
  // Protocol-relative
  '[//example.com Protocol-relative]',
  // Multiple external links on one line
  '[http://a.org A] and [http://b.org B]',
  // External link inside running text
  'Visit [http://example.com this site] for more.',
  // Link with no label and query string
  '[http://example.com?q=foo&bar=baz]',
  // Link target with fragment
  '[http://example.com#anchor Anchor link]',
  // Link with special characters in URL
  '[http://example.com/path/(parens)/here Label]',
  // URL not inside brackets – handled by magicLinks stage, not this one
  'bare http://example.com in text',
  // Malformed bracket (no closing) – left as-is
  '[http://example.com unclosed',
  // Bracket with &lt; in URL – truncated at entity
  '[http://example.com/a&lt;b Label]',
  // Preserve exact separator whitespace between URL and label
  '[https://example.com\tlabel]',
], { name: 'external_links' });