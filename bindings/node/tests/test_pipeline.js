#!/usr/bin/env node
'use strict';
// Parity test: full 11-stage parse pipeline on combined samples.
// These exercises multiple stages interacting at once.
const { runTests } = require('./helpers');

runTests([
  // Simple prose – no special markup
  'This is a paragraph of plain text.',

  // Redirect with comment
  '#REDIRECT [[Target]] <!-- a comment -->',

  // Template with link inside argument
  '{{Template|[[Page|label]]}}',

  // Section heading with template
  '== {{PAGENAME}} ==\nContent.',

  // Table with a link in a cell
  '{|\n| [[Page|link]] || plain\n|}',

  // Table cell link text spanning inline HTML must remain one link
  '{|\n! Modern [[State of matter|state<br />of matter]]\n|}',

  // Nested templates
  '{{Outer|{{Inner|arg}}}}',

  // Bold and italic with internal link
  "'''bold''' and ''[[Page|italic link]]''",

  // External link next to internal link
  '[[Page]] and [http://example.com external].',

  // Magic links alongside internal markup
  "See RFC 2119, [[Specification]] and http://example.com.",

  // List with templates
  '* {{Template}}\n* plain item\n* [[Page]]',

  // HR separating sections
  "== Before ==\n\n----\n\n== After ==",

  // Heading title should parse inline links
  '== Relationship to surface [[bulk density]] ==',

  // Double-underscore with surrounding text
  'Before __NOTOC__ after.',

  // Comment inside a template
  '{{Template|<!-- comment -->value}}',

  // Extension tag that disables parsing inside
  '<nowiki>[[not a link]] {{not a template}}</nowiki>',

  // Reference in running text
  'Claim.<ref>{{Citation|title=Foo|year=2020}}</ref> More text.',

  // Ref inner content with quotes and inline HTML should preserve html tokens
  "A<ref>''x'' efficacy is > <sub>2</sub>; ''y'' is > <sub>2</sub>.</ref>B",

  // Direct comments inside ref inner text should become comment tokens
  'A<ref>alpha<!--c--></ref>B',

  // Comment inside template parameter-value nested in ref should stay tokenized
  'A<ref>{{Citation|access-date= 29 June 2011<!--Added by DASHBot-->}}</ref>B',

  // Multiline quotes in parameter-value must be parsed per-line
  "{{T|v='''a\n''b'''}}",

  // Wikitext repro (An American in Paris): apostrophes inside <score> must stay raw text
  '<score raw=1 sound=1>\\relative c\'\' { x }</score>',

  // HTML tags inside template parameter-value should be tokenized inline
  '{{Citation|title=Effect of land albedo, CO<sub>2</sub>, orography}}',

  // Refn-style template argument with links and quotes in parameter value
  "{{Refn|In ''Anarchism: From Theory to Practice'' (1970), historian [[Daniel Guérin]] describes [[libertarian socialism]].|group=nb}}",

  // Minimized repro from Anarchy.wikitext: nested templates plus links/quotes in Refn value
  "{{Refn|In ''Anarchism: From Theory to Practice'' (1970),{{Sfn|Guérin|1970|p=12}} anarchist historian [[Daniel Guérin]] described it as a synonym for [[libertarian socialism]], and wrote that anarchism \"is really a synonym for socialism.\"{{Sfn|Arvidsson|2017}} In his many works on anarchism, historian [[Noam Chomsky]] describes anarchism, alongside [[libertarian Marxism]], as the [[libertarian]] wing of [[socialism]].{{Sfn|Otero|1994|p=617}}|group=nb}}",

  // Complex article excerpt
  "'''Title''' (born [[1970]]) is a [[person]].\n\n== Career ==\n* [[Job A]]\n* [[Job B]]\n\n== References ==\n<references/>",

  // Export repro: title normalization should map [[ẚ]] to link name Aʾ (JS behavior)
  '[[ẚ]]',

  // Export repro: list markers inside template parameter values should tokenize
  "{{Wikisource-inline|list=\n** \"[[s:A Dictionary of the English Language/A|A]]\" in ''[[s:A Dictionary of the English Language|A Dictionary of the English Language]]'' by [[Samuel Johnson]]\n}}",

  // Wikitext repro: gallery caption containing multiple wikilinks must preserve inline spacing
  '<gallery>\nFile:Achilles departure Eretria Painter CdM Paris 851.jpg|Achilles and the [[Nereid]] Cymothoe, Attic [[red-figure]] [[kantharos]] from [[Volci]] ([[Cabinet des Médailles]], Bibliothèque nationale, Paris)\n</gallery>',

  // Wikitext repro (trimmed): multi-line gallery should keep each image as gallery-image token
  '<gallery>\nFile:Achilles departure Eretria Painter CdM Paris 851.jpg|Achilles and the [[Nereid]] Cymothoe\nFile:Akhilleus embassy Staatliche Antikensammlungen 8770.jpg|The embassy to Achilles, Attic red-figure [[hydria]]\n</gallery>',

  // Wikitext repro: parser-function style magic word with ':' should not become template
  '(${{formatnum:{{Inflation|US|800|1861|r=-2}}}} in current dollars)',

  // Wikitext repro: imagemap should produce imagemap-image / imagemap-link structure
  '<imagemap>\nFile:Emancipation proclamation.jpg|thumb|upright=1.25|\'\'[[First Reading of the Emancipation Proclamation of President Lincoln]]\'\'|alt=A dark-haired, bearded, middle-aged man holding documents is seated among seven other men.\npoly 269 892 254 775 193 738 [[Edwin M. Stanton|Edwin Stanton]]\n</imagemap>',

  // BEGIN: auto-generated post-processing parity failures (234 cases)
  "<ref>https://example.org/a</ref>",
  "<ref>RFC 2119</ref>",
  "<ref>----</ref>",
  "<ref>-{zh-hans:简;zh-hant:繁;}-</ref>",
  "== https://example.org/a ==",
  "== RFC 2119 ==",
  "== -{zh-hans:简;zh-hant:繁;}- ==",
  "{{T|v=RFC 2119}}",
  "{{T|v=__NOTOC__}}",
  "{{T|v=-{zh-hans:简;zh-hant:繁;}-}}",
  "{|\n| https://example.org/a\n|}",
  "{|\n| RFC 2119\n|}",
  "{|\n| -{zh-hans:简;zh-hant:繁;}-\n|}",
  "A -{zh-hans:简;zh-hant:繁;}- B",
  "<ref>{{C|title=RFC 2119}}</ref>",
  "<ref>{{C|title=__NOTOC__}}</ref>",
  "<ref>{{C|title=-{zh-hans:简;zh-hant:繁;}-}}</ref>",
  "<ref>plain ; https://example.org/a</ref>",
  "<ref>plain ; RFC 2119</ref>",
  "<ref>plain ; -{zh-hans:简;zh-hant:繁;}-</ref>",
  "<ref>''it'' ; -{zh-hans:简;zh-hant:繁;}-</ref>",
  "<ref>'''bo''' ; -{zh-hans:简;zh-hant:繁;}-</ref>",
  "<ref>[[L|t]] ; -{zh-hans:简;zh-hant:繁;}-</ref>",
  "<ref>[http://example.com x] ; -{zh-hans:简;zh-hant:繁;}-</ref>",
  "<ref>https://example.org/a ; plain</ref>",
  "<ref>https://example.org/a ; https://example.org/a</ref>",
  "<ref>https://example.org/a ; RFC 2119</ref>",
  "<ref>https://example.org/a ; -{zh-hans:简;zh-hant:繁;}-</ref>",
  "<ref>https://example.org/a ; * item</ref>",
  "<ref>RFC 2119 ; plain</ref>",
  "<ref>RFC 2119 ; https://example.org/a</ref>",
  "<ref>RFC 2119 ; RFC 2119</ref>",
  "<ref>RFC 2119 ; -{zh-hans:简;zh-hant:繁;}-</ref>",
  "<ref>RFC 2119 ; * item</ref>",
  "<ref>CO<sub>2</sub> ; -{zh-hans:简;zh-hant:繁;}-</ref>",
  "<ref>a<!--c-->b ; -{zh-hans:简;zh-hant:繁;}-</ref>",
  "<ref>__NOTOC__ ; -{zh-hans:简;zh-hant:繁;}-</ref>",
  "<ref>---- ; plain</ref>",
  "<ref>---- ; ''it''</ref>",
  "<ref>---- ; '''bo'''</ref>",
  "<ref>---- ; [[L|t]]</ref>",
  "<ref>---- ; [http://example.com x]</ref>",
  "<ref>---- ; https://example.org/a</ref>",
  "<ref>---- ; RFC 2119</ref>",
  "<ref>---- ; CO<sub>2</sub></ref>",
  "<ref>---- ; a<!--c-->b</ref>",
  "<ref>---- ; __NOTOC__</ref>",
  "<ref>---- ; ----</ref>",
  "<ref>---- ; -{zh-hans:简;zh-hant:繁;}-</ref>",
  "<ref>---- ; * item</ref>",
  "<ref>---- ; {{T|x=y}}</ref>",
  "<ref>-{zh-hans:简;zh-hant:繁;}- ; plain</ref>",
  "<ref>-{zh-hans:简;zh-hant:繁;}- ; ''it''</ref>",
  "<ref>-{zh-hans:简;zh-hant:繁;}- ; '''bo'''</ref>",
  "<ref>-{zh-hans:简;zh-hant:繁;}- ; [[L|t]]</ref>",
  "<ref>-{zh-hans:简;zh-hant:繁;}- ; [http://example.com x]</ref>",
  "<ref>-{zh-hans:简;zh-hant:繁;}- ; https://example.org/a</ref>",
  "<ref>-{zh-hans:简;zh-hant:繁;}- ; RFC 2119</ref>",
  "<ref>-{zh-hans:简;zh-hant:繁;}- ; CO<sub>2</sub></ref>",
  "<ref>-{zh-hans:简;zh-hant:繁;}- ; a<!--c-->b</ref>",
  "<ref>-{zh-hans:简;zh-hant:繁;}- ; __NOTOC__</ref>",
  "<ref>-{zh-hans:简;zh-hant:繁;}- ; ----</ref>",
  "<ref>-{zh-hans:简;zh-hant:繁;}- ; -{zh-hans:简;zh-hant:繁;}-</ref>",
  "<ref>-{zh-hans:简;zh-hant:繁;}- ; * item</ref>",
  "<ref>-{zh-hans:简;zh-hant:繁;}- ; {{T|x=y}}</ref>",
  "<ref>* item ; ''it''</ref>",
  "<ref>* item ; '''bo'''</ref>",
  "<ref>* item ; [[L|t]]</ref>",
  "<ref>* item ; [http://example.com x]</ref>",
  "<ref>* item ; https://example.org/a</ref>",
  "<ref>* item ; RFC 2119</ref>",
  "<ref>* item ; CO<sub>2</sub></ref>",
  "<ref>* item ; a<!--c-->b</ref>",
  "<ref>* item ; __NOTOC__</ref>",
  "<ref>* item ; ----</ref>",
  "<ref>* item ; -{zh-hans:简;zh-hant:繁;}-</ref>",
  "<ref>* item ; {{T|x=y}}</ref>",
  "<ref>{{T|x=y}} ; -{zh-hans:简;zh-hant:繁;}-</ref>",
  "{{T|v=plain ; RFC 2119}}",
  "{{T|v=plain ; __NOTOC__}}",
  "{{T|v=plain ; -{zh-hans:简;zh-hant:繁;}-}}",
  "{{T|v=''it'' ; CO<sub>2</sub>}}",
  "{{T|v=''it'' ; __NOTOC__}}",
  "{{T|v=''it'' ; -{zh-hans:简;zh-hant:繁;}-}}",
  "{{T|v='''bo''' ; CO<sub>2</sub>}}",
  "{{T|v='''bo''' ; __NOTOC__}}",
  "{{T|v='''bo''' ; -{zh-hans:简;zh-hant:繁;}-}}",
  "{{T|v=[[L|t]] ; CO<sub>2</sub>}}",
  "{{T|v=[[L|t]] ; __NOTOC__}}",
  "{{T|v=[[L|t]] ; -{zh-hans:简;zh-hant:繁;}-}}",
  "{{T|v=[http://example.com x] ; CO<sub>2</sub>}}",
  "{{T|v=[http://example.com x] ; __NOTOC__}}",
  "{{T|v=[http://example.com x] ; -{zh-hans:简;zh-hant:繁;}-}}",
  "{{T|v=https://example.org/a ; CO<sub>2</sub>}}",
  "{{T|v=https://example.org/a ; __NOTOC__}}",
  "{{T|v=https://example.org/a ; -{zh-hans:简;zh-hant:繁;}-}}",
  "{{T|v=RFC 2119 ; plain}}",
  "{{T|v=RFC 2119 ; RFC 2119}}",
  "{{T|v=RFC 2119 ; CO<sub>2</sub>}}",
  "{{T|v=RFC 2119 ; a<!--c-->b}}",
  "{{T|v=RFC 2119 ; __NOTOC__}}",
  "{{T|v=RFC 2119 ; ----}}",
  "{{T|v=RFC 2119 ; -{zh-hans:简;zh-hant:繁;}-}}",
  "{{T|v=RFC 2119 ; * item}}",
  "{{T|v=RFC 2119 ; {{T|x=y}}}}",
  "{{T|v=CO<sub>2</sub> ; ''it''}}",
  "{{T|v=CO<sub>2</sub> ; '''bo'''}}",
  "{{T|v=CO<sub>2</sub> ; [[L|t]]}}",
  "{{T|v=CO<sub>2</sub> ; [http://example.com x]}}",
  "{{T|v=CO<sub>2</sub> ; https://example.org/a}}",
  "{{T|v=CO<sub>2</sub> ; RFC 2119}}",
  "{{T|v=CO<sub>2</sub> ; __NOTOC__}}",
  "{{T|v=CO<sub>2</sub> ; -{zh-hans:简;zh-hant:繁;}-}}",
  "{{T|v=a<!--c-->b ; RFC 2119}}",
  "{{T|v=a<!--c-->b ; __NOTOC__}}",
  "{{T|v=a<!--c-->b ; -{zh-hans:简;zh-hant:繁;}-}}",
  "{{T|v=__NOTOC__ ; plain}}",
  "{{T|v=__NOTOC__ ; ''it''}}",
  "{{T|v=__NOTOC__ ; '''bo'''}}",
  "{{T|v=__NOTOC__ ; [[L|t]]}}",
  "{{T|v=__NOTOC__ ; [http://example.com x]}}",
  "{{T|v=__NOTOC__ ; https://example.org/a}}",
  "{{T|v=__NOTOC__ ; RFC 2119}}",
  "{{T|v=__NOTOC__ ; CO<sub>2</sub>}}",
  "{{T|v=__NOTOC__ ; a<!--c-->b}}",
  "{{T|v=__NOTOC__ ; __NOTOC__}}",
  "{{T|v=__NOTOC__ ; ----}}",
  "{{T|v=__NOTOC__ ; -{zh-hans:简;zh-hant:繁;}-}}",
  "{{T|v=__NOTOC__ ; * item}}",
  "{{T|v=__NOTOC__ ; {{T|x=y}}}}",
  "{{T|v=---- ; RFC 2119}}",
  "{{T|v=---- ; __NOTOC__}}",
  "{{T|v=---- ; -{zh-hans:简;zh-hant:繁;}-}}",
  "{{T|v=-{zh-hans:简;zh-hant:繁;}- ; plain}}",
  "{{T|v=-{zh-hans:简;zh-hant:繁;}- ; ''it''}}",
  "{{T|v=-{zh-hans:简;zh-hant:繁;}- ; '''bo'''}}",
  "{{T|v=-{zh-hans:简;zh-hant:繁;}- ; [[L|t]]}}",
  "{{T|v=-{zh-hans:简;zh-hant:繁;}- ; [http://example.com x]}}",
  "{{T|v=-{zh-hans:简;zh-hant:繁;}- ; https://example.org/a}}",
  "{{T|v=-{zh-hans:简;zh-hant:繁;}- ; RFC 2119}}",
  "{{T|v=-{zh-hans:简;zh-hant:繁;}- ; CO<sub>2</sub>}}",
  "{{T|v=-{zh-hans:简;zh-hant:繁;}- ; a<!--c-->b}}",
  "{{T|v=-{zh-hans:简;zh-hant:繁;}- ; __NOTOC__}}",
  "{{T|v=-{zh-hans:简;zh-hant:繁;}- ; ----}}",
  "{{T|v=-{zh-hans:简;zh-hant:繁;}- ; -{zh-hans:简;zh-hant:繁;}-}}",
  "{{T|v=-{zh-hans:简;zh-hant:繁;}- ; * item}}",
  "{{T|v=-{zh-hans:简;zh-hant:繁;}- ; {{T|x=y}}}}",
  "{{T|v=* item ; RFC 2119}}",
  "{{T|v=* item ; __NOTOC__}}",
  "{{T|v=* item ; -{zh-hans:简;zh-hant:繁;}-}}",
  "{{blockquote|<ref name=\"vra\">x [http://www.protectcivilrights.org/pdf/voting/AlabamaVRA.pdf ''Voting Rights in Alabama (1982–2006)''] {{Webarchive|url=x|date=y}} z</ref>}}",
  "{{T|v={{T|x=y}} ; RFC 2119}}",
  "{{T|v={{T|x=y}} ; __NOTOC__}}",
  "{{T|v={{T|x=y}} ; -{zh-hans:简;zh-hant:繁;}-}}",
  "{|\n| plain ; https://example.org/a\n|}",
  "{|\n| plain ; RFC 2119\n|}",
  "{|\n| plain ; -{zh-hans:简;zh-hant:繁;}-\n|}",
  "{|\n| ''it'' ; CO<sub>2</sub>\n|}",
  "{|\n| ''it'' ; a<!--c-->b\n|}",
  "{|\n| ''it'' ; -{zh-hans:简;zh-hant:繁;}-\n|}",
  "{|\n| ''it'' ; {{T|x=y}}\n|}",
  "{|\n| '''bo''' ; CO<sub>2</sub>\n|}",
  "{|\n| '''bo''' ; a<!--c-->b\n|}",
  "{|\n| '''bo''' ; -{zh-hans:简;zh-hant:繁;}-\n|}",
  "{|\n| '''bo''' ; {{T|x=y}}\n|}",
  "{|\n| [[L|t]] ; CO<sub>2</sub>\n|}",
  "{|\n| [[L|t]] ; a<!--c-->b\n|}",
  "{|\n| [[L|t]] ; -{zh-hans:简;zh-hant:繁;}-\n|}",
  "{|\n| [[L|t]] ; {{T|x=y}}\n|}",
  "{|\n| [http://example.com x] ; CO<sub>2</sub>\n|}",
  "{|\n| [http://example.com x] ; a<!--c-->b\n|}",
  "{|\n| [http://example.com x] ; -{zh-hans:简;zh-hant:繁;}-\n|}",
  "{|\n| [http://example.com x] ; {{T|x=y}}\n|}",
  "{|\n| https://example.org/a ; plain\n|}",
  "{|\n| https://example.org/a ; https://example.org/a\n|}",
  "{|\n| https://example.org/a ; RFC 2119\n|}",
  "{|\n| https://example.org/a ; CO<sub>2</sub>\n|}",
  "{|\n| https://example.org/a ; a<!--c-->b\n|}",
  "{|\n| https://example.org/a ; -{zh-hans:简;zh-hant:繁;}-\n|}",
  "{|\n| https://example.org/a ; * item\n|}",
  "{|\n| https://example.org/a ; {{T|x=y}}\n|}",
  "{|\n| RFC 2119 ; plain\n|}",
  "{|\n| RFC 2119 ; https://example.org/a\n|}",
  "{|\n| RFC 2119 ; RFC 2119\n|}",
  "{|\n| RFC 2119 ; CO<sub>2</sub>\n|}",
  "{|\n| RFC 2119 ; a<!--c-->b\n|}",
  "{|\n| RFC 2119 ; -{zh-hans:简;zh-hant:繁;}-\n|}",
  "{|\n| RFC 2119 ; * item\n|}",
  "{|\n| RFC 2119 ; {{T|x=y}}\n|}",
  "{|\n| CO<sub>2</sub> ; ''it''\n|}",
  "{|\n| CO<sub>2</sub> ; '''bo'''\n|}",
  "{|\n| CO<sub>2</sub> ; [[L|t]]\n|}",
  "{|\n| CO<sub>2</sub> ; [http://example.com x]\n|}",
  "{|\n| CO<sub>2</sub> ; https://example.org/a\n|}",
  "{|\n| CO<sub>2</sub> ; RFC 2119\n|}",
  "{|\n| CO<sub>2</sub> ; __NOTOC__\n|}",
  "{|\n| CO<sub>2</sub> ; -{zh-hans:简;zh-hant:繁;}-\n|}",
  "{|\n| a<!--c-->b ; ''it''\n|}",
  "{|\n| a<!--c-->b ; '''bo'''\n|}",
  "{|\n| a<!--c-->b ; [[L|t]]\n|}",
  "{|\n| a<!--c-->b ; [http://example.com x]\n|}",
  "{|\n| a<!--c-->b ; https://example.org/a\n|}",
  "{|\n| a<!--c-->b ; RFC 2119\n|}",
  "{|\n| a<!--c-->b ; __NOTOC__\n|}",
  "{|\n| a<!--c-->b ; -{zh-hans:简;zh-hant:繁;}-\n|}",
  "{|\n| __NOTOC__ ; CO<sub>2</sub>\n|}",
  "{|\n| __NOTOC__ ; a<!--c-->b\n|}",
  "{|\n| __NOTOC__ ; -{zh-hans:简;zh-hant:繁;}-\n|}",
  "{|\n| __NOTOC__ ; {{T|x=y}}\n|}",
  "{|\n| ---- ; -{zh-hans:简;zh-hant:繁;}-\n|}",
  "{|\n| -{zh-hans:简;zh-hant:繁;}- ; plain\n|}",
  "{|\n| -{zh-hans:简;zh-hant:繁;}- ; ''it''\n|}",
  "{|\n| -{zh-hans:简;zh-hant:繁;}- ; '''bo'''\n|}",
  "{|\n| -{zh-hans:简;zh-hant:繁;}- ; [[L|t]]\n|}",
  "{|\n| -{zh-hans:简;zh-hant:繁;}- ; [http://example.com x]\n|}",
  "{|\n| -{zh-hans:简;zh-hant:繁;}- ; https://example.org/a\n|}",
  "{|\n| -{zh-hans:简;zh-hant:繁;}- ; RFC 2119\n|}",
  "{|\n| -{zh-hans:简;zh-hant:繁;}- ; CO<sub>2</sub>\n|}",
  "{|\n| -{zh-hans:简;zh-hant:繁;}- ; a<!--c-->b\n|}",
  "{|\n| -{zh-hans:简;zh-hant:繁;}- ; __NOTOC__\n|}",
  "{|\n| -{zh-hans:简;zh-hant:繁;}- ; ----\n|}",
  "{|\n| -{zh-hans:简;zh-hant:繁;}- ; -{zh-hans:简;zh-hant:繁;}-\n|}",
  "{|\n| -{zh-hans:简;zh-hant:繁;}- ; * item\n|}",
  "{|\n| -{zh-hans:简;zh-hant:繁;}- ; {{T|x=y}}\n|}",
  "{|\n| * item ; https://example.org/a\n|}",
  "{|\n| * item ; RFC 2119\n|}",
  "{|\n| * item ; -{zh-hans:简;zh-hant:繁;}-\n|}",
  "{|\n| {{T|x=y}} ; ''it''\n|}",
  "{|\n| {{T|x=y}} ; '''bo'''\n|}",
  "{|\n| {{T|x=y}} ; [[L|t]]\n|}",
  "{|\n| {{T|x=y}} ; [http://example.com x]\n|}",
  "{|\n| {{T|x=y}} ; https://example.org/a\n|}",
  "{|\n| {{T|x=y}} ; RFC 2119\n|}",
  "{|\n| {{T|x=y}} ; __NOTOC__\n|}",
  "{|\n| {{T|x=y}} ; -{zh-hans:简;zh-hant:繁;}-\n|}",
  // END: auto-generated post-processing parity failures
], { name: 'pipeline' });