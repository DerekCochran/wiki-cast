"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.parseQuotes = void 0;
const quote_1 = require("../src/nowiki/quote");
/* NOT FOR BROWSER */
const constants_1 = require("../util/constants");
/* NOT FOR BROWSER END */
/**
 * 解析单引号
 * @param wikitext
 * @param config
 * @param accum
 * @param tidy 是否整理
 */
const parseQuotes = (wikitext, config, accum, tidy) => {
    const arr = wikitext.split(/('{2,})/u), { length } = arr;
    if (length === 1) {
        return wikitext;
    }
    let nBold = 0, nItalic = 0, firstSingle, firstMulti, firstSpace;
    for (let i = 1; i < length; i += 2) {
        const len = arr[i].length;
        switch (len) {
            case 2:
                nItalic++;
                break;
            case 4:
                arr[i - 1] += `'`;
                arr[i] = `'''`;
            // fall through
            case 3:
                nBold++;
                if (firstSingle !== undefined) {
                    break;
                }
                else if (arr[i - 1].endsWith(' ')) {
                    if (firstMulti === undefined && firstSpace === undefined) {
                        firstSpace = i;
                    }
                }
                else if (arr[i - 1].slice(-2, -1) === ' ') {
                    firstSingle = i;
                }
                else {
                    firstMulti ??= i;
                }
                break;
            default:
                arr[i - 1] += `'`.repeat(len - 5);
                arr[i] = `'''''`;
                nItalic++;
                nBold++;
        }
    }
    if (nItalic % 2 === 1 && nBold % 2 === 1) {
        const i = firstSingle ?? firstMulti ?? firstSpace;
        if (i !== undefined) {
            arr[i] = `''`;
            arr[i - 1] += `'`;
        }
    }
    let bold = false, italic = false;
    for (let i = 1; i < length; i += 2) {
        const n = arr[i].length, isBold = n !== 2, isItalic = n !== 3, 
        // @ts-expect-error abstract class
        token = new quote_1.QuoteToken(arr[i], { bold: isBold && Boolean(bold), italic: isItalic && Boolean(italic) }, config, accum);
        if (isBold) {
            /* NOT FOR BROWSER */
            if (!tidy && bold) {
                bold.setAttribute('bold', token);
                token.setAttribute('bold', bold);
            }
            /* NOT FOR BROWSER END */
            bold = !bold && token;
        }
        if (isItalic) {
            /* NOT FOR BROWSER */
            if (!tidy && italic) {
                italic.setAttribute('italic', token);
                token.setAttribute('italic', italic);
            }
            /* NOT FOR BROWSER END */
            italic = !italic && token;
        }
        arr[i] = `\0${accum.length - 1}q\x7F`;
    }
    /* NOT FOR BROWSER */
    if (tidy && (bold || italic)) {
        // @ts-expect-error abstract class
        new quote_1.QuoteToken((bold ? `'''` : '') + (italic ? `''` : ''), { bold, italic }, config, accum);
        arr.push(`\0${accum.length - 1}q\x7F`);
    }
    /* NOT FOR BROWSER END */
    return arr.join('');
};
exports.parseQuotes = parseQuotes;
constants_1.parsers['parseQuotes'] = __filename;
