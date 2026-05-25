#!/usr/bin/env node
'use strict';
// Parity test: templates, arguments, section headings (stage 1 – parseBraces).
const { runTests } = require('./helpers');

const tempTests = [
  `<gallery align="left" caption="Various depictions of Saint Anne" |perrow="6">
File:Adorazione dei magi, metà del V sec. Santa Miaria Maggiore Roma, Arco trionfale -FG.jpg|''Adoration of the Magi,'' with (supposedly) St. Anne in the center (5th&nbsp;ct.), [[Santa Maria Maggiore]], Rome
File:Faras Saint Anne.jpg|[[Coptic Orthodox Church|Coptic]] ''[[Saint Anne (wall painting)|Saint Anne]]'' from [[Faras]], [[Nubia]] (8th century), [[National Museum, Warsaw|National Museum]] in [[Warsaw]]
File:Annuciation to Anne-edit.jpg|''Annunciation to Anne,'' [[mosaic]] (11th&nbsp;ct), [[Chora Church]], Istanbul
File:La Martorana (39521628772).jpg|[[Eastern Orthodoxy|Eastern Orthodox]] church [[Church of Santa Maria dell'Ammiraglio|la Martorana]] (12th&nbsp;ct.), Palermo, Sicily
File:Annarelief.jpg|[[International Gothic|Late Gothic]] [[Relief]] bust of crowned St. Anne (as [[spolia]] in the rebuilt) Annakirche, [[Düren]], Germany
File:Frankfurt Karmeliterkloster Annenaltar.jpg|''Legends of St. Anne'' (15th&nbsp;ct.), altar of St. Anne, cloister of the [[Carmelites]], [[Frankfurt]]
File:Sainte Anne et les trois Marie.jpg|''Saint Anne and [[the Three Marys]],'' ''[[Hours of Étienne Chevalier|Book of [[Hours of Étienne Chevalier]],'' illuminated by [[Jean Fouquet]] (late 15th&nbsp;ct.), [[Bibliothèque nationale de France]], Paris
File:Lignee-Ste-Anne v1500.jpg|''[[The Line of Saint Anne]]'', [[Gérard David]] (c.&nbsp;1500), [[Musée des Beaux-Arts de Lyon]]
File:Oberwesel, Liebfrauenkirche 20170322 047-edit.jpg|''The Holy Kinship'' (early 16th&nbsp;ct.), Liebfrauenkirche [[Oberwesel]], Germany
File:BMVB - Doménico Theotokópoulus - La Sagrada Família amb Santa Anna i Sant Joanet - 8606.jpg|''The Holy Family with St. Anne and St. John'' by [[El Greco]] (c.&nbsp;1600), [[Biblioteca Museu Víctor Balaguer]], Vilanova i la Geltrú (Barcelona)
File:Brooklyn Museum - Saint Anne (Sainte Anne) - James Tissot - overall.jpg|''Saint Anne'', [[James Tissot]] (late 19th&nbsp;ct.), [[Brooklyn Museum]], New York
</gallery>`
];

if (process.argv[1] === __filename) {
    runTests(tempTests, { name: 'wiki-cast' });
}

module.exports = { tempTests };
