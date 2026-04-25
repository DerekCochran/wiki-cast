#!/usr/bin/env node
'use strict';
// Parity test: HTML comments and extension tags (stage 0b).
const { runTests } = require('./helpers');

runTests([
  // Simple comment
  'before <!--comment--> after',
  // Multi-line comment
  'a <!--\n  multi-line\n  comment\n--> b',
  // Unclosed comment
  'start <!-- unclosed',
  // Nested comment-like (MediaWiki does not nest comments)
  '<!-- outer <!-- inner --> still outer -->',
  // nowiki tag – prevents parsing of contained markup
  'literal <nowiki>[[notalink]]</nowiki> tail',
  // nowiki self-closing
  '<nowiki/>',
  // ref tag
  'text<ref>Citation here.</ref> end',
  // ref with attributes
  'text<ref name="foo">Named ref.</ref> end',
  // ref inner content should still parse quotes/template in later stages
  "text<ref>Merriman, John M. (2009). ''How a Bombing in Fin-de-Siecle Paris Ignited the Age of Modern Terror''. New Haven: Yale University Press. p. 42. {{ISBN|9780300158864}}</ref> end",
  // ref with nested template using {{!}} in parameter value
  'text<ref>{{Cite web |title=Foo {{!}} Bar |url=https://example.com}}</ref> end',
  // ref self-closing
  'text<ref name="bar"/> end',
  // references tag
  '<references/>',
  // pre tag (extension, not HTML)
  '<pre>preformatted content</pre>',
  // Double comment
  '<!-- a --> middle <!-- b -->',
  // Comment with wikitext inside is suppressed
  '<!-- [[NotALink]] -->',
  // includeonly tag – only rendered when transcluded
  'before <includeonly>only when included</includeonly> after',
  // noinclude tag
  '<noinclude>only on the template page</noinclude>',
  // Plain text – no-op
  'just plain text',

  // BEGIN: auto-generated parity sweep (comment_and_ext)
  "<ref>RFC 2119</ref>",
  "<ref>https://example.org/a</ref>",
  "<ref>-{zh-hans:简;zh-hant:繁;}-</ref>",
  "<ref>----</ref>",
  "<ref>{{T|v=RFC 2119}}</ref>",
  // END: auto-generated parity sweep (comment_and_ext)
], { name: 'comment_and_ext' });