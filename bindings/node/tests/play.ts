"use strict";
const wikiparser = require("wikiparser-node");

const root = wikiparser.parse("{{Infobox person\n| name = John Doe\n| birth_date = {{Birth date|1950|1|1}}\n}}")

console.log(JSON.stringify(root, null, 2));

