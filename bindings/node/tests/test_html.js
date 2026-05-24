#!/usr/bin/env node
'use strict';
// Parity test: sanitized HTML tags (stage 2 – parseHtml).
const { runTests } = require('./helpers');

runTests([
  // Simple inline element
  'plain <b>bold</b> text',
  // Span with attribute
  '<span class="highlight">inside</span> end',
  // Self-closing void element
  'line break<br/>here',
  // Nested inline elements
  '<b><i>bold italic</i></b>',
  // Unknown tag – not in allowed list, left as-is
  '<foo>not a real tag</foo>',
  // Heading with HTML inside
  '== Heading with <span>span</span> ==',
  // Table element tags
  '<table><tr><td>cell</td></tr></table>',
  // Attributes: multiple
  '<span id="x" class="y">text</span>',
  // Uppercase tag name – should be lowercased and treated as the same tag
  '<B>upper bold</B>',
  // code and pre inline
  '<code>inline code</code> and <pre>block</pre>',
  // Unclosed tag
  '<b>unclosed bold',
  // nowiki prevents HTML parsing inside
  '<nowiki><b>not bold</b></nowiki>',
  // Nested but mismatched tags
  '<b><i>bold italic</b></i>',
  // abbr with title attribute
  '<abbr title="HyperText Markup Language">HTML</abbr>',
  // Empty element
  '<span></span>',
  // wbr void element
  'word<wbr/>break',
  // meta/link require real itemprop+content/href attrs, not substring matches in values
  '<meta data="itemprop" content="x">',
  '<link data="itemprop" href="/x">',
], { name: 'html' });