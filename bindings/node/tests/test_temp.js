#!/usr/bin/env node
'use strict';
// Parity test: templates, arguments, section headings (stage 1 – parseBraces).
const { runTests } = require('./helpers');

const tempTests = [
  `<gallery widths="180px" heights="180px">
Caduceus on Mauryan coin.jpg | Caduceus symbol on a Maurya-era [[punch-marked coin]]
India Mauryan emperor Ashoka Punch-marked Coin.jpg | A punch-marked coin attributed to Ashoka<ref>{{cite book |last=Mitchiner |first=Michael |date=1978 |title=Oriental Coins & Their Values: The Ancient and Classical World 600 B.C. - A.D. 650 |publisher=Hawkins Publications |page=544 |isbn=978-0-9041731-6-1}}</ref>
I15 1karshapana Maurya Ashoka MACW4229 1ar (8486624862).jpg | A Maurya-era silver coin of 1 [[karshapana]], possibly from Ashoka's period, workshop of Mathura. ''Obverse:'' Symbols including a sun and an animal ''Reverse:'' Symbol ''Dimensions:'' 13.92 x 11.75&nbsp;mm ''Weight:'' 3.4 g.
</gallery>`
];

if (process.argv[1] === __filename) {
    runTests(tempTests, { name: 'wiki-cast' });
}

module.exports = { tempTests };
