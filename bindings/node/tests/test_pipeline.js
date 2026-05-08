#!/usr/bin/env node
'use strict';
// Parity test: full 11-stage parse pipeline on combined samples.
// These exercises multiple stages interacting at once.
const path = require('path');
const { runTests } = require('./helpers');
const { newProto, nativeProto } = require('./native_token_patch');

// This should loop through each test and fast fail on the first mismatch.

const tests = [
  `[[File:A map of the descendants of Abu Bakr al-Siddiq.svg | alt=A map showing the 
  ]]`,
  `<ref name="https://pt.scribd.com/document/742344591/smartproxy-cities"/>`,
  `[[Mushroom poisoning|deadly mushrooms]]`,
  `; A'''B''' : C`,
  `; A''B'' : C`,
  `; A'''''B''''' : C`,
  `; outer '''bold : text''' end`,
  `＿＿目次＿＿`,
  `text ＿＿目次＿＿ more text`,
  `  ＿＿目次＿＿  `,
  '== Heading == ',
  `== Heading ==\xC2\xA0`,
  `== Heading ==\xE3\x80\x80`,
  `== Normal Heading ==`,
  `=== Nested ===  `,
  `== Heading\xC2\xA0text ==`,
  `[[/Sub]]`,
  `[[/Sub|display text]]`,
  `[[../Sibling]]`,
  `[[#Section]]`,
  `{{Template}} and [[/Subpage]]`,
  `#REDIRECT [[WP:Foo]]`,
  `#REDIRECT [[Project:Shortcut]]`,
  `#REDIRECT [[en:English article]]`,
  `#REDIRECT [[Target page|display]]`,
  `[[File:Foo.jpg|link=<bad>|right|Caption]]`,
  `[[File:Foo.jpg|notapixelvalue|right|Caption]]`,
  `[[File:Foo.jpg|200px|right|Caption]]`,
  `[[File:Foo.jpg|200x150px]]`,
  `[[File:Diagram.svg|lang=zh]]`,
  `[[File:Photo.jpg|lang=zh]]`,
  `[[File:Book.djvu|page=3]]`,
  `[[File:Photo.jpg|page=3]]`,
  `[[File:Foo.jpg|link=Main Page]]`,
  `[[File:Foo.jpg|link=]]`,
  `[[File:Foo.jpg|border|200px|right|Caption]]`,
  `-{zh:简体;zh-hant:繁體}-`,
  `Before -{A|B}- after`,
  `{{T|-{A|B}-}}`,
  `<ref>-{A|B}-</ref>`,
  `<gallery>\nFile:Foo.jpg|Caption <!-- hidden --> text\n</gallery>`,
  `<gallery>\nFile:Foo.jpg|''italic'' and '''bold''' caption\n</gallery>`,
  `<gallery>\nFile:Foo.jpg|See [http://example.com link]\n</gallery>`,
  `<gallery>\nFile:Foo.jpg|See RFC 2119\n</gallery>`,
  `<gallery>\nFile:Foo.jpg|{{Template|arg}} caption\n</gallery>`,
  `<imagemap>\nFile:Foo.jpg|thumb|''italic'' caption\npoly 0 0 10 10 [[Link]]\n</imagemap>`,
  `<imagemap>\nFile:Foo.jpg|See RFC 2119\npoly 0 0 10 10 [[Link]]\n</imagemap>`,
  `<poem>\n* First item\n* Second item\n</poem>`,
  `<poem>* First item\n* Second item</poem>`,
  `<poem>\n# Numbered first\n# Numbered second\n</poem>`,
  `<poem>\n; Term\n: Definition\n</poem>`,
  `[[{{okina}}Iolani Palace]]`,
  `[[/|\]] (Back slash)`,
  `<ref name="The Pulitzer Prizes {{pipe}} Poetry">{{Cite web |title=The Pulitzer Prizes {{pipe}} Poetry |url=http://www.pulitzer.org/bycat/Poetry |access-date=October 31, 2010 |publisher=Pulitzer.org}}</ref>`,
  `{{#tag:something|{{#expr:1+1}}|{{#time:Y}}}}`,
  `<gallery caption="Prints from [[Bernard de Montfaucon]]'s ''L'antiquité expliquée et représentée en figures'' (Band 2,2) page 358 ff.">
  File:Montfaucon Abraxas Plaque 149.xcf|Plaque 149
  </gallery>`,
  `{{#tag:timeline|
  ImageSize  = width:1500 height:auto barincrement:18
  PlotArea   = top:10 bottom:20 right:130 left:10
  AlignBars  = late
  DateFormat = x.y
  Period     = from:1816.90 till:{{#expr:{{#time:Y}}+{{#time:m}}/6}}
  TimeAxis   = orientation:horizontal
  ScaleMajor = unit:year increment:10 start:1820
  ScaleMinor = unit:year increment:1 start:1817

  Define $now = {{#expr:{{#time:Y}}+{{#time:m}}/12}}

  Colors =
    id:5year   value:rgb(0.8, 0.8, 0.8)

  BarData =
    barset:GovernorLine
    barset:Governors
    #barset:blankline

  PlotData=
  width:1 align:right fontsize:S shift:(-3,-4) anchor:from fontsize:8 color:black

  barset:GovernorLine
  from:1832 till:end text:Governors

  width:6 align:left fontsize:S shift:(5,-4) anchor:till fontsize:10

  barset:Governors
  from:1819.86 till:1820.52 color:demrep text:"William W. Bibb"
  from:1820.52 till:1821.86 color:demrep text:"Thomas Bibb"
  from:1821.86 till:1825.9 color:demrep text:"Israel Pickens"
  from:1825.9 till:1829.89 color:jackson text:"John Murphy"

  LineData=
  from:1817.73 till:1819.86 atpos:989 color:noparty width:6 # WWB noparty
  from:1831.9 till:1833 atpos:883 color:jackson width:6 # JG jackson

  layer:back
  # This section creates the vertical lines.
  at:1820.00 width:0.1 color:0year
  at:1825.00 width:0.1 color:5year
  at:1830.00 width:0.1 color:0year
  at:1835.00 width:0.1 color:5year
  at:1840.00 width:0.1 color:0year
  at:1845.00 width:0.1 color:5year
  at:1850.00 width:0.1 color:0year
  }}`,
  `<Gallery>
  File:The Odeon of Herodes Atticus on September 13, 2020.jpg|The [[Odeon of Herodes Atticus]] built in AD 161 by [[Herodes Atticus]]
  </Gallery>`,
  `archive-url=https://web.archive.org/web/20160325000841/http://danmarkshistorien.dk/leksikon-og-kilder/vis/materiale/aarhus-domkirke/?chash=a83dfe233ede9644167a9b5ea9aa35a4&tx_historyview_pi1&#91;lang&#93;=1`,
  ` [[File:Maaloula square alef.svg|20px|]]`,
  `[[File:Uranocene-3D-balls.png|thumb|upright=0.55|Predicted structure of amerocene [(η<sup>8</sup>-C<sub>8</sub>H<sub>8</sub>)<sub>2</sub><nowiki>Am]</nowiki>]]`,
  `|- 1 CAR
  ! 1
  ! rowspan="4" | Position
  | style="background:#dfffdf;" | 4
  | style="background:#dfffdf;" | 4`,
  `{|
  |- 1 CAR
  ! 1
  ! rowspan="4" | Position
  | style="background:#dfffdf;" | 4
  | style="background:#dfffdf;" | 4
  |}`,
  `{|
  |- {{T}} CAR
  |}`,
  `{|
  |- -{zh-hans:a;zh-hant:b;}- CAR
  |}`,
  `{|
  |- <!--c--> 1 CAR
  |}`,
  `== References ==
  {{Reflist}}
  {{refbegin}}
  * {{Brooklands: On Audi & Auto Union 1980|editor-mask=6}}
  {{refend}}
  `,
  `[[ζ Arietis]]`,
  `<imagemap>
  File:Actinopterygii.jpg||300px
  rect 0 0 333 232 [[Electrophorus electricus|Electric eel]]
  </imagemap>`,
  `<imagemap>
  File:Foo.jpg|thumb||alt=A
  poly 1 1 2 2 [[L|t]]
  </imagemap>`,
  `<imagemap>
  File:Foo.jpg|||alt=A
  poly 1 1 2 2 [[L|t]]
  </imagemap>`,
  `<ref>Andy Pease, [http://windliterature.org/2014/07/01/america-the-beautiful-by-katharine-lee-bates-and-samuel-augustus-ward-arr-carmen-dragon/ {{"'}}America the Beautiful' by Katharine Lee Bates and Samuel Augustus Ward, arr. Carmen Dragon"] ({{webarchive|url=https://web.archive.org/web/20180222162222/http://windliterature.org/2014/07/01/america-the-beautiful-by-katharine-lee-bates-and-samuel-augustus-ward-arr-carmen-dragon/ |date=February 22, 2018}}), Wind Band Literature, July 1, 2014; accessed 2019-08-17.</ref>`,
  `{| class="wikitable sortable"
  ! colspan="3" |[[File:Diplomatic_relations_of_Angola.svg|frameless|425x425px]]
  |-
  !#
  !Country
  !Date
  |-
  |174
  |{{Flag|Bahamas}}
  |{{dts|26 September 2025}}<ref>{{Cite web |date=26 September 2025 |title=We established diplomatic relations with Angola by signing a memorandum to that effect in New York at the United Nations. |url=https://www.facebook.com/fredmitchellmbm/posts/pfbid02KWQbzWBsS1WiabSQRmZJMMBf9Mv6584W6jaTTufWp341bGqqLNKoFie7wdvqc3JYl |access-date=26 September 2025 |website=Fred Mitchell - Minute By Minute on Facebook}}</ref>
  |}`,
  `<gallery mode="packed" caption="Road transport in Angola.">
  Midd Town Luanda.jpg|Automobiles in [[Luanda]].
  The Nowhere road.jpg|New highway (2019).
  </gallery>`,
  `<references>
  <ref name="Schopenhauer-2018">{{cite book |author-last=Schopenhauer |author-first=Arthur |date=2018 |title=The World as Will and Representation |orig-date=1844 |volume=2 |place=Cambridge |publisher=Cambridge University Press |doi=10.1017/9780511843112 |isbn=978-0-521-87034-4 |editor-last=Welchman |editor-first=Alistair |editor2-last=Janaway |editor2-first=Christopher |editor3-last=Norman |editor3-first=Judith |author-link1=Arthur Schopenhauer |title-link=The World as Will and Representation}}</ref>
  </references>`,
  `
  | style="text-align:center;"| [[File:Emblem of Qatar-2022.svg|20px|Link=Emblem of Qatar|alt=Emblem]]`,
  `|<poem style="margin-left:1em;">1911 version<ref>{{cite book |url=https://archive.org/details/americabeautiful00baterich |last=Bates |first=Katharine Lee |date=1911 |title=America the Beautiful and Other Poems |location=New York |publisher=Thomas Y. Crowell Company |pages=3–4 |via=archive.org}}</ref>sea!</poem>|}`,
  `{{Hatnote group|
  {{Redirect|Materna||Materna (disambiguation)|and|America the Beautiful (disambiguation)}}
  {{}}
  }}
  `,
  `{| class="wikitable" style="text-align:center"
  ! pentane || 2-methylbutane || 2,2-dimethylpropane
  |}`,
  `{{Cite web <!-- Citation bot changes to journal -->|url=https://www.cdc.gov/niosh/docs/2012-108/ |title=NIOSH Pesticide Poisoning Monitoring Program Protects Farmworkers |publisher=[[Centers for Disease Control and Prevention]] |access-date=15 April 2013 |url-status=live |archive-url=https://web.archive.org/web/20130402004253/http://www.cdc.gov/niosh/docs/2012%2D108/ |archive-date=2 April 2013|doi=10.26616/NIOSHPUB2012108 |year=2011 |doi-access=free}}`,
  `[[File:Andorra - panoramio (2).jpg|thumb|Streets of the city centre of Andorra la Vella in 1986. From 1986 until 1989 Andorra normalised the economic treaties with the [[European Economic Community|EEC]].<ref>{{cite news |date=18 December 1989 |title=La CE concluye un acuerdo de unión aduanera con Andorra |trans-title=The EC concludes a customs union agreement with Andorra |url=https://elpais.com/diario/1989/12/18/economia/629938809_850215.html |newspaper=El País |language=es}}</ref><ref>{{cite news |date=27 September 1986 |title=François Mitterrand alienta las reformas en Andorras |trans-title=François Mitterrand encourages reforms in Andorra |url=https://elpais.com/diario/1986/09/27/internacional/528156020_850215.html |newspaper=El País |language=es}}</ref>|alt=]]`,
  `{|{{chset-table-header1|ASCII (1977/1986)}}| {{chset-cell1 | 124 U+007C: VERTICAL LINE | [[Vertical bar|{{pipe}}]] | style=background:#ffb2b2}}|}`,
  `{|{{chset-table-header1|ASCII (1977/1986)}}
  | 
  }`,
  `{|{{chset-table-header1|ASCII (1977/1986)}}|}`,
  `{{chset-cell1 | 124 U+007C: VERTICAL LINE | [[Vertical bar|{{pipe}}]] | style=background:#ffb2b2}}`,
  `{{chset-cell1 | 61 U+003D: EQUALS SIGN | [[=]] }}`,
  `| {{chset-cell1 | 123 U+007B: LEFT CURLY BRACKET | [[Left curly bracket|{]] | style=background:#ffffb2}}`,
  'This is a paragraph of plain text.',
  '#REDIRECT [[Target]] <!-- a comment -->',
  '{{Template|[[Page|label]]}}',
  '== {{PAGENAME}} ==\nContent.',
  '{|\n| [[Page|link]] || plain\n|}',
  '{|\n| alias = ISO-IR-006,<ref>reftext</ref> ANSI_X3.4-1968\n|}',
  '{|\n! Modern [[State of matter|state<br />of matter]]\n|}',
  '{{Outer|{{Inner|arg}}}}',
  "'''bold''' and ''[[Page|italic link]]''",
  '[[Page]] and [http://example.com external].',
  "See RFC 2119, [[Specification]] and http://example.com.",
  '* {{Template}}\n* plain item\n* [[Page]]',
  "== Before ==\n\n----\n\n== After ==",
  '== Relationship to surface [[bulk density]] ==',
  'Before __NOTOC__ after.',
  '{{Template|<!-- comment -->value}}',
  '<nowiki>[[not a link]] {{not a template}}</nowiki>',
  'Claim.<ref>{{Citation|title=Foo|year=2020}}</ref> More text.',
  "A<ref>''x'' efficacy is > <sub>2</sub>; ''y'' is > <sub>2</sub>.</ref>B",
  'A<ref>alpha<!--c--></ref>B',
  'A<ref>{{Citation|access-date= 29 June 2011<!--Added by DASHBot-->}}</ref>B',
  "{{T|v='''a\n''b'''}}",
  '<score raw=1 sound=1>\\relative c\'\' { x }</score>',
  '<ref>\n* a\n* b\n</ref>',
  "{{Blockquote|<poem><ref>x ''L'Etoile'' (as in ''H'')</ref></poem>}}",
  '{{Citation|title=Effect of land albedo, CO<sub>2</sub>, orography}}',
  "{{Refn|In ''Anarchism: From Theory to Practice'' (1970), historian [[Daniel Guérin]] describes [[libertarian socialism]].|group=nb}}",
  "{{Refn|In ''Anarchism: From Theory to Practice'' (1970),{{Sfn|Guérin|1970|p=12}} anarchist historian [[Daniel Guérin]] described it as a synonym for [[libertarian socialism]], and wrote that anarchism \"is really a synonym for socialism.\"{{Sfn|Arvidsson|2017}} In his many works on anarchism, historian [[Noam Chomsky]] describes anarchism, alongside [[libertarian Marxism]], as the [[libertarian]] wing of [[socialism]].{{Sfn|Otero|1994|p=617}}|group=nb}}",
  "'''Title''' (born [[1970]]) is a [[person]].\n\n== Career ==\n* [[Job A]]\n* [[Job B]]\n\n== References ==\n<references/>",
  '[[ẚ]]',
  "{{Wikisource-inline|list=\n** \"[[s:A Dictionary of the English Language/A|A]]\" in ''[[s:A Dictionary of the English Language|A Dictionary of the English Language]]'' by [[Samuel Johnson]]\n}}",
  '<gallery>\nFile:Achilles departure Eretria Painter CdM Paris 851.jpg|Achilles and the [[Nereid]] Cymothoe, Attic [[red-figure]] [[kantharos]] from [[Volci]] ([[Cabinet des Médailles]], Bibliothèque nationale, Paris)\n</gallery>',
  '<gallery>\nFile:Achilles departure Eretria Painter CdM Paris 851.jpg|Achilles and the [[Nereid]] Cymothoe\nFile:Akhilleus embassy Staatliche Antikensammlungen 8770.jpg|The embassy to Achilles, Attic red-figure [[hydria]]\n</gallery>',
  '(${{formatnum:{{Inflation|US|800|1861|r=-2}}}} in current dollars)',
  '<imagemap>\nFile:Emancipation proclamation.jpg|thumb|upright=1.25|\'\'[[First Reading of the Emancipation Proclamation of President Lincoln]]\'\'|alt=A dark-haired, bearded, middle-aged man holding documents is seated among seven other men.\npoly 269 892 254 775 193 738 [[Edwin M. Stanton|Edwin Stanton]]\n</imagemap>',
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
  "{{#vardefine:x|1}}",
  "{{#vardefine:x|{{T|y}}}}",
  "{{#iferror:<strong class=\"error\">x</strong>|bad|ok}}",
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
  '[[Image:Foo.jpg|caption [[Link]] text]]',
  '[[Image:Foo.jpg|upright|caption [[Link]] text]]',
  '<imagemap>\n\nFile:Foo.jpg|thumb|alt=A\npoly 1 1 2 2 [[L|{{T|x}}]]\npoly 3 3 4 4 [[L2]]\n\n</imagemap>',
  '<pre>{{T|x=[[L|t]]}}</pre>',
  '<hiero>{{T|x=[[L|t]]}}</hiero>',
  '<categorytree>Category:Physics</categorytree>',
  '<categorytree>[[Category:Physics|Physics]]</categorytree>',
  '<categorytree>{{T|x=[[L|t]]}}</categorytree>',
];

const ROOT = path.resolve(__dirname, '..', '..', '..');
const CONFIGS = ['enwiki', 'jawiki', 'llwiki'];

for (const configName of CONFIGS) {
  const configPath = path.join(ROOT, 'config', `${configName}.json`);
  process.env.WIKI_CONFIG = configPath;
  newProto.config = configPath;
  nativeProto.config = configPath;

  for (const test of tests) {
    const ok = runTests([test], { name: `pipeline-${configName}` });
    if (!ok) {
      process.exit(1);
    }
  }
}

