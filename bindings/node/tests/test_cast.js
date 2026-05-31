#!/usr/bin/env node
'use strict';
// Parity test: templates, arguments, section headings (stage 1 – parseBraces).
const { runTests } = require('./helpers');

const castTests = [
  // Failures from export of wikitexts
// `{| class="wikitable" width="100%"
// ! style="width:6%" | Event
// |-
// | [[Shulaveri-Shomu culture|Shulaveri]]: a late Neolithic/Eneolithic culture that existed on the territory of present-day Georgia, Azerbaijan and the Armenian Highlands. The culture is dated to mid-6th or early-5th millennia BC and is thought to be one of the earliest known Neolithic cultures; started in c.6000 BC and lasted until 4000 BC.
// |-
// |- Some of the earliest known traces of [[wine]] are found in Georgia, dating to c. 6000 BC.
// |}`,
//   `{{Cite tweet |user=CoryBooker |number=1255659700739284996 |title=I hope you'll support @TriciaforWI for Congress. She's running in a tough special election in Wisconsin on May 12th against an opponent who's been endorsed by Donald Trump. Let's make sure Tricia flips this seat. Chip in to her campaign today: https://secure.actblue.com/donate/tzcb&nbsp;https://twitter.com/emilyslist/status/1255120659727990785&nbsp;…|first=Cory|last=Booker|date=April 29, 2020|access-date=May 13, 2020}}`,
// `<gallery mode=packed heights=300>
// File:Colliers Wood London 2011 07.jpg|alt=Destinations of CS7 in the style of a tube line, on a large upright sign.|Cycle Superhighway CS7 start point at [[Colliers Wood]] Underground Station]
// File:Milton Keynes Redway.gif|Cycleway network in Milton Keynes. NCR routes 6 and 51 are highlighted in red. In 1970 in the United Kingdom, the [[Milton Keynes Development Corporation]] produced the [[History of Milton Keynes#Milton Keynes Development Corporation: designing a city for 250,000 people|"Master Plan for Milton Keynes"]].<ref>[http://www.mkweb.co.uk/milton_keynes_general/displayarticle.asp?id=285 Modern Milton Keynes: the master plan] {{Webarchive|url=https://web.archive.org/web/20120728111624/http://www.mkweb.co.uk/milton_keynes_general/displayarticle.asp?ID=285 |date=28 July 2012 }} MK Web</ref>
// File:Biketrail map2detail.jpg| A [[locator map]] on the [[numbered-node cycle network]], at node 20 in [[Schoten]], near [[Antwerp]]. Each intersection is marked with a numbered circle; any route through the network can be represented as a string of numbers.
// </gallery>`,
// `{{SAFESUBST:}} IT\ICCU\VEAV\\045592`,
 `'''[[Classic of Poetry]]'', "Airs of the States - Airs of [[Wey (state)|Wey]] - [https://ctext.org/book-of-poetry/bo-xi?searchu=%E4%BC%AF%E5%85%AE%E6%9C%85%E5%85%AE%E3%80%81%E9%82%A6%E4%B9%8B%E6%A1%80%E5%85%AE%E3%80%82&searchmode=showall#result Bo Xi]". quote:「伯兮'''朅'''兮、邦之'''桀'''兮。」; [[James Legge]]'s translation: "My noble husband is how '''martial-like'''! The '''hero''' of the country!"; [[Zhu Xi]], ''Collected Commentaries on the Classic of Poetry'', "vol. 2", quote: 「'''朅'''，武貌。'''桀'''，才過人也。」. [https://ctext.org/library.pl?if=en&file=9214&page=119#%E6%9C%85%E6%AD%A6%E8%B2%8C%E6%A1%80%E6%89%8D%E9%81%8E%E4%BA%BA%E4%B9%9F p. 119 of 141]`,
 `<ref>Scruton, Roger (1999). The Aesthetics of Music. Oxford University Press. Print ISBN 9780198167273.</ref>`,
 `{{Tree chart|border=no | | | | | | | | | | | | | | | | | | | | | | | | | [[File:Coat of arms of Republic of Venice.svg|93px]]|[[File:Coat of arms of Republic of Venice.svg|93px]]=[[File:Coat of arms of Republic of Venice.svg|93px]]}}`,
 `{{code|lang=html|code=<font [color=<var>color</var>] [size=<var>size</var>] [face=<var>face</var>]>...</font>}}`,
 `{{ubli|[[Grand Jury (Ireland) Act 1837]]|[[Coroners (Ireland) Act 1846]]|[[Grand Jury Cess (Ireland) Act 1848]]|[[Summary Jurisdiction (Ireland) Act 1850]]|[[Grand Jury (Ireland) Act 1857]]|[[County Surveyors, &c. (Ireland) Act 1861]]|[[County Surveyors (Ireland) Act 1862]]|[[Vestry Cess Abolition (Ireland) Act 1864]]|[[Grand Jury (Ireland) Act 1873]]|[[Prison Officers Superannuation (Ireland) Act 1873]]|[[Statute Law Revision Act 1874]]|[[Public Works Loans (Ireland) Act 1877]]|[[General Prisons (Ireland) Act 1877]]|[[Statute Law Revision Act 1890]]|[[Statute Law Revision Act 1891|[[Grand Jury (Ireland) Act 1895]]]]}}`,
 `[[File:Bear Valley Whaleback DCP 0776.jpg|thumb|upright=1.1[[Bear Valley Strip Mine]], southwest of [[Shamokin, Pennsylvania|Shamokin]] in the [[Coal Region]]]]`,
  `{{Infobox automobile
| name == Dodge Viper SRT-10 (ZB II) ==
| manufacturer = [[Chrysler LLC]]<br>[[Chrysler Group LLC]]
|'''Coupé:''' {{convert|47.6|in|mm|abbr=on}}
|'''Convertible:''' {{convert|48.6|in|mm|abbr=on}}
}}
| weight = {{ubl
|'''SRT-10:''' {{convert|3460|lb|kg|0|abbr=on}}
|'''ACR:''' {{convert|3408|lb|kg|abbr=on}}
}}
| predecessor = [[Dodge Viper (ZB I)]]
| successor = [[Dodge Viper (VX I)]]
}}`,
`{{Mono|1==:=}}`,
`<imagemap>
:File:Comparison of temperature scales blank.svg|845x580px|class=skin-invert-image|
circle 40 330 4  [[:File:Comparison of temperature scales blank.svg|0 K / 0 °R (−273.15 °C)]]
</imagemap>`,
`{{mono|1==, -ci=, <, -ci<, >, -ci>, <=, -ci<=, >=}}`,
`{|border=1　
|- style="background:LIGHTGREY" 
|hitting average||Games||At bat||Runs||Hits||RBI||Double||Triple||HR||K||Walk
|- 
||.310||9||29||2||9||4||4||0||0||5||8
|}`,  
`{{Infobox university
 | name                   = Canberra Institute of Technology
  | postgrad               = 
 | doctoral               = 
 | '''profession''''''''' = 
 | city                   = Canberra
 | state                  = [[Australian Capital Territory|ACT]]
 }}`,  
  `{{cite web|url=http://www.lnfs.es/Data/Adjuntos/1.3%20RFEF%20%20Reglamento%20General%202014-2015.pdf|title=Reglamento General|website=www.[[LNFS|publisher=[[Royal Spanish Football Federation]]|trans-title=General Regulations|access-date=26 March 2015|lnfs]].es|language=es}}`,
  `<references>-Block vollständig erhalten.

Hier ist der komplette Abschluss für deinen Artikel 3rd Army Group (France):

Code-Snippet

==References==
<references>
<ref name="hetou3-2">{{cite book|last=Mary|first=Jean-Yves|author2=Hohnadel, Alain|author3=Sicard|title=Hommes et Ouvrages de la Ligne Maginot, Tome 3|publisher=Histoire & Collections|year=2003|pages=150–152|isbn=2-913903-88-6|language=fr}}</ref>
<ref name="hetou3-3">Mary, Tome 3, p. 146</ref>
<ref name="Mary3-4">Mary, Tome 3, p. 189</ref>
<ref name="8e-armee">{{Cite web|url=http://france1940.free.fr/oob/8armee.html|title = 8e Armée Order of Battle / Ordre de bataille, 10/05/1940}}</ref>
<ref name="ga3-reserves">{{Cite web|url=http://france1940.free.fr/oob/ga3.html|title = Réserves du 3e Groupe d'Armées Order of Battle, 10 May 1940}}</ref>
</references>`,
`<gallery mode="packed" heights="180px">
File:Santa Giustina (Padua) - Chapel of Saint Luke.jpg|Chapel of Saint Luke
Abbazia di Santa Giustina (St Luc).jpg|Altar and Reliquary of [[St. Luke the Evangelist]]
Santa Giustina (Padua) - Chapel of Saint Luke - Tomb of Luke the Evangelist (front).jpg|Reliquary of [[St. Luke the Evangelist]] and transept organ
Santa Giustina (Padua) - Chapel of Saint Luke - Tomb of Luke the Evangelist (rear).jpg|Reliquary of [[St. Luke the Evangelist]] (rear)
Santa Giustina (Padua) - Chapel of Saint Luke - Madonna of Constantinople Sixteenth-century version.jpg|''Madonna of Constantinople'' Sixteenth-century version
Padova - Santa Giustina - MadonnaCostantinopolitana (Expo -12 Contrasto 25).jpg|''''Madonna of Constantinople'' original version
</gallery>`,
`https://web.archive.org/web/20160304040450/http://www.hollywoodflip.com/articlex.php?subaction=showfull&id=1281026953&archive=&start_from=&ucat=1&; `,
`<span |align=right style="color:green;">`,
`<gallery>
File:Oceanic whitetip shark at Elphinstone Reef.jpg|[[Oceanic whitetip shark]] at Elphinstone Reef
File:Elphinstone Reef soft corals.jpg|[[Soft corals]] and [[Anthias]] at Elphinstone Reef
File:Elphinstone Reef memorial.jpg|Memorial dedicated to [[Arno Wallaard]] on south plato
File:Hawksbill turtle at Elphinstone Reef, Red Sea, Egypt (35150034493).jpg|[[Hawksbill sea turtle|Hawksbill turtle]] at Elphinstone Reef[[File:The_drop-off_at_Elphinstone_Reef,_Egypt.jpg|thumb|Anthias and hard corals at the Elphinstone Reef drop-off[[File:Dendronephthya_Soft_corals_at_Elphinstone_Reef,_Egypt.jpg|thumb|Dendronephthya Soft corals at Elphinstone Reef, Egypt. They thrive in the strong currents at this offshore site.[[File:Bluecheek_butterflyfish_at_Elphinstone_Reef,_Egypt.jpg|thumb|A pair of bluecheek butterflyfish (Chaetodon semilarvatus) at Elphinstone Reef, Egypt.]]]]]]
</gallery>`,
`{{Australia-hurdles	-athletics-bio-stub}}`,
`{{Football kit	box |
| pattern_la = 
| pattern_b  = 
| pattern_ra = 	
| pattern_sh =	
| pattern_so = 
| leftarm    =  84D3EB	
| body       =  84D3EB
| rightarm   =  84D3EB	
| shorts     =  84D3EB	
| socks      = 84D3EB	
| title  = -2025
}}`,
`<ref name= "Winfield">Winfield p. 208</ref>`,
`<div style=position:center; Transform-rotate|90|display=block>`,
`{{safesubst<noinclude/>:#invoke:political party|fetch|Republican Party of Arkansas|color}}`,
`<ref>{{multiref|
{{cite book |first3=J. S. |last3=Sandars |last1=O'Malley |first1=Edwin L. |last2=Hardcastle |first2=Henry |title=Reports of the Decisions of the Judges for the trial of Election Petitions in England and Ireland as pursuant to the Parliamentary Elections Act 1868 |location=London |publisher=Stevens & Haynes |date=1910 |volume=5 |url=https://babel.hathitrust.org/cgi/pt?id{{=}}mdp.35112103161263 |pages=212–217 }}|
{{cite book |title=Copy of the Shorthand Writers' Notes of the Judgment of Mr. Justice Lawrance and Mr. Justice Walton on the Trial of the Election Petition of the City of Worcester; also the Minutes of Evidence taken at the Trial of the said Election Petition |date=14 June 1906 |url=https://babel.hathitrust.org/cgi/pt?id=umn.31951d02039880s&seq=5 |series=Parliamentary papers |volume=HC 1906 xcv (198) 255 }}|
{{cite book |title=Commons Chamber |date=14 June 1906 |volume=158 |url=https://hansard.parliament.uk/Commons/1906-06-14/debates/7d1bb89d-ffd9-4973-8941-1539dda3b54b/CommonsChamber}}
}}</ref>`,
`https://web.archive.org/web/20181103210140/https://www.nytimes.com/2012/11/17/world/middleeast/in-gaza-tragic-result-for-misplaced-hopes-of-cease-fire.html?_r=1&;;;; `,
  `{{!:}}`,
  `<gallery mode="packed" style="text-align: center;" caption="Gallery" heights="140px" perrow="3">
File:Paul Cézanne - Gardanne (Horizontal View) (Gardanne (vue horizontale)) - BF917 - Barnes Foundation.jpg|Paul_Cézanne_-_Gardanne_(Horizontal_View)_(Gardanne_(vue_horizontale))_-_BF917_-_Barnes_Foundation]]|View of Gardanne by Paul Cézanne
</gallery>`,
  `<gallery>
File:Hawksbill Egypt (35150034493).jpg|[[Hawksbill sea turtle|Hawksbill turtle]] at Elphinstone Reef[[File:The_drop-off_at_Elphinstone_Reef,_Egypt.jpg|thumb|Anthias drop-off[[File:Dendronephthya_Soft_corals_at_Elphinstone_Reef,_Egypt.jpg|thumb|Dendronephthya Soft corals.[[File:Bluecheek_butterflyfish_at_Elphinstone_Reef,_Egypt.jpg|thumb|A .]]]]]]
</gallery>`,
  `{| class="wikitable"
|-
|-bgcolor=<!-- party shade if gain -->lightblue
|}`,
  `{| class="wikitable sortable"  style="font-size: 100%"
|}`,
  `{| class=wikitable =wikitable 
|}`,
  `[[Image:Barrère Pierre 1690-1755 Ornithologiae.png|thumbnail|250px|right|Plate from ''''Ornithologiae Specimen'' de Barrère showing his system of bird classification]]`,
  `[[Image:Edward Snowden-2.jpg{{!}}border|thumb|right|200px|During the season, many critics and analysts noticed parallelisms between the series' premise and [[Edward Snowden]].]]`,
  `{|class=wikitable
!=width:60px|[[A Just Russia|JR]]
|}`,
  `<ref = name = "galloway"></ref>`,
    `{| class="wikitable sortable"
| '''[[American pipit]]''' ||''Anthus rubescens'' || '''A'''||=center style="background: #006666"| {{font color|White|Least concern}} || align=center|{{decrease}}|| Vagrant.|| [[Image: Anthus rubescens japonicus.JPG|175px]]
|}`,
  `Richardson, Dan and Jacobs, Daniel (2007) ''The Rough Guide to Egypt''. ISBN 978 -1-84353-782-3 (7th edition). p.718`,
  `* {{ISBN|978-88-11-73836-7}}, ISBN 978 -82-91165-04-2.`,
  `ISBN 978-0-9559732 -0-8.`,
  `<gallery Mode="packed" heights="140px">
File:COLLECTIE TROPENMUSEUM De Broodbakkersstraat in Pasoeroean TMnr 60052542.jpg|Chinatown of Broodbakkerstraat (now Jalan Niaga) in Pasuruan from Hoofdstraat (now Jalan Soekarno Hatta)
File:COLLECTIE TROPENMUSEUM Stoplicht boven het kruispunt in de Hoofdstraat Pasoeroean TMnr 60052543.jpg|Traffic light over the intersection in the Hoofdstraat of Pasuruan
File:PSSM B16 at Waroeng-dowo.jpg|Warungdowo as the main operational station of PsSM with Hohenzollern B16 tram engine (right side).
File:Stasiun Winongan.jpg|Winongan tram station around 1910-1920s
File:Pasoeroan Town in 1946's map.tif|thumb|Map issued in February 1946 showing the city of Pasuruan including the main railway line of [[Staatsspoorwegen]] (SS) and PsSM's steam tram line]]
</gallery>`,
  `<ref>{{usurped|1={{usurped|1=[https://archive.today/20070702164815/http://www.popcultureshock.com/reviews.php?id=4542 APComics 2005 Preview » PopCultureShock<!-- Bot generated title -->]}}}}</ref>`,
  `{{Clade|style=font-size:90%; line-height:90%
|label1=Halobacteriales
|1={{clade
  |1={{clade
    |1=[[Natronoarchaeaceae]]
    |2={{clade
      |1=''[[Haloparvum]]'' **
      |2={{clade
        |1=''[[Halopenitus]]'' **
        |2=''[[Halorubrum]]'' {[[Halorubraceae]]}
         }}
       }}
     }}
  |2={{clade
    |1=[[Haloferacaceae]]
    |2={{clade
      |1={{clade
        |1=[[Halobacteriaceae]]
        |2=[[Haloarculaceae]]
         }}
      |2={{clade
        |1={{clade
          |1=''[[Halostella]]'' * {"Halostellaceae"}
          |2={{clade
            |1=''[[Halalkalicoccus]]'' {"Halalkalicoccaceae"}
            |2=''[[Halococcus]]'' {[[Halococcaceae]]}
             }}
           }}
        |2={{clade
          |1={{clade
            |1=''[[Salinarchaeum]]'' * {"Salinarchaeaceae"}
            |2=[[Haladaptaceae]] (Halorubellaceae)
             }}
          |2=[[Natrialbaceae]]
           }}
         }}
       }}
     }}
   }}
}}`,  
  `<ref = name=lance>[https://www.theglobeandmail.com/sports/more-sports/lance-armstrong-building-deception-upon-lies/article4500335/] " Lance Armstrong: building deception upon lies" ''Globe and Mail'', BRUCE DOWBIGGIN.</ref>`,
  `[[File:Jersey yellow.svg|20px|link=General classification in the Tour de France|alt=Yellow 
]]`,
  `{{reflist |group=Note |refs=
<ref name="c">French reports: "The (over 5{{nbs"[[Hectare|ha]]) era...}}</ref>
}}`,
  `* ﻿{{cite web |url= https://reason.com/2019/10/31/its-that-old-looney-tuner-lysander-spooner/ |title= It's That Old Looney Tuner, Lysander Spooner |last= Bagge |first= Peter |date= November 2019 |website= Reason Magazine |format= Comic strip| access-date= }}`,
  `<ref =EJ440-444>{{Cite book|volume=18|edition=Second| location= Detroit |date= 2007| page=440-444|title=Encyclopaedia Judaica|last1=Berenbaum|first1=Michael|last2=Skolnik|first2=Fred|publisher=Thomson Gale}}</ref>`,
  `{{ubl|
|'''Legitimate'''<ref name="nyugenphuc">Bao Dai had two sons and three daughters, according to the genealogy of the Nyugen Phuc clan. Only his children by Nam Phuong are listed. His obituary in ''[https://www.independent.co.uk/news/people/obituary-bao-dai-1243873.html The Independent]'' says he had two sons and two daughters while the ''[https://www.nytimes.com/1997/08/02/world/bao-dai-83-of-vietnam-emperor-and-bon-vivant.html New York Times]'' says two sons and four daughters. (''''[http://giapha.nguyenphuoctoc.info/download/NGUYEN-PHUC-TOC-THE-PHA.pdf Nguyễn Phúc tộc thế phả]'', 1995, p. 330).</ref>{{ubl|
|[[Bảo Long]] (1936–2007)
|[[Phương Mai]] (1937–2021)
|[[Phương Liên]] (1938–)
|Phương Dung (1942–)
|[[Bảo Thăng]] (1943–2017)}}
|'''Unrecognized'''{{ubl|
|Phương Thảo (1946–)
|Phương Minh (1949–2012)
|[[Bảo Ân]] (1951–)
|Bảo Hoàng (1954–1955)
|Bảo Sơn (1957–1987)
|Phương Từ (1955)
|Patrick-Édouard Bloch-Carcenac (1958–)
}}
}}`,
  `{{Collapsible list|title=''See list''{{[[Felix de Muelenaere|Comte de Muelenaere]] (1831-1832)|[[Albert Goblet d'Alviella|Comte d'Alviella]] (1832-1834)|[[Barthélémy de Theux de Meylandt|Chevalier de Theux de Meylandt]] (1834-1840; 1846-1847)|[[Joseph Lebeau]] (1840-1841)|[[Jean-Baptiste Nothomb]] (1841-1845)|[[Sylvain Van de Weyer]] (1845-1846)|[[Charles Rogier]] (1847-1852; 1857-1862)|[[Henri de Brouckère]] (1852-1855)|[[Pierre de Decker]] (1855-1857)}}}}`,
  `{{&prime;}}`,
  `<imagemap>
Image:Menthol synthesis.png|

rect 6 14 131 92 [[myrcene]]
rect 136 46 201 63 [[diethylamine]]
rect 468 110 628 180 [[citronellal]]
rect 387 112 458 135 [[zinc bromide]]
rect 95 97 223 209 [[menthol]]

desc bottom-left
#Notes:
#Details on the new coding for clickable images is here: [[mw:Extension:ImageMap]]
#[https://web.archive.org/web/20080327003154/http://tools.wikimedia.de/~dapete/ImageMapEdit/ImageMapEdit.html?en This image editor] was used.
</imagemap>`,
  `{{refn|''StarHorse2: Fifth Expansion''
* Fiscal year ended 31 March 2010: ¥2.8&nbsp;billion<ref name="sega_mar10"/>
* Fiscal year ended 31 March 2011: ¥2&nbsp;billion<ref name="sega_mar11"/>
* Currency conversion:<ref name="xe_currency"/>
** ¥2.8 billion = $34.6039 million
** ¥2 billion = $24.7171 million
|group=n|name=StarHorse2}}`,
  `{{SAFESUBST:<noinclude />#invoke:Unsubst||date=__DATE__ |$B=
{{More citations needed section| name  = More citations needed section
| find  = {{#if:{{{find|}}}|{{{find|}}}|none}}
| find2  = {{{find2|{{{unquoted|}}}}}}
|date=June 2025| talk  = {{{talk|}}}
| small = {{{small|}}}
}}
}}`,
  `<poem>In the name of God, the Merciful, the Compassionate
Name and surname
Signature|author=Ruhollah Khomeini|title=Iranian Constitution|source=Article 67 of the Constitution of the Islamic Republic of Iran}}
===Monarch===
{{blockquote|<poem>
So help me, God Almighty!
(This I affirm!)</poem>`,
  `<inputbox>
id = style-searchbox
type=fulltext
width=35
break=yez
searchfilter=deepcat:"Canadian people"
namespaces=Main**
placeholder=e.g. female historians
searchbuttonlabel = Search Canadian people articles 
</inputbox>`,
  `{{c. |1979|lk=none}}`,
  `<gallery>
File:Sydney Skyline (5620756401).jpg|The [[Sydney central business district]] in [[Sydney]]'s [[Western Suburbs, Sydney|western suburbs
</gallery>`,
  `<ref name="Poznań">''{{cite web |url=http://www.poznan.pl/mim/public/publikacje/pages.html?co=list&id=19&ch=20&instance=1017&lang=pl |title=Poznań Official Website – Twin Towns|access-date=29 November 2008 |publisher={{fontcolor|Green|(in [[Polish language|{{fontcolor|Green|Polish}}]])}} [[copyright|]] 1998–2008 Urząd Miasta Poznania }}''</ref>`,
  `{| class=wikitable
|+ Occitan words and their French, Catalan and Spanish cognates
|-
!  scope="col" rowspan=2 | English
!! scope="col" colspan=2 | Cognate of French
!! scope="col" colspan=3 | Cognate of Catalan and Spanish
|-
!  scope="col" | Occitan
!! scope="col" | French
!! scope="col" | Occitan
!! scope="col" | Catalan
!! scope="col" | Spanish
|-
| broom || style{{=}}"background: Gainsboro" | {{lang|oc|balaja}} || {{lang|fr|balai}} || style{{=}}"background: Gainsboro" | {{lang|oc|escoba}} || {{lang|ca|escombra}} || {{lang|es|escoba}}
|-
|}`,
  `<imagemap>
File:Subtraction_game_SMIL.svg|thumb|Interactive subtraction game.
default [http://upload.wikimedia.org/wikipedia/commons/4/4d/Subtraction_game_SMIL.svg]
</imagemap>`,
  `{{Infobox national military
| name = Tatmadaw
| native_name = {{lang|my|တပ်မတော်}} {{lang|my-latn|{{small|Tapmătau}}}}<br>{{lang|en|[[Royal Burmese Armed Forces|Royal Armed Forces]]}}
| image = {{ubl|[[File:Full Emblem of the Myanmar Armed Forces.svg|200px|frameless]]|[[Emblem of Tatmadaw|Emblem of the Myanmar Armed Forces]]
----
[[File: Flag of the Armed Forces (Tatmadaw) of Myanmar.svg|225px|border]]|Flag of the Myanmar Armed Forces}}
| image_size = 
| alt = 
| caption = 
| image2 = {{ubl|[[File: Emblem of the Myanmar Armed Forces.svg|150px|frameless]]|Mark Logo
----
{{Photomontage
 | photo1a = Shoulder Sleeve of Myanmar Army.svg
 | size    = 300
 | spacing = 5
 | color   = transparent
 | border  = 0
 | text    = 
}}|'''Top:''' Emblems of main service branches: [[Myanmar Army|Army]]{{efn|Also the [[formation patch]] of Chief of Staffs' office.<ref>{{cite web |url=https://www.cincds.gov.mm/ |title=CINCDS Myanmar |publisher=Cincds.gov.mm |date= |accessdate=2022-08-03 |archive-date=14 June 2022 |archive-url=https://web.archive.org/web/20220614005935/https://cincds.gov.mm/ |url-status=live }}</ref>}}, [[Myanmar Navy|Navy]] and [[Myanmar Air Force|Air Force]]|'''Bottom:''' Emblems of auxiliary services: [[Myanmar Coast Guard|Coast Guard]], [[Myanmar Police Force|Police Force]] and [[Myanmar Border Guard Forces|Border Guard Forces]]}}
| branches = {{plainlist|
* {{army|MYA}}
* {{navy|MYA}}
* {{air force|MYA}}
* {{flagicon image|Myanmar MOHA Flag.svg}} [[Ministry of Home Affairs (Myanmar)]] (de facto)
}}
* {{flagicon image|Flag of the Myanmar Police Force.svg}} [[Myanmar Police Force]]{{cn|date=March 2026}}
| headquarters = [[Naypyidaw]], [[Myanmar]]
| website = {{Bulleted list
| {{URL|mod.gov.mm}}
| {{URL|cincds.gov.mm}}
}}
| ranks = [[Military ranks of Myanmar]]
}}`,

    `<imagemap>
File:Mustelidae-01.jpg|250px|alt=Alt text 
rect 800 1066 1599 1594 [[Honey badger |Honey badger (''Mellivora capensis'')]] 
desc none
default [[Mustelidae]]
</imagemap>`,
  `<gallery mode="packed" caption="Coloniae and Municipia image gallery">
  File:Arco Romano.jpg|Roman arch of [[Pax Iulia|]]''[[Pax Julia|Pax Iulia]]'' ([[Beja, Portugal|Beja]])
  </gallery>`,
  `{{gloss|listen, earl, to [[Kvasir]]'s blood (=poetry)}}`,
  `<ref name="Eaton-sep" |pages=286 |date=July 2025}}<ref name="Eaton 2004">{{cite book |last=Eaton |first=Richard M. |title=Temple desecration and Muslim states in medieval India |date=2004 |publisher=Hope India Publications |isbn=978-8178710273 |location=Gurgaon |pages=31–49 |quote=For, while  }}</ref>`,
  `[[File:Kasparov-34.jpg{{!}}border|thumb|alt=refer to caption|Kasparov in 2007|upright=0.75]]`,
  `[[File:Sarnia Cherie.ogg|alt=
  Chord progression of Sarnia Chérie (English: Guernsey Dear), unofficial anthem of Guernsey]]`,
  `{|width=50% |gap=4em
  | '''Child'''
  | '''Namesake'''
  |-
  | 1st son
  | paternal grandfather
  |-
  | 2nd son
  | maternal grandfather
  |-
  | 3rd son
  | father
  |-
  | 4th son
  | father's oldest brother
  |-
  | 1st daughter
  | maternal grandmother
  |-
  | 2nd daughter
  | paternal grandmother
  |-
  | 3rd daughter
  | mother
  |-
  | 4th daughter
  | mother's oldest sister
  |}
  `,
  `{{cite web https://www.imf.org/external/datamapper/profile/GHA#:~:text=Here's%20some%20information%20about%20Ghana's%20GDP%20from,PPP%2C%20share%20of%20world**%200.14%25%20in%202026|url=https://www.imf.org/en/Publications/WEO/weo-database/2025/april |language=en |access-date=21 June 2025 |archive-date=28 April 2025 |archive-url=https://web.archive.org/web/20250428212902/https://www.imf.org/en/Publications/WEO/weo-database/2025/April |url-status=live }}
  `,
  `<gallery widths="160px" heights="160px" style="text-align:center;" caption="Schlegel diagrams of some fullerenes">
  Graph of 20-fullerene w-nodes.svg|C20<br />([[dodecahedron]])
  Graph of 26-fullerene 5-base w-nodes.svg|C26
  Graph of 60-fullerene w-nodes.svg|C60<br/>([[truncated icosahedron]])
  Graph of 70-fullerene w-nodes.svg|C70
  </gallery>
  `,
  `<gallery widths="180px" heights="180px">
  Caduceus on Mauryan coin.jpg | Caduceus symbol on a Maurya-era [[punch-marked coin]]
  India Mauryan emperor Ashoka Punch-marked Coin.jpg | A punch-marked coin attributed to Ashoka<ref>{{cite book |last=Mitchiner |first=Michael |date=1978 |title=Oriental Coins & Their Values: The Ancient and Classical World 600 B.C. - A.D. 650 |publisher=Hawkins Publications |page=544 |isbn=978-0-9041731-6-1}}</ref>
  I15 1karshapana Maurya Ashoka MACW4229 1ar (8486624862).jpg | A Maurya-era silver coin of 1 [[karshapana]], possibly from Ashoka's period, workshop of Mathura. ''Obverse:'' Symbols including a sun and an animal ''Reverse:'' Symbol ''Dimensions:'' 13.92 x 11.75&nbsp;mm ''Weight:'' 3.4 g.
  </gallery>
  `,
  `<gallery mode="packed">
  File:Collins class submarine with the aircraft carrier Charles de Gaulle in May 2019.jpg|[[French aircraft carrier Charles de Gaulle|[[French aircraft carrier Charles de Gaulle|''Charles de Gaulle'' (R91)]] nuclear-powered aircraft carrier
  File:Temeraire1048.jpg|[[Triomphant-class submarine|[[Triomphant-class submarine|''Triomphant'']]-class nuclear ballistic missile submarine
  </gallery>`,  
  `[<!-- http://perso.univ-rennes1.fr/antoine.chambert-loir/DJVU/ -->https://www.irphe.fr/~clanet/otherpaperfile/articles/Galois/N0029062_PDF_1_84.pdf Œuvres Mathématiques]`,
  // Basic template
  '{{Template|}}',
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
  // Subst/safesubst modifier should keep magic-word structure
  '{{subst:CURRENTYEAR}}',
  '{{safesubst:CURRENTYEAR}}',
  // #invoke should emit invoke-module/invoke-function children
  '{{#invoke:Foo|bar}}',
  '{{#invoke:Foo|bar|x=y}}',
  // Namespaced transclusion titles should not be forced into Template: namespace
  '{{Module:Foo}}',
  '{{Template:Bar}}',
  '{{User:Example}}',
  '{{:File:Example.jpg}}',
  // Multiline parameter values containing heading syntax should remain inside template
  '{{T|x=\n==H==\n}}',
  '{{A|\n=H=\n}}',
  // Arg default should be parsed through stages 0-2 in JS
  '{{{a|<b>x</b>}}}',
  '{{{a|{{T}}}}}',

  // BEGIN: auto-generated parity sweep (braces)
  "{{T|v=RFC 2119}}",
  "{{T|v=__NOTOC__}}",
  "{{T|v=-{zh-hans:简;zh-hant:繁;}-}}",
  // END: auto-generated parity sweep (braces)
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
  // boolean attribute should still produce empty attr-value child
  '<ref a=b c="d" e>z</ref>',
  // ref inner content should still parse quotes/template in later stages
  "text<ref>John M. (2009). ''How a ''. {{ISBN|9780300158864}}</ref> end",
  // inExt=true behavior: [[A|]] should remain literal inside ref
  '<ref>[[A|]]</ref>',
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
  // Horizontal rule (4 dashes minimum)
  'Line above\n----\nLine below',
  // More than 4 dashes
  'Line above\n-----------\nLine below',
  // Fewer than 4 dashes – not an HR
  'Line above\n---\nLine below',
  // __NOTOC__ keyword
  '__NOTOC__ in text',
  // __TOC__ keyword
  '==Section==\n__TOC__\nContent',
  // __FORCETOC__ keyword
  'Start\n__FORCETOC__\nEnd',
  // __NOEDITSECTION__ keyword
  '__NOEDITSECTION__\n== Section ==',
  // __NEWSECTIONLINK__ keyword
  '__NEWSECTIONLINK__',
  // Alias keywords should canonicalize token name via config maps
  '__NOTC__',
  '__nOtC__',
  '__NOCC__',
  '__DISAMBIG__',
  '__EXPECTED_UNCONNECTED_PAGE__',
  // Double underscore that is NOT a keyword – left as-is
  '__NOTAKEYWORD__',
  // Section heading finalization (detected in stage 1, finalized in stage 4)
  '== Section Title ==\nContent here.',
  '=== Level 3 ===',
  // HR inside a table is still valid
  '{|\n|-\n| before\n----\nafter\n|}',

  // Heading trail must be a text node, not a bare string
  '== Before ==\n\n',
  '== Before ==\n\n----\n\n== After ==',

  // BEGIN: auto-generated parity sweep (hr_and_double_underscore)
  "== RFC 2119 ==",
  "== https://example.org/a ==",
  "== -{zh-hans:简;zh-hant:繁;}- ==",
  // END: auto-generated parity sweep (hr_and_double_underscore)        
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
  // Test Image: vs File: namespace for nested link in caption
  '[[Image:Foo.jpg|caption [[Link]] text]]',
  '[[File:Foo.jpg|caption [[Link]] text]]',
  '[[File:Foo.jpg|upright|caption [[Link]] text]]',
  '[[Image:Foo.jpg|upright|caption [[Link]] text]]',
  // Simple internal link
  'See [[Main Page]] for details.',
  // Link with display text
  '[[Page|display text]]',
  // Delimiter with empty display text should keep empty text child
  '[[Page|]]',
  // Link with colon prefix (forces link, not category/file)
  '[[:Category:Foo|label]]',
  // Namespace-prefixed link
  '[[Help:Contents]]',
  // Link with anchor
  '[[Page#Section|section link]]',
  // File / image embed (namespace 6)
  '[[File:Image.jpg]]',
  // File with options
  '[[File:Image.jpg|thumb|right|Caption text]]',
  '[[File:Image.jpg| thumb | upright=0.8 | Caption text]]',
  // File target should preserve/canonicalize first-letter case like JS parser
  '[[File:water_reflectivity.jpg]]',
  // File caption that is only a nested wikilink after thumb
  '[[File:Image.jpg|thumb|[[Pierre-Joseph Proudhon]]]]',
  // Category link
  '[[Category:Example]]',
  // Category link with sort key
  '[[Category:Example|sort key]]',
  // Self-link (anchor only)
  '[[#Section]]',
  // Interwiki-like (invalid, treated as plain text in standard config)
  '[[en:English article]]',
  // Link with template inside display text
  '[[Page|{{Template}}]]',
  // Multiple links in one sentence
  '[[Page A]] and [[Page B]] go here.',
  // Link immediately followed by letters (no space)
  '[[Page]]s',
  // Nested brackets that are NOT a link
  '[[invalid link]]s or just [[valid]]',

  // BEGIN: auto-generated parity sweep (links)
  "[[A|-{zh-hans:简;zh-hant:繁;}-]]",
  "[[File:water_reflectivity.jpg|thumb|RFC 2119]]",
  "[[File:water_reflectivity.jpg|thumb|https://example.org/a]]",
  "[[File:Mardi&nbsp;Gras&nbsp;Mobile&nbsp;Order of Inca.jpg|thumb|left|upright|Mobile is the birthplace of Mardi Gras in the U.S.]]",
  // END: auto-generated parity sweep (links)
  // Unordered list
  '* Item 1\n* Item 2\n* Item 3',
  // Ordered list
  '# One\n# Two\n# Three',
  // Description list – term
  '; Term : Definition',
  // Description list – definition only
  ': just a definition',
  // Mixed list types
  '* Bullet\n# Number\n* Bullet again',
  // Nested unordered
  '* Level 1\n** Level 2\n** Level 2 again\n* Level 1 again',
  // Nested ordered
  '# First\n## Sub-first\n## Sub-second\n# Second',
  // Mixed nesting
  '* Bullet\n*# Numbered sub-item\n* Bullet again',
  // List item with inline markup
  "* ''italic'' item",
  // List item with a link
  '* [[Main Page|home page]] item',
  // Indented block (: prefix) that is NOT a definition list term
  ': indented paragraph',
  // Deep nesting
  '**** fourth level',
  // List after a blank line (new list context)
  '* First list\n\n* Second list',
  // Plain text line between list items
  '* item 1\nplain text\n* item 2',

  // BEGIN: auto-generated parity sweep (list)
  "* -{zh-hans:简;zh-hant:繁;}-",
  "* {{T|v=RFC 2119}}",
  // END: auto-generated parity sweep (list)  
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
  `{{Infobox country
| religion = {{unbulleted list
|{{Tree list}}
** <ref>{{cite book|year=2010|quote=...&nbsp;rules.}}</ref>
{{Tree list/end}}
 }}
}}
`,
 '{{NS}}',
`{{usurped|1=[https://web.archive.org/1021500/http://www.snagfilms.com/films/ 'Afghanistan' (2000) {{!}} SnagFilms]}}`,
`{{Retracted|doi=10.1016/j.jbc.2021.100764|pmid=34237888|http://retractionwatch.com/?s=%22Xuetao+Cao%22 ''Retraction Watch''|intentional=yes}}`,
`{{cite book | last=Long | first=A. A. | title=Science and Speculation. Studies in Hellenistic Theory and Practice | publisher=Cambridge University Press | year=2005 |<!--165–191-->page=174 |editor1=Barnes, Jonathan |editor2=Brunschwig, J. | chapter=6: Astrology: arguments pro and contra |isbn=978-0-521-02218-7}}`,
`<div style{{=}}"padding-left:4em;">`,
`{{Dark mode invert|image=y|[[Image:Bijection.svg|thumb|A bijective function, ''f'': ''X'' → ''Y'', where set X is {1, 2, 3, 4} and set Y is {A, B, C, D}. For example, ''f''(1) = D.]]}}`,
 `{{cite journal |last=Gupta |first=Radha Charan |author-link=Radha Charan Gupta |title=Varāhamihira's Calculation of {{tmath|{}^nC_r}} and the Discovery of Pascal's Triangle |journal=Gaṇita Bhāratī |volume=14 |number=1–4 |year=1992 |pages=45–49 }} Reprinted in {{cite book |editor-last=Ramasubramanian |editor-first=K. |year=2019 |title=Gaṇitānanda |publisher=Springer |doi=10.1007/978-981-13-1229-8_29 |pages=285–289 }}`,
  `<gallery mode="packed" heights="180px">. 
helpers.js:355
File:Starry Night Over the Rhone.jpg|Van Gogh's ''[[Starry Night Over the Rhône]]'' (1888). Blue used to create a mood or atmosphere. A cobalt blue sky, and cobalt or ultramarine water.
helpers.js:355
File:Matisse Conversation.jpg|''[[The Conversation (Matisse)|The Conversation]]'' by [[Henri Matisse]] (1908–1912)
helpers.js:355
</gallery>
  `,
`{{Script/Hebrew|כִּֽי־אַתָּ֤ה שַׁלּ֙וֹתָ֙ גּוֹיִ֣ם רַבִּ֔ים יְשָׁלּ֖וּךָ כׇּל־יֶ֣תֶר עַמִּ֑ים מִדְּמֵ֤י אָדָם֙ וַֽחֲמַס־אֶ֔רֶץ קִרְיָ֖ה וְכׇל־יֹ֥שְׁבֵי בָֽהּ׃ {פ}}}`,
  `<section begin="UK General Election 2001"/>`,
  `[[File:A map of the descendants of Abu Bakr al-Siddiq.svg | alt=A map showing the ]]`,
  `<ref name="https://pt.scribd.com/document/742344591/smartproxy-cities"/>`,
  `[[Mushroom poisoning|deadly mushrooms]]`,
  `; A'''B''' : C`,
  `; A''B'' : C`,
  `; A'''''B''''' : C`,
  //`; outer '''bold : text''' end`,
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
  // Simple italic
  "This is ''italic'' text.",
  // Simple bold
  "This is '''bold''' text.",
  // Bold and italic combined
  "This is '''''bold italic''''' text.",
  // Italic then bold on the same line
  "''italic'' and '''bold''' on one line.",
  // Unbalanced italic (odd count) – MediaWiki balancing rules apply
  "''unbalanced italic",
  // Four apostrophes – treated as one leading apostrophe + bold
  "''''four apostrophes''''",
  // Six apostrophes – five active + one leading
  "''''''six apostrophes''''''",
  // Bold spanning across italic
  "'''bold ''both''' italic''",
  // Multiple quote runs on one line
  "''a'' '''b''' ''c'''",
  // First bold-start after a space (affects balancing heuristic)
  "text '''bold with space before''' text",
  // Line with only apostrophes and no other text
  "'' '''",
  // Quotes inside a template argument (handled by quotes parser per line)
  "{{Template|''arg''}}",
  // Nested link with italic display text
  "[[Page|''italic display'']]",  
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
  '{|\n| alias = ISO-IR-006,<ref>reftext</ref> ANSI_X3.4-1968\n|}',
  `{|\n|- 1 CAR\n|}`,
  `{|\n|- 1 car\n|}`,
  `{|\n|- A1 CAR\n|}`,
  `{|\n|- _x :y\n|}`,
  `{|\n|- data-x foo\n|}`,
  `{|\n|- 1=2 CAR\n|}`,
  `{|\n|- "1" CAR\n|}`,
  `{|\n|- '1' CAR\n|}`,
  `{|\n|- {{T}} CAR\n|}`,
  `{|\n|- -{zh-hans:a;zh-hant:b;}- CAR\n|}`,
  `{|\n|- / CAR\n|}`,
  `{|\n|- 123 456\n|}`,
  `{|\n|- a.b c-d e:f\n|}`,
  `{|\n|- \0 1\n|}`,
  `{|\n|- \x7F 1\n|}`,
  `{|\n|- rowspan=4 1 CAR\n|}`,
  `{|\n|- 1   CAR\n|}`,
  `{|\n|- 1\tCAR\n|}`,
  `{|\n|- <!--c--> 1 CAR\n|}`,
  `{|\n|- \0 12t\x7F CAR\n|}`,
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
  // Template-like boolean table attribute in a cell
  '{|\n| {{green}} | x\n|}',
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
  // Baseline converter samples under a variant-enabled config
  '-{text}-',
  '-{zh-hans:简体;zh-hant:繁體}-',
  '-{zh:漢字;zh-hans:汉字;zh-hant:漢字}-',
  '-{A|zh-hans:简体;zh-hant:繁體}-',
  '-{R|raw text}-',
  '-{zh-hans:-{inner}-;zh-hant:outer}-',
  '{{Template|-{zh-hans:简;zh-hant:繁}-}}',
  "''italic'' -{ zh-hans:汉字 }- text",
  '-{}-',
  '-{|zh-hans:简体;zh-hant:繁體}-',
  // Unidirectional rule form
  '-{a=>zh-hans:简;zh-hant:繁}-',  
];

if (process.argv[1] === __filename) {
    runTests(castTests, { name: 'wiki-cast' });
}

module.exports = { castTests };
