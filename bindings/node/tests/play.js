"use strict";
const path = require('path');
const wikiparser = require("wikiparser-node");
const nativeProto = require(path.join(__dirname, '..', 'build', 'Release', 'wikiparser-node-c-tokenizer.node'));

wikiparser.config = "enwiki";
const config = wikiparser.getConfig();
const jsImpl = wikiparser.parse("{{Infobox person\n| name = John Doe\n| birth_date = {{Birth date|1950|1|1}}\n}}", {
    config: config
});
const nativeImpl = nativeProto.parse("{{Infobox person\n| name = John Doe\n| birth_date = {{Birth date|1950|1|1}}\n}}", {
    config: config
});

if( jsImpl.toString() !== nativeImpl.toString() ) {
    console.error("Error: The string representations of the AST outputs differ between implementations.");
}
if (JSON.stringify(jsImpl) === JSON.stringify(nativeImpl)) {
    console.log("Success: Both implementations produce the same AST.");
} else {
    console.error("Error: The AST outputs differ between implementations.");
}