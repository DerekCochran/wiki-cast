"use strict";
const path = require('path');
const wikiparser = require("wikiparser-node");
const nativeProto = require(path.join(__dirname, '..', 'build', 'Debug', 'wikiparser-node-c-tokenizer.node'));

wikiparser.config = "enwiki";
const testString = "{{Infobox person\n| name = John Doe\n| birth_date = {{Birth date|1950|1|1}}\n}}";
const config = wikiparser.getConfig();
const jsImpl = wikiparser.parse(testString, {
    config: config
});

const testBuffer = Buffer.from(testString, 'utf-8');
const nativeImpl = nativeProto.parse(testBuffer, {
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