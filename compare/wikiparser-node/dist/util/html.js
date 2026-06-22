"use strict";
/* NOT FOR BROWSER */
Object.defineProperty(exports, "__esModule", { value: true });
exports.getId = exports.html = exports.getCommon = void 0;
const string_1 = require("./string");
/* NOT FOR BROWSER END */
/**
 * get common prefix length
 * @param prefix
 * @param lastPrefix
 */
const getCommon = (prefix, lastPrefix) => prefix.startsWith(lastPrefix) ? lastPrefix.length : [...lastPrefix].findIndex((ch, i) => ch !== prefix[i]);
exports.getCommon = getCommon;
/* NOT FOR BROWSER */
/**
 * get next list item
 * @param char list syntax
 * @param state
 * @param state.dt
 */
const nextItem = (char, { dt }) => {
    if (char === '*' || char === '#') {
        return '</li>\n<li>';
    }
    const { length } = dt, close = dt[length - 1] ? '</dt>\n' : '</dd>\n';
    if (char === ';') {
        dt[length - 1] = true;
        return `${close}<dt>`;
    }
    dt[length - 1] = false;
    return `${close}<dd>`;
};
/**
 * close list item
 * @param chars list syntax
 * @param state
 * @param state.dt
 */
const closeList = (chars, { dt }) => {
    let result = '';
    for (let i = chars.length - 1; i >= 0; i--) {
        const char = chars[i];
        switch (char) {
            case '*':
            case '#':
                result += `</li></${char === '*' ? 'ul' : 'ol'}>`;
                break;
            case ':':
                result += `</${dt.pop() ? 'dt' : 'dd'}></dl>`;
            // no default
        }
    }
    return result;
};
/**
 * open list item
 * @param chars list syntax
 * @param state
 * @param state.dt
 */
const openList = (chars, { dt }) => {
    let result = '';
    for (const char of chars) {
        switch (char) {
            case '*':
            case '#':
                result += `<${char === '*' ? 'ul' : 'ol'}><li>`;
                break;
            case ':':
                dt.push(false);
                result += '<dl><dd>';
                break;
            default:
                dt.push(true);
                result += '<dl><dt>';
        }
    }
    return result;
};
/**
 * convert to HTML
 * @param childNodes a Token's contents
 * @param separator delimiter between nodes
 * @param opt options
 */
const html = (childNodes, separator, opt = {}) => {
    let lastPrefix = '';
    const results = [], { removeBlank } = opt, state = { dt: [] };
    delete opt.removeBlank;
    for (let j = 0; j < childNodes.length; j++) {
        const child = childNodes[j];
        let result = child.toHtmlInternal(opt);
        if (child.is('list-range')) {
            const { previousSibling } = child, { innerText } = previousSibling;
            if ((child.length > 0 || /\s$/u.test(innerText))
                && previousSibling.is('list')
                && !/[;#*]/u.test(innerText)
                && child.closest('ext-inner#poem,list-range')?.type === 'ext-inner') {
                lastPrefix = '';
                result = `<span style="display: inline-block; margin-inline-start: ${previousSibling.indent}em;">${result}</span>`;
            }
            else {
                result = result.trim();
                const prefix = innerText.trim(), prefix2 = prefix.replaceAll(';', ':'), commonPrefixLength = (0, exports.getCommon)(prefix2, lastPrefix);
                let pre = closeList(lastPrefix.slice(commonPrefixLength), state);
                if (prefix.length === commonPrefixLength) {
                    pre += nextItem(prefix.slice(-1), state);
                }
                else {
                    if (state.dt.at(-1) && prefix[commonPrefixLength - 1] === ':') {
                        pre += nextItem(':', state);
                    }
                    if (lastPrefix) {
                        pre += '\n';
                    }
                    pre += openList(prefix.slice(commonPrefixLength), state);
                }
                result = pre + result;
                let { nextSibling } = child;
                while (nextSibling?.is('dd')) {
                    const next = nextSibling.nextSibling;
                    result += nextItem(':', state) + next.toHtmlInternal(opt).trim();
                    ({ nextSibling } = next);
                    j += 2;
                }
                if (nextSibling?.type === 'text'
                    && nextSibling.data === '\n'
                    && nextSibling.nextVisibleSibling?.is('list')) {
                    j += 2;
                    lastPrefix = prefix2;
                }
                else {
                    lastPrefix = '';
                    result += closeList(prefix2, state);
                }
            }
        }
        results.push(result);
    }
    return (removeBlank ? results.filter(result => result !== '') : results).join(separator);
};
exports.html = html;
/**
 * get the id of a section heading
 * @param tokens inner tokens of a section heading
 */
const getId = (tokens) => {
    let content;
    if (typeof tokens === 'string') {
        content = tokens;
    }
    else {
        const opt = { nocc: true };
        content = Array.isArray(tokens) ? (0, exports.html)(tokens, '', opt) : tokens.toHtmlInternal(opt);
    }
    const id = (0, string_1.decodeHtml)((0, string_1.sanitizeAlt)(content.replaceAll('_', ' ')))
        .replace(/[\s_]+/gu, '_');
    return id.endsWith('_') ? id.slice(0, -1) : id;
};
exports.getId = getId;
