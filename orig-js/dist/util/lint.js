"use strict";
var __importDefault = (this && this.__importDefault) || function (mod) {
    return (mod && mod.__esModule) ? mod : { "default": mod };
};
Object.defineProperty(exports, "__esModule", { value: true });
exports.provideValues = exports.cache = exports.fixByPipe = exports.fixByEscape = exports.fixBySpace = exports.fixByUpper = exports.fixByComment = exports.fixByOpen = exports.fixByClose = exports.fixByDecode = exports.fixByRemove = exports.fixByInsert = exports.fixBy = exports.generateForSelf = exports.generateForChild = exports.getEndPos = exports.isFostered = void 0;
const common_1 = require("@bhsd/common");
const debug_1 = require("./debug");
const rect_1 = require("../lib/rect");
const index_1 = __importDefault(require("../index"));
const tableTags = new Set(['tr', 'td', 'th', 'caption']), tableTemplates = new Set(['Template:!!', 'Template:!-']);
/**
 * Check if the content is fostered
 * @param token
 */
const isFostered = (token) => {
    const first = token.childNodes.find(child => child.text().trim());
    if (!first
        || first.type === 'text' && first.data.trim().startsWith('!')
        || first.is('magic-word') && first.name === '!'
        || first.is('template') && tableTemplates.has(first.name)
        || first.is('html') && tableTags.has(first.name)) {
        return false;
    }
    else if (first.is('arg')) {
        return first.length > 1 && (0, exports.isFostered)(first.childNodes[1]);
    }
    else if (first.is('magic-word')) {
        try {
            const severity = first.getPossibleValues().map(exports.isFostered);
            return severity.includes(2) ? 2 : severity.includes(1) && 1;
        }
        catch { }
    }
    return first.is('template')
        || first.is('magic-word') && first.name === 'invoke'
        ? 1
        : 2;
};
exports.isFostered = isFostered;
/**
 * 计算结束位置
 * @param top 起始行
 * @param left 起始列
 * @param height 高度
 * @param width 宽度
 */
const getEndPos = (top, left, height, width) => ({
    line: top + height - 1,
    character: (height === 1 ? left : 0) + width,
});
exports.getEndPos = getEndPos;
/**
 * 生成lint函数
 * @param func lint函数
 */
const factory = (func) => (token, rect, rule, msg, severity = 'error') => {
    const { start } = rect, { top, left } = rect instanceof rect_1.BoundingRect ? rect : new rect_1.BoundingRect(token, start), { offsetHeight, offsetWidth } = token, { startIndex, startLine, startCol } = func(token, start, top, left), { line, character } = (0, exports.getEndPos)(startLine, startCol, offsetHeight, offsetWidth);
    return {
        rule,
        message: index_1.default.msg(msg),
        severity,
        startIndex,
        endIndex: startIndex + token.toString().length,
        startLine,
        endLine: line,
        startCol,
        endCol: character,
    };
};
exports.generateForChild = factory((child, start, line, col) => {
    const index = child.getRelativeIndex(), { top, left } = child.parentNode.posFromIndex(index);
    return {
        startIndex: start + index,
        startLine: line + top,
        startCol: top ? left : col + left,
    };
});
exports.generateForSelf = 
/* #__PURE__ */ factory((_, startIndex, startLine, startCol) => ({ startIndex, startLine, startCol }));
/**
 * Quick fix
 * @param e LintError
 * @param desc description of the fix
 * @param text replacement text
 * @param offset offset to the start index
 */
const fixBy = (e, desc, text, offset = 0) => ({ desc: index_1.default.msg(desc), range: [e.startIndex + offset, e.endIndex], text });
exports.fixBy = fixBy;
/**
 * Quick fix: insert the text
 * @param index the index to insert the text
 * @param desc description of the fix
 * @param text inserted text
 */
const fixByInsert = (index, desc, text) => ({ desc: index_1.default.msg(desc), range: [index, index], text });
exports.fixByInsert = fixByInsert;
/**
 * Quick fix: remove the error
 * @param e LintError
 * @param offset offset to the start index
 * @param text replacement text
 */
const fixByRemove = (e, offset = 0, text = '') => typeof e === 'number'
    ? { desc: index_1.default.msg('remove'), range: [e, e + offset], text }
    : (0, exports.fixBy)(e, 'remove', text, offset);
exports.fixByRemove = fixByRemove;
/**
 * Quick fix: decode the link
 * @param e LintError
 * @param link the link to decode
 */
const fixByDecode = (e, link) => (0, exports.fixBy)(e, 'decode', (0, common_1.rawurldecode)(link.text().replace(/%(?=21|3[ce]|5[bd]|7[b-d])/giu, '%25')));
exports.fixByDecode = fixByDecode;
/**
 * Quick fix: close the syntax
 * @param index the index to insert the closing syntax
 * @param text the closing syntax text
 * @param offset offset to the start index
 */
const fixByClose = (index, text, offset = 0) => ({ desc: index_1.default.msg('close'), range: [index + offset, index], text });
exports.fixByClose = fixByClose;
/**
 * Quick fix: open the syntax
 * @param index the index of the tag to open
 */
const fixByOpen = (index) => ({ desc: index_1.default.msg('open'), range: [index + 1, index + 2], text: '' });
exports.fixByOpen = fixByOpen;
/**
 * Quick fix: comment out
 * @param e LintError
 * @param text the closing syntax text
 */
const fixByComment = (e, text) => (0, exports.fixBy)(e, 'comment', `<!--${text}-->`);
exports.fixByComment = fixByComment;
/**
 * Quick fix: convert to upper case
 * @param e LintError
 * @param text the closing syntax text
 */
const fixByUpper = (e, text) => (0, exports.fixBy)(e, 'uppercase', text.toUpperCase());
exports.fixByUpper = fixByUpper;
/**
 * Quick fix: insert space
 * @param index the index to insert the space
 * @param offset offset to the end index
 */
const fixBySpace = (index, offset = 0) => ({ desc: index_1.default.msg('whitespace'), range: [index, index + offset], text: ' ' });
exports.fixBySpace = fixBySpace;
/**
 * Quick fix: escape the character
 * @param index the index to escape the character
 * @param char the escaped character
 * @param offset offset to the end index
 */
const fixByEscape = (index, char, offset = 1) => ({ desc: index_1.default.msg('escape'), range: [index, index + offset], text: char.repeat(offset) });
exports.fixByEscape = fixByEscape;
/**
 * Quick fix: escape the `|` character
 * @param index the index to escape the character
 * @param text the text to be replaced
 */
const fixByPipe = (index, text) => ({
    desc: index_1.default.msg('escape'),
    range: [index, index + text.length],
    text: text.replace(/\|/gu, '&#124;'),
});
exports.fixByPipe = fixByPipe;
/**
 * 缓存计算结果
 * @param store 缓存的值
 * @param compute 计算新值的函数
 * @param update 更新缓存的函数
 * @param force 是否强制缓存
 */
const cache = (store, compute, update, force) => {
    if (store && (force || index_1.default.viewOnly && store[0] === debug_1.Shadow.rev)) {
        return store[1];
    }
    const result = compute();
    if (force || index_1.default.viewOnly) {
        update([debug_1.Shadow.rev, result]);
    }
    return result;
};
exports.cache = cache;
/**
 * 获取HTML属性值可选列表
 * @param tag 标签名
 * @param attribute 属性名
 */
const provideValues = (tag, attribute) => {
    if (tag === 'ol' && attribute === 'type') {
        return ['1', 'a', 'A', 'i', 'I'];
    }
    else if (tag === 'th' && attribute === 'scope') {
        return ['row', 'col', 'rowgroup', 'colgroup'];
    }
    else if (attribute === 'dir') {
        return ['ltr', 'rtl', 'auto'];
    }
    return attribute === 'aria-hidden' ? ['true', 'false'] : [];
};
exports.provideValues = provideValues;
