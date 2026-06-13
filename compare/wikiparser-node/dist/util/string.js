"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.isInterwiki = exports.removeCommentLine = exports.newline = exports.sanitizeAlt = exports.sanitizeId = exports.sanitizeAttr = exports.sanitize = exports.normalizeSpace = exports.encode = exports.noWrap = exports.escapeRegExp = exports.print = exports.escape = exports.replaceEntities = exports.decodeNumber = exports.decodeHtml = exports.decodeHtmlBasic = exports.text = exports.removeComment = exports.tidy = exports.trimLc = exports.extUrlChar = exports.extUrlCharFirst = exports.zs = void 0;
exports.restore = restore;
/* NOT FOR BROWSER */
const common_1 = require("@bhsd/common");
/* NOT FOR BROWSER END */
exports.zs = String.raw ` \xA0\u1680\u2000-\u200A\u202F\u205F\u3000`;
const commonExtUrlChar = String.raw `[^[\]<>"\0-\x1F\x7F${exports.zs}\uFFFD]`;
exports.extUrlCharFirst = String.raw `(?:\[[\da-f:.]+\]|${commonExtUrlChar})`;
exports.extUrlChar = String.raw `(?:${commonExtUrlChar}|\0\d+[cn!~]\x7F)*`;
/**
 * trim and toLowerCase
 * @param s 字符串
 */
const trimLc = (s) => s.trim().toLowerCase();
exports.trimLc = trimLc;
function restore(s, stack, translate) {
    if (translate === 1) {
        return s.replace(/\0(\d+)g\x7F/gu, (_, p1) => restore(String(stack[p1]), stack, 2));
    }
    else if (translate === 2) {
        return s.replace(/\0(\d+)n\x7F/gu, (_, p1) => String(stack[p1]));
    }
    return s.replace(/\0(\d+)\x7F/gu, (_, p1) => stack[p1]);
}
/**
 * 生成正则替换函数
 * @param regex 正则表达式
 * @param replace 替换字符串或函数
 */
const factory = (regex, replace) => (str) => str.replace(regex, replace);
/** 清理解析专用的不可见字符 */
exports.tidy = factory(/[\0\x7F]|\r$/gmu, '');
/** remove half-parsed comment-like tokens */
exports.removeComment = factory(/\0\d+[cn]\x7F/gu, '');
/**
 * extract effective wikitext
 * @param childNodes a Token's contents
 * @param separator delimiter between nodes
 */
const text = (childNodes, separator = '') => childNodes.map(child => typeof child === 'string' ? child : child.text()).join(separator);
exports.text = text;
const names = { lt: '<', gt: '>', lbrack: '[', rbrack: ']', lbrace: '{', rbrace: '}', nbsp: ' ', amp: '&', quot: '"' };
/** decode HTML entities */
exports.decodeHtmlBasic = factory(/&(?:#(\d+|[Xx][\da-fA-F]+)|([lg]t|[LG]T|[lr]brac[ke]|nbsp|amp|AMP|quot|QUOT));/gu, (_, code, name) => code
    ? String.fromCodePoint(Number((/^x/iu.test(code) ? '0' : '') + code))
    : names[name.toLowerCase()]);
let decodeHtmlResolved;
/**
 * decode HTML entities
 * @param str
 */
const decodeHtml = (str) => {
    decodeHtmlResolved ??= (() => {
        /* NOT FOR BROWSER ONLY */
        // eslint-disable-next-line @typescript-eslint/no-unnecessary-condition
        if (typeof process === 'object' && typeof process.versions?.node === 'string') {
            try {
                const { decodeHTMLStrict } = require('entities');
                return (s) => decodeHTMLStrict(s).replace(/\xA0/gu, ' ');
            }
            catch /* c8 ignore next */ { }
        }
        /* NOT FOR BROWSER ONLY END */
        /* c8 ignore next */
        return exports.decodeHtmlBasic;
    })();
    return decodeHtmlResolved(str);
};
exports.decodeHtml = decodeHtml;
/** decode numbered HTML entities */
exports.decodeNumber = factory(/&#(\d+|x[\da-f]+);/giu, (_, code) => String.fromCodePoint(Number((/^x/iu.test(code) ? '0' : '') + code)));
/* PRINT ONLY */
const entities = {
    '&': 'amp',
    '<': 'lt',
    '>': 'gt',
    '"': 'quot',
    '\n': '#10',
    /* NOT FOR BROWSER */
    '{': '#123',
    '}': '#125',
    '[': '#91',
    ']': '#93',
    '|': '#124',
};
/**
 * replace by HTML entities
 * @param re regex
 */
const replaceEntities = (re = /[&<>"{}[\]|]/gu) => factory(re, p => `&${entities[p]};`);
exports.replaceEntities = replaceEntities;
/** escape HTML entities */
exports.escape = (0, exports.replaceEntities)(/[&<>]/gu);
/**
 * 以HTML格式打印
 * @param childNodes 子节点
 * @param opt 选项
 */
const print = (childNodes, opt = {}) => {
    const { pre = '', post = '', sep = '' } = opt;
    return pre + childNodes.map(child => child.print()).join(sep) + post;
};
exports.print = print;
/* PRINT ONLY END */
/* NOT FOR BROWSER */
/** escape special chars for RegExp constructor */
// eslint-disable-next-line @typescript-eslint/no-unnecessary-condition
exports.escapeRegExp = RegExp.escape || factory(/[\\{}()|.?*+^$[\]]/gu, String.raw `\$&`);
/** escape newlines */
exports.noWrap = factory(/\n/gu, String.raw `\n`);
/** encode URI */
exports.encode = factory(/[<>[\]#|=]+/gu, encodeURIComponent);
/**
 * convert newline in text nodes to single whitespace
 * @param token 父节点
 */
const normalizeSpace = (token) => {
    if (token) {
        for (const child of token.childNodes) {
            if (child.type === 'text' && child.data.includes('\n')) {
                child.replaceData(child.data.replace(/\n+/gu, ' '));
            }
        }
    }
};
exports.normalizeSpace = normalizeSpace;
/** escape HTML entities */
exports.sanitize = (0, exports.replaceEntities)(/[<>]|&(?=amp(?!;))/giu);
const replaceAttrEntities = (0, exports.replaceEntities)(/[<>"]/gu);
/**
 * escape HTML entities in attributes
 * @param attr 属性值
 * @param id 是否是`id`属性
 */
const sanitizeAttr = (attr, id) => replaceAttrEntities(attr.replace(/\s+|&#10;/gu, id ? '_' : ' '));
exports.sanitizeAttr = sanitizeAttr;
/** escape HTML entities in heading id */
exports.sanitizeId = (0, exports.replaceEntities)(/["<>&]/gu);
/**
 * sanitize selected HTML attributes
 * @param str attribute value
 */
const sanitizeAlt = (str) => str?.replace(/<\/?[a-z].*?>/gu, '').trim()
    .replace(/\s+|"/gu, m => m === '"' ? '&quot;' : ' ');
exports.sanitizeAlt = sanitizeAlt;
/** escape newline */
exports.newline = factory(/\n/gu, '&#10;');
/**
 * remove lines that only contain comments
 * @param str
 * @param standalone whether for a standalone document
 */
const removeCommentLine = (str, standalone) => {
    const lines = str.split('\n'), { length } = lines;
    if (!standalone && length < 3) {
        return (0, exports.removeComment)(str);
    }
    const offset = standalone ? 0 : 1, end = length - offset;
    return (0, exports.removeComment)([
        ...lines.slice(0, offset),
        ...lines.slice(offset, end).filter(line => !/^(?!\s*$)(?:\s|\0\d+c\x7F)*$/u.test(line)),
        ...lines.slice(end),
    ].join('\n'));
};
exports.removeCommentLine = removeCommentLine;
/^(zh|en)\s*:/diu; // eslint-disable-line @typescript-eslint/no-unused-expressions
const getInterwikiRegex = (0, common_1.getRegex)(interwiki => new RegExp(String.raw `^(${interwiki.join('|')})\s*:`, 'diu'));
/**
 * 是否是跨维基链接
 * @param title 链接标题
 * @param config
 * @param config.interwiki 跨维基前缀列表
 */
const isInterwiki = (title, { interwiki }) => interwiki.length > 0
    ? getInterwikiRegex(interwiki).exec(title.replaceAll('_', ' ').replace(/^\s*:?\s*/u, ''))
    : null;
exports.isInterwiki = isInterwiki;
