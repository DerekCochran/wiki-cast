#!/usr/bin/env node
'use strict';
// Parity test: templates, arguments, section headings (stage 1 – parseBraces).
const { runTests } = require('./helpers');

const tempTests = [
  `{{cite web https://www.imf.org/external/datamapper/profile/GHA#:~:text=Here's%20some%20information%20about%20Ghana's%20GDP%20from,PPP%2C%20share%20of%20world**%200.14%25%20in%202026|url=https://www.imf.org/en/Publications/WEO/weo-database/2025/april |language=en |access-date=21 June 2025 |archive-date=28 April 2025 |archive-url=https://web.archive.org/web/20250428212902/https://www.imf.org/en/Publications/WEO/weo-database/2025/April |url-status=live }}`
];

if (process.argv[1] === __filename) {
    runTests(tempTests, { name: 'wiki-cast' });
}

module.exports = { tempTests };
