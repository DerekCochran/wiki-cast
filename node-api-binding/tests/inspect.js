'use strict';
const { nodeToJSON, Token, proto, patch } = require('./helpers.js');

function inspect(wikitext) {
  let result;
  const token = new Token(wikitext);
  token.type = 'root';
  proto.__orig_parse.call(token, 11, false);
  result = JSON.stringify(nodeToJSON(token), null, 2);
  return result;
}

const samples = [
  '#REDIRECT [[Target Page]]',
  '#REDIRECT [[Target|ignored display]]',
  'before <!--comment--> after',
  '<ref>Citation here.</ref>',
  '<noinclude>template-only</noinclude>',
  'just plain text',
];

for (const s of samples) {
  console.log('=== INPUT:', JSON.stringify(s));
  console.log(inspect(s));
  console.log();
}
