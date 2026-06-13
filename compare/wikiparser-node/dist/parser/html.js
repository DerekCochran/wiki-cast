"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.parseHtml = void 0;
const attributes_1 = require("../src/attributes");
const html_1 = require("../src/tag/html");
/* NOT FOR BROWSER */
const constants_1 = require("../util/constants");
/* NOT FOR BROWSER END */
const regex = /^(\/?)([a-z][^\s/>]*)((?:\s|\/(?!>))[^>]*?)?(\/?>)([^<]*)$/iu;
/**
 * 解析HTML标签
 * @param wikitext
 * @param config
 * @param accum
 */
const parseHtml = (wikitext, config, accum) => {
    const { html } = config;
    config.htmlElements ??= new Set([...html[0], ...html[1], ...html[2]]);
    const bits = wikitext.split('<');
    let text = bits.shift();
    for (const x of bits) {
        const mt = regex.exec(x), t = mt?.[2], name = t?.toLowerCase();
        if (!mt || !config.htmlElements.has(name)) {
            text += `<${x}`;
            continue;
        }
        const [, slash, , params = '', brace, rest] = mt, { length } = accum, 
        // @ts-expect-error abstract class
        attrs = new attributes_1.AttributesToken(params, 'html-attrs', name, config, accum), itemprop = attrs.hasAttr('itemprop');
        if (name === 'meta' && !(itemprop && attrs.hasAttr('content'))
            || name === 'link' && !(itemprop && attrs.hasAttr('href'))) {
            text += `<${x}`;
            accum.length = length;
            continue;
        }
        text += `\0${accum.length}x\x7F${rest}`;
        // @ts-expect-error abstract class
        new html_1.HtmlToken(t, attrs, slash === '/', brace === '/>', config, accum);
    }
    return text;
};
exports.parseHtml = parseHtml;
constants_1.parsers['parseHtml'] = __filename;
