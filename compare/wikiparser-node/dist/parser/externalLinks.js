"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.parseExternalLinks = void 0;
const string_1 = require("../util/string");
const extLink_1 = require("../src/extLink");
const magicLink_1 = require("../src/magicLink");
/* NOT FOR BROWSER */
const constants_1 = require("../util/constants");
/* NOT FOR BROWSER END */
/**
 * 解析外部链接
 * @param wikitext
 * @param config
 * @param accum
 * @param inFile 是否在图链中
 */
const parseExternalLinks = (wikitext, config, accum, inFile) => {
    // eslint-disable-next-line @typescript-eslint/no-unused-expressions
    /\[((?:\0\d+[cn]\x7F)*(?:\0\d+f\x7F|(?:(?:ftp:\/\/|\/\/)(?:\[[\da-f:.]+\]|[^[\]<>"\t\n\p{Zs}])|\0\d+m\x7F)[^[\]<>"\t\n\p{Zs}]*(?=[[\]<>"\t\p{Zs}]|\0\d)))(\p{Zs}*(?!\p{Zs}))([^\]\n]*)\]/giu;
    config.regexExternalLinks ??= new RegExp(String.raw `\[((?:\0\d+[cn]\x7F)*(?:\0\d+f\x7F|(?:(?:${config.protocol}|//)${string_1.extUrlCharFirst}|\0\d+m\x7F)${string_1.extUrlChar}(?=[[\]<>"\t${string_1.zs}]|\0\d)))([${string_1.zs}]*(?![${string_1.zs}]))([^\]\x01-\x08\x0A-\x1F\uFFFD]*)\]`, 'giu');
    return wikitext.replace(config.regexExternalLinks, (_, url, space, text) => {
        const { length } = accum, mt = /&[lg]t;/u.exec(url);
        if (mt) {
            space = '';
            text = url.slice(mt.index) + space + text;
            url = url.slice(0, mt.index);
        }
        if (inFile) {
            // @ts-expect-error abstract class
            new magicLink_1.MagicLinkToken(url, 'ext-link-url', config, accum);
            return `[\0${length}f\x7F${space}${text}]`;
        }
        // @ts-expect-error abstract class
        new extLink_1.ExtLinkToken(url, space, text, config, accum);
        return `\0${length}w\x7F`;
    });
};
exports.parseExternalLinks = parseExternalLinks;
constants_1.parsers['parseExternalLinks'] = __filename;
