"use strict";
const path = require('path');
const wikiparser = require("wikiparser-node");
const nativeProto = require(path.join(__dirname, '..', 'build', 'Debug', 'wikiparser-node-c-tokenizer.node'));

wikiparser.config = "enwiki";
const testString = "[[#Section]]";
const config = wikiparser.getConfig();
const jsImpl = wikiparser.parse(testString, {
    config: config
});

const testBuffer = Buffer.from(testString, 'utf-8');
const nativeImpl = nativeProto.parse(testBuffer, {
    config: config
});

if( jsImpl.toString() !== nativeImpl.toString() ) {
    console.log("jsImpl.toString(): "+ jsImpl.toString());
    console.log("nativeImpl.toString(): "+ nativeImpl.toString());
    console.error("Error: The string representations of the AST outputs differ between implementations.");
}
if (JSON.stringify(jsImpl) === JSON.stringify(nativeImpl)) {
    console.log("Success: Both implementations produce the same AST.");
} else {
    console.log("jsImpl JSON: "+ JSON.stringify(jsImpl));
    console.log("nativeImpl JSON: "+ JSON.stringify(nativeImpl));
    console.error("Error: The AST outputs differ between implementations.");
}