"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.parseMagicLinks = void 0;
const string_1 = require("../util/string");
const magicLink_1 = require("../src/magicLink");
/* NOT FOR BROWSER */
const constants_1 = require("../util/constants");
/* NOT FOR BROWSER END */
const space = String.raw `[${string_1.zs}\t]|&nbsp;|&#0*160;|&#x0*a0;`, sp = `(?:${space})+`, spdash = `(?:${space}|-)`, magicLinkPattern = String.raw `(?:RFC|PMID)${sp}\d+\b|ISBN${sp}(?:97[89]${spdash}?)?(?:\d${spdash}?){9}[\dx]\b`;
/**
 * 解析自由外链
 * @param wikitext
 * @param config
 * @param accum
 */
const parseMagicLinks = (wikitext, config, accum) => {
    if (!config.regexMagicLinks) {
        try {
            // eslint-disable-next-line @typescript-eslint/no-unused-expressions
            /(^|[^\p{L}\p{N}_])(?:(?:ftp:\/\/|http:\/\/)((?:\[[\da-f:.]+\]|[^[\]<>"\t\n\p{Zs}])[^[\]<>"\0\t\n\p{Zs}]*)|(?:rfc|pmid)[\p{Zs}\t]+\d+\b|isbn[\p{Zs}\t]+(?:97[89][\p{Zs}\t-]?)?(?:\d[\p{Zs}\t-]?){9}[\dx]\b)/giu;
            config.regexMagicLinks = new RegExp(String.raw `(^|[^\p{L}\p{N}_])(?:(?:${config.protocol})(${string_1.extUrlCharFirst}${string_1.extUrlChar})|${magicLinkPattern})`, 'giu');
        }
        catch /* c8 ignore start */ {
            // eslint-disable-next-line @typescript-eslint/no-unused-expressions
            /(^|\W)(?:(?:ftp:\/\/|http:\/\/)((?:\[[\da-f:.]+\]|[^[\]<>"\s])[^[\]<>"\0\s]*)|(?:rfc|pmid)\s+\d+\b|isbn\s+(?:97[89][\s-]?)?(?:\d[\s-]?){9}[\dx]\b)/giu;
            config.regexMagicLinks = new RegExp(String.raw `(^|\W)(?:(?:${config.protocol})(${string_1.extUrlCharFirst}${string_1.extUrlChar})|${magicLinkPattern})`, 'giu');
        }
        /* c8 ignore stop */
    }
    return wikitext.replace(config.regexMagicLinks, (m, lead, p1) => {
        let url = lead ? m.slice(lead.length) : m;
        if (p1) {
            let trail = '';
            const m2 = /&(?:[lg]t|nbsp|#x0*(?:3[ce]|a0)|#0*(?:6[02]|160));/iu.exec(url);
            if (m2) {
                trail = url.slice(m2.index);
                url = url.slice(0, m2.index);
            }
            const sep = url.includes('(') ? /[^,;\\.:!?][,;\\.:!?]+$/u : /[^,;\\.:!?)][,;\\.:!?)]+$/u, sepChars = sep.exec(url);
            if (sepChars) {
                let correction = 1;
                if (sepChars[0][1] === ';'
                    && /&(?:[a-z]+|#x[\da-f]+|#\d+)$/iu.test(url.slice(0, sepChars.index))) {
                    correction = 2;
                }
                trail = url.slice(sepChars.index + correction) + trail;
                url = url.slice(0, sepChars.index + correction);
            }
            if (trail.length >= p1.length) {
                return m;
            }
            // @ts-expect-error abstract class
            new magicLink_1.MagicLinkToken(url, undefined, config, accum);
            return `${lead}\0${accum.length - 1}w\x7F${trail}`;
        }
        else if (!/^(?:RFC|PMID|ISBN)/u.test(url)) {
            return m;
        }
        // @ts-expect-error abstract class
        new magicLink_1.MagicLinkToken(url, 'magic-link', config, accum);
        return `${lead}\0${accum.length - 1}i\x7F`;
    });
};
exports.parseMagicLinks = parseMagicLinks;
constants_1.parsers['parseMagicLinks'] = __filename;
