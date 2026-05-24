"use strict";
const path = require('path');
const wikiparser = require("wikiparser-node");
const nativeProto = require(path.join(__dirname, '..', 'build', 'Debug', 'wikiparser-node-c-tokenizer.node'));

process.env.DEBUG_PARAM_VALUE = 'true';

wikiparser.config = "enwiki";
nativeProto.config = "/home/djc/git/wikiparser-node-c-tokenizer/config/enwiki.json";

const testString = '{{Template|}}';
const jsImpl = wikiparser.parse(testString);

const testBuffer = Buffer.from(testString, 'utf-8');
const nativeImpl = nativeProto.parse(testBuffer);

const jsString = jsImpl.toString();
const nativeString = nativeImpl.toString();
if( jsString !== nativeString ) {
    console.log("jsImpl.toString(): "+ jsString);
    console.log("nativeImpl.toString(): "+ nativeString);
    console.error("Error: The string representations of the AST outputs differ between implementations.");
}
const jsJSON = JSON.stringify(jsImpl);
const nativeJSON = nativeImpl.jsonStringifyWikiparserNode();
if (jsJSON === nativeJSON) {
    console.log("Success: Both implementations produce the same AST.");
    console.log("js JSON: "+ jsJSON);
    console.log("c  JSON: "+ nativeJSON);
} else {
    console.log("js JSON: "+ jsJSON);
    console.log("c  JSON: "+ nativeJSON);
    console.error("Error: The AST outputs differ between implementations.");
}