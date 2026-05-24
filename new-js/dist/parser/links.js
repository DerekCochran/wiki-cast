"use strict";
var __importDefault = (this && this.__importDefault) || function (mod) {
    return (mod && mod.__esModule) ? mod : { "default": mod };
};
Object.defineProperty(exports, "__esModule", { value: true });
exports.parseLinks = void 0;
const string_1 = require("../util/string");
const index_1 = __importDefault(require("../index"));
const quotes_1 = require("./quotes");
const externalLinks_1 = require("./externalLinks");
const index_2 = require("../src/link/index");
const file_1 = require("../src/link/file");
const category_1 = require("../src/link/category");
/* NOT FOR BROWSER */
const constants_1 = require("../util/constants");
/* NOT FOR BROWSER END */
const regexImg = /^((?:(?!\0\d+!\x7F)[^\n[\]{}|])+)(\||\0\d+!\x7F)([\s\S]*)$/u;
/**
 * 解析内部链接
 * @param wikitext
 * @param config
 * @param accum
 * @param page 页面名称
 * @param tidy 是否整理链接
 */
const parseLinks = (wikitext, config, accum, page, tidy) => {
    /^\s*(?:ftp:\/\/|\/\/)/iu; // eslint-disable-line @typescript-eslint/no-unused-expressions
    config.regexLinks ??= new RegExp(String.raw `^\s*(?:${config.protocol}|//)`, 'iu');
    const regex = config.inExt
        ? /^((?:(?!\0\d+!\x7F)[^\n[\]{}|])+)(?:(\||\0\d+!\x7F)([\s\S]*?[^\]]))?\]\]([\s\S]*)$/u
        : /^((?:(?!\0\d+!\x7F)[^\n[\]{}|])*)(?:(\||\0\d+!\x7F)([\s\S]*?[^\]])?)?\]\]([\s\S]*)$/u, bits = wikitext.split('[[');
    let s = bits.shift();
    for (let i = 0; i < bits.length; i++) {
        let mightBeImg = false, link, delimiter, text, after;
        const x = bits[i], m = regex.exec(x);
        if (m) {
            [, link, delimiter, text, after] = m;
            if (after.startsWith(']') && text?.includes('[')) {
                text += ']';
                after = after.slice(1);
            }
        }
        else {
            const m2 = regexImg.exec(x);
            if (m2) {
                mightBeImg = true;
                [, link, delimiter, text] = m2;
            }
        }
        if (link === undefined || config.regexLinks.test(link) || /\0\d+[exhbru]\x7F/u.test(link)) {
            s += `[[${x}`;
            continue;
        }
        // eslint-disable-next-line prefer-const
        let trimmed = (0, string_1.removeComment)(link).trim();
        const force = trimmed.startsWith(':');
        if (force && mightBeImg) {
            s += `[[${x}`;
            continue;
        }
        const { ns, valid, 
        /* NOT FOR BROWSER */
        interwiki, } = index_1.default.normalizeTitle(trimmed, 0, false, config, { halfParsed: true, temporary: true, decode: true, selfLink: true, page });
        if (!valid) {
            s += `[[${x}`;
            continue;
        }
        else if (mightBeImg) {
            if (ns !== 6
                || interwiki) {
                s += `[[${x}`;
                continue;
            }
            let found = false;
            for (i++; i < bits.length; i++) {
                const next = bits[i], p = next.split(']]');
                if (p.length > 2) {
                    found = true;
                    text += `[[${p[0]}]]${p[1]}`;
                    after = p.slice(2).join(']]');
                    break;
                }
                else if (p.length === 2) {
                    text += `[[${p[0]}]]${p[1]}`;
                }
                else {
                    text += `[[${next}`;
                    break;
                }
            }
            text = (0, exports.parseLinks)(text, config, accum, page, tidy);
            if (!found) {
                s += `[[${link}${delimiter}${text}`;
                continue;
            }
        }
        text &&= (0, quotes_1.parseQuotes)(text, config, accum, tidy);
        let SomeLinkToken = index_2.LinkToken;
        if (!force) {
            if (ns === 6
                && !interwiki) {
                text &&= (0, externalLinks_1.parseExternalLinks)(text, config, accum, true);
                SomeLinkToken = file_1.FileToken;
            }
            else if (ns === 14
                && !interwiki) {
                SomeLinkToken = category_1.CategoryToken;
            }
        }
        if (text === undefined && delimiter) {
            text = '';
        }
        s += `\0${accum.length}l\x7F${after}`;
        // @ts-expect-error abstract class
        new SomeLinkToken(link, text, config, accum, delimiter);
    }
    return s;
};
exports.parseLinks = parseLinks;
constants_1.parsers['parseLinks'] = __filename;
