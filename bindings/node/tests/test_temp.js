#!/usr/bin/env node
'use strict';
// Parity test: templates, arguments, section headings (stage 1 – parseBraces).
const { runTests } = require('./helpers');

const tempTests = [
  `{{Collapsible list|title=''See list''{{[[Felix de Muelenaere|Comte de Muelenaere]] (1831-1832)|[[Albert Goblet d'Alviella|Comte d'Alviella]] (1832-1834)|[[Barthélémy de Theux de Meylandt|Chevalier de Theux de Meylandt]] (1834-1840; 1846-1847)|[[Joseph Lebeau]] (1840-1841)|[[Jean-Baptiste Nothomb]] (1841-1845)|[[Sylvain Van de Weyer]] (1845-1846)|[[Charles Rogier]] (1847-1852; 1857-1862)|[[Henri de Brouckère]] (1852-1855)|[[Pierre de Decker]] (1855-1857)}}}}`
];

if (process.argv[1] === __filename) {
    runTests(tempTests, { name: 'wiki-cast' });
}

module.exports = { tempTests };
