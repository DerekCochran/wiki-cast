#!/usr/bin/env node
'use strict';
// Parity test: templates, arguments, section headings (stage 1 – parseBraces).
const { runTests } = require('./helpers');

const tempTests = [
  `{{ubl|
|'''Legitimate'''<ref name="nyugenphuc">Bao Dai had two sons and three daughters, according to the genealogy of the Nyugen Phuc clan. Only his children by Nam Phuong are listed. His obituary in ''[https://www.independent.co.uk/news/people/obituary-bao-dai-1243873.html The Independent]'' says he had two sons and two daughters while the ''[https://www.nytimes.com/1997/08/02/world/bao-dai-83-of-vietnam-emperor-and-bon-vivant.html New York Times]'' says two sons and four daughters. (''''[http://giapha.nguyenphuoctoc.info/download/NGUYEN-PHUC-TOC-THE-PHA.pdf Nguyễn Phúc tộc thế phả]'', 1995, p. 330).</ref>{{ubl|
|[[Bảo Long]] (1936–2007)
|[[Phương Mai]] (1937–2021)
|[[Phương Liên]] (1938–)
|Phương Dung (1942–)
|[[Bảo Thăng]] (1943–2017)}}
|'''Unrecognized'''{{ubl|
|Phương Thảo (1946–)
|Phương Minh (1949–2012)
|[[Bảo Ân]] (1951–)
|Bảo Hoàng (1954–1955)
|Bảo Sơn (1957–1987)
|Phương Từ (1955)
|Patrick-Édouard Bloch-Carcenac (1958–)
}}
}}`
];

if (process.argv[1] === __filename) {
    runTests(tempTests, { name: 'wiki-cast' });
}

module.exports = { tempTests };
