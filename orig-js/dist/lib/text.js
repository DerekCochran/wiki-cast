"use strict";
var __runInitializers = (this && this.__runInitializers) || function (thisArg, initializers, value) {
    var useValue = arguments.length > 2;
    for (var i = 0; i < initializers.length; i++) {
        value = useValue ? initializers[i].call(thisArg, value) : initializers[i].call(thisArg);
    }
    return useValue ? value : void 0;
};
var __esDecorate = (this && this.__esDecorate) || function (ctor, descriptorIn, decorators, contextIn, initializers, extraInitializers) {
    function accept(f) { if (f !== void 0 && typeof f !== "function") throw new TypeError("Function expected"); return f; }
    var kind = contextIn.kind, key = kind === "getter" ? "get" : kind === "setter" ? "set" : "value";
    var target = !descriptorIn && ctor ? contextIn["static"] ? ctor : ctor.prototype : null;
    var descriptor = descriptorIn || (target ? Object.getOwnPropertyDescriptor(target, contextIn.name) : {});
    var _, done = false;
    for (var i = decorators.length - 1; i >= 0; i--) {
        var context = {};
        for (var p in contextIn) context[p] = p === "access" ? {} : contextIn[p];
        for (var p in contextIn.access) context.access[p] = contextIn.access[p];
        context.addInitializer = function (f) { if (done) throw new TypeError("Cannot add initializers after decoration has completed"); extraInitializers.push(accept(f || null)); };
        var result = (0, decorators[i])(kind === "accessor" ? { get: descriptor.get, set: descriptor.set } : descriptor[key], context);
        if (kind === "accessor") {
            if (result === void 0) continue;
            if (result === null || typeof result !== "object") throw new TypeError("Object expected");
            if (_ = accept(result.get)) descriptor.get = _;
            if (_ = accept(result.set)) descriptor.set = _;
            if (_ = accept(result.init)) initializers.unshift(_);
        }
        else if (_ = accept(result)) {
            if (kind === "field") initializers.unshift(_);
            else descriptor[key] = _;
        }
    }
    if (target) Object.defineProperty(target, contextIn.name, descriptor);
    done = true;
};
var __setFunctionName = (this && this.__setFunctionName) || function (f, name, prefix) {
    if (typeof name === "symbol") name = name.description ? "[".concat(name.description, "]") : "";
    return Object.defineProperty(f, "name", { configurable: true, value: prefix ? "".concat(prefix, " ", name) : name });
};
var __importDefault = (this && this.__importDefault) || function (mod) {
    return (mod && mod.__esModule) ? mod : { "default": mod };
};
Object.defineProperty(exports, "__esModule", { value: true });
exports.AstText = void 0;
const string_1 = require("../util/string");
const lint_1 = require("../util/lint");
const debug_1 = require("../util/debug");
const index_1 = __importDefault(require("../index"));
const node_1 = require("./node");
/* NOT FOR BROWSER */
const constants_1 = require("../util/constants");
const readOnly_1 = require("../mixin/readOnly");
const cached_1 = require("../mixin/cached");
/* NOT FOR BROWSER END */
const sp = /* #__PURE__ */ (() => String.raw `[${string_1.zs}\t]*`)(), source = /* #__PURE__ */ (() => String.raw `<(?:/[^\S\n]*)?([a-z]\w*)|\{+|\}+|\[{2,}|\[(?![^[]*?\])|((?:^|\])[^[]*?)\]+|\n={2,}`)();
// eslint-disable-next-line @typescript-eslint/no-unused-expressions
/\bhttps?[:/]\/+|\b(?:rfc|pmid)(?=[-:：]?\s*\d)|\bisbn(?=[-:：]?\s*(?:\d(?:\s*|-)){6})/giu;
const errorSyntax = /* #__PURE__ */ (() => new RegExp(String.raw `${source}|\bhttps?[:/]/+|\b(?:rfc|pmid)(?=[-:：]?${sp}\d)|\bisbn(?=[-:：]?${sp}(?:\d(?:${sp}|-)){6})`, 'giu'))();
// eslint-disable-next-line @typescript-eslint/no-unused-expressions
/<(?:\/[^\S\n]*)?([a-z]\w*)|\{+|\}+|\[{2,}|\[(?![^[]*?\])|((?:^|\])[^[]*?)\]+|\n={2,}/giu;
const errorSyntaxUrl = /* #__PURE__ */ new RegExp(source, 'giu'), noLinkTypes = new Set(['attr-value', 'ext-link-text', 'link-text']), regexes = {
    '[': /[[\]]/u,
    '{': /[{}]/u,
    ']': /[[\]](?=[^[\]]*$)/u,
    '}': /[{}](?=[^{}]*$)/u,
}, disallowedTags = new Set([
    'html',
    'head',
    'style',
    'title',
    'body',
    'a',
    'audio',
    'img',
    'video',
    'embed',
    'iframe',
    'object',
    'canvas',
    'script',
    'col',
    'colgroup',
    'tbody',
    'tfoot',
    'thead',
    'button',
    'input',
    'label',
    'option',
    'select',
    'textarea',
    /* NOT FOR BROWSER */
    'base',
    'menu',
    'area',
    'map',
    'track',
    'picture',
    'source',
    'datalist',
    'fieldset',
    'form',
    'legend',
    'meter',
    'optgroup',
    'output',
    'progress',
    'details',
    'dialog',
    'slot',
    'template',
    'dir',
    'frame',
    'frameset',
    'marquee',
    'param',
    'xmp',
]);
const wordRegex = /* #__PURE__ */ (() => {
    try {
        // eslint-disable-next-line prefer-regex-literals
        return new RegExp(String.raw `[\p{L}\p{N}_]`, 'u');
    }
    catch /* c8 ignore start */ {
        return /\w/u;
    }
    /* c8 ignore stop */
})();
/**
 * text node
 *
 * 文本节点
 */
let AstText = (() => {
    let _classSuper = node_1.AstNode;
    let _instanceExtraInitializers = [];
    let _private_setData_decorators;
    let _private_setData_descriptor;
    let _toHtmlInternal_decorators;
    return class AstText extends _classSuper {
        static {
            const _metadata = typeof Symbol === "function" && Symbol.metadata ? Object.create(_classSuper[Symbol.metadata] ?? null) : void 0;
            _private_setData_decorators = [(0, readOnly_1.readOnly)()];
            _toHtmlInternal_decorators = [(0, cached_1.cached)()];
            __esDecorate(this, _private_setData_descriptor = { value: __setFunctionName(function (text) {
                    /* NOT FOR BROWSER */
                    const { data } = this, e = new Event('text', { bubbles: true });
                    /* NOT FOR BROWSER END */
                    this.setAttribute('data', text);
                    /* NOT FOR BROWSER */
                    if (data !== text) {
                        this.dispatchEvent(e, { type: 'text', oldText: data });
                    }
                }, "#setData") }, _private_setData_decorators, { kind: "method", name: "#setData", static: false, private: true, access: { has: obj => #setData in obj, get: obj => obj.#setData }, metadata: _metadata }, null, _instanceExtraInitializers);
            __esDecorate(this, null, _toHtmlInternal_decorators, { kind: "method", name: "toHtmlInternal", static: false, private: false, access: { has: obj => "toHtmlInternal" in obj, get: obj => obj.toHtmlInternal }, metadata: _metadata }, null, _instanceExtraInitializers);
            if (_metadata) Object.defineProperty(this, Symbol.metadata, { enumerable: true, configurable: true, writable: true, value: _metadata });
        }
        data = (__runInitializers(this, _instanceExtraInitializers), '');
        get type() {
            return 'text';
        }
        /* NOT FOR BROWSER */
        /** text length / 文本长度 */
        get length() {
            return this.data.length;
        }
        set length(n) {
            if (n >= 0 && n < this.length) {
                this.replaceData(this.data.slice(0, n));
            }
        }
        /* NOT FOR BROWSER END */
        /** @param text 包含文本 */
        constructor(text) {
            super();
            if (index_1.default.viewOnly || index_1.default.internal) {
                this.data = text;
                /* NOT FOR BROWSER */
            }
            else {
                Object.defineProperties(this, {
                    data: { value: text, writable: false },
                    childNodes: { enumerable: false, configurable: false },
                });
            }
        }
        /** @private */
        toString(skip) {
            return skip && !this.parentNode?.getAttribute('built') ? (0, string_1.removeComment)(this.data) : this.data;
        }
        /** @private */
        text() {
            return this.data;
        }
        /** @private */
        lint(start = this.getAbsoluteIndex(), errorRegex) {
            LINT: {
                if (errorRegex === false) {
                    return [];
                }
                const { data, parentNode, nextSibling, previousSibling } = this;
                /* c8 ignore next 3 */
                if (!parentNode) {
                    throw new Error('An isolated text node cannot be linted!');
                }
                const { type, parentNode: grandparent } = parentNode;
                if (type === 'attr-value') {
                    const { name, tag } = grandparent;
                    if (tag === 'ref' && (name === 'name' || name === 'follow')
                        || name === 'group' && (tag === 'ref' || tag === 'references')
                        || tag === 'choose' && (name === 'before' || name === 'after')) {
                        return [];
                    }
                }
                errorRegex ??= parentNode.isPlain() && !noLinkTypes.has(type) ? errorSyntax : errorSyntaxUrl;
                if (data.search(errorRegex) === -1) {
                    return [];
                }
                errorRegex.lastIndex = 0;
                const errors = [], nextType = nextSibling?.type, nextName = nextSibling?.name, previousType = previousSibling?.type, root = this.getRootNode(), rootStr = root.toString(), { ext, html, variants } = root.getAttribute('config'), { top, left } = root.posFromIndex(start), { lintConfig } = index_1.default, tagLike = lintConfig.rules['tag-like'], specified = typeof tagLike === 'object' && tagLike[1]
                    ? new Set(Object.keys(tagLike[1]).filter(tag => tag !== 'invalid' && tag !== 'disallowed'))
                    : new Set(), tags = new Set([
                    'onlyinclude',
                    'noinclude',
                    'includeonly',
                    ...ext,
                    ...html[0],
                    ...html[1],
                    ...html[2],
                    ...specified,
                    ...lintConfig.getSeverity('tag-like', 'disallowed') ? disallowedTags : [],
                ]);
                for (let mt = errorRegex.exec(data); mt; mt = errorRegex.exec(data)) {
                    let { index, 0: error } = mt;
                    const [, tag, prefix] = mt, lbrackInExtLinkText = error === '[' && type === 'ext-link-text';
                    if (prefix && prefix !== ']') {
                        const { length } = prefix;
                        index += length;
                        error = error.slice(length);
                    }
                    else if (error.startsWith('<') && !tags.has(tag.toLowerCase())
                        || lbrackInExtLinkText && (/&(?:rbrack|#93|#x5[Dd];);/u.test(data.slice(index + 1))
                            || nextSibling?.is('ext') && nextName === 'nowiki'
                                && nextSibling.innerText?.includes(']'))) {
                        continue;
                    }
                    else if (error.startsWith('\n==')) {
                        index++;
                        error = error.slice(1);
                    }
                    error = error.toLowerCase();
                    const [char] = error, magicLink = error === 'rfc' || error === 'pmid' || error === 'isbn', lbrace = char === '{', rbrace = char === '}', lbrack = char === '[', rbrack = char === ']';
                    let { length } = error;
                    if (rbrack && (index || length > 1)) {
                        errorRegex.lastIndex--;
                    }
                    // Rule & Severity
                    let startIndex = start + index, endIndex = startIndex + length, rule, severity, endLine, endCol;
                    const nextChar = rootStr[endIndex], previousChar = rootStr[startIndex - 1], leftBracket = lbrace || lbrack, lConverter = error === '{' && previousChar === '-' && variants.length > 0, rConverter = error === '}' && nextChar === '-' && variants.length > 0, brokenExtLink = lbrack && nextType === 'free-ext-link' && !data.slice(index + 1).trim()
                        || rbrack && previousType === 'free-ext-link'
                            && !data.slice(0, index).includes(']');
                    if (magicLink) {
                        rule = 'lonely-http';
                        error = error.toUpperCase();
                        severity = lintConfig.getSeverity(rule, error);
                    }
                    else if (char === '<') {
                        rule = 'tag-like';
                        let key;
                        if (/^<\/?\s/u.test(error) || !/[\s/>]/u.test(nextChar ?? '')) {
                            key = 'invalid';
                        }
                        else if (specified.has(tag)) {
                            key = tag;
                        }
                        else if (disallowedTags.has(tag) && !ext.includes(tag)) {
                            key = 'disallowed';
                        }
                        severity = lintConfig.getSeverity(rule, key);
                    }
                    else if (char === '=') {
                        rule = 'syntax-like';
                        severity = lintConfig.getSeverity(rule, 'heading');
                    }
                    else if (lConverter || rConverter) {
                        rule = 'lonely-bracket';
                        severity = lintConfig.getSeverity(rule, 'converter');
                        if (lConverter && index > 0) {
                            error = '-{';
                            index--;
                            startIndex--;
                            length = 2;
                        }
                        else if (rConverter && index < data.length - 1) {
                            error = '}-';
                            endIndex++;
                            length = 2;
                        }
                    }
                    else if (brokenExtLink) {
                        rule = 'lonely-bracket';
                        severity = lintConfig.getSeverity(rule, 'extLink');
                    }
                    else if (leftBracket || rbrace || rbrack) {
                        rule = 'lonely-bracket';
                        if (length > 1 || lbrace && nextChar === char || rbrace && previousChar === char) {
                            severity = lintConfig.getSeverity(rule, 'double');
                        }
                        else {
                            if (!lbrackInExtLinkText) {
                                const regex = regexes[char], remains = leftBracket ? data.slice(index + 1) : data.slice(0, index);
                                if (lbrace && regex.exec(remains)?.[0] === '}'
                                    || rbrace && regex.exec(remains)?.[0] === '{') {
                                    continue;
                                }
                                else if (!remains.includes(char)) {
                                    const sibling = leftBracket ? 'nextSibling' : 'previousSibling';
                                    let cur = this[sibling];
                                    while (cur && (cur.type !== 'text' || !regex.test(cur.data))) {
                                        cur = cur[sibling];
                                    }
                                    if (cur && regex.exec(cur.data)[0] !== char) {
                                        continue;
                                    }
                                }
                            }
                            severity = lintConfig.getSeverity(rule, 'single');
                        }
                    }
                    else {
                        rule = 'lonely-http';
                        severity = lintConfig.getSeverity(rule);
                    }
                    if (!severity) {
                        continue;
                    }
                    // LintError
                    const pos = this.posFromIndex(index), { line: startLine, character: startCol } = (0, lint_1.getEndPos)(top, left, pos.top + 1, pos.left);
                    if (char === '=') {
                        const lineEnd = data.indexOf('\n', index);
                        let sibling = nextSibling, line;
                        if (lineEnd === -1) {
                            let end = 0;
                            while (sibling) {
                                if (sibling.type === 'text') {
                                    end = sibling.data.indexOf('\n');
                                    if (end !== -1) {
                                        break;
                                    }
                                }
                                sibling = sibling.nextSibling;
                            }
                            if (!sibling) {
                                continue;
                            }
                            line = sibling.data.slice(0, end);
                        }
                        else {
                            line = data.slice(index + length, lineEnd);
                        }
                        if (!/(?:^|[^=])=+\s*(?:\S\s*)?$/u.test(line)) {
                            continue;
                        }
                        if (lineEnd === -1) {
                            endIndex = sibling.getAbsoluteIndex() + line.length;
                            ({ top: endLine, left: endCol } = root.posFromIndex(endIndex));
                        }
                        else {
                            endIndex += line.length;
                            length += line.length;
                        }
                    }
                    const e = {
                        rule,
                        message: index_1.default.msg(char === '=' ? 'header-like' : 'lonely', magicLink || char === 'h' || lConverter || rConverter ? error : char),
                        severity,
                        startIndex,
                        endIndex,
                        startLine,
                        endLine: endLine ?? startLine,
                        startCol,
                        endCol: endCol ?? startCol + length,
                    };
                    // Suggestions
                    if (lintConfig.computeEditInfo) {
                        if (char === '<') {
                            e.suggestions = [(0, lint_1.fixByEscape)(startIndex, '&lt;')];
                        }
                        else if (char === 'h' && type !== 'link-text' && wordRegex.test(previousChar || '')) {
                            e.suggestions = [(0, lint_1.fixBySpace)(startIndex)];
                        }
                        else if (lbrackInExtLinkText) {
                            const i = parentNode.getAbsoluteIndex() + parentNode.toString().length;
                            e.suggestions = [(0, lint_1.fixByEscape)(i, '&#93;')];
                        }
                        else if (error === ']' && brokenExtLink) {
                            const i = start - previousSibling.toString().length;
                            e.suggestions = [(0, lint_1.fixByInsert)(i, 'left-bracket', '[')];
                        }
                        else if (magicLink) {
                            e.suggestions = [
                                ...mt[0] === error
                                    ? []
                                    : [(0, lint_1.fixByUpper)(e, error)],
                                ...nextChar === ':' || nextChar === '：'
                                    ? [(0, lint_1.fixBySpace)(endIndex, 1)]
                                    : [],
                            ];
                        }
                    }
                    errors.push(e);
                }
                return errors;
            }
        }
        /**
         * 修改内容
         * @param text 新内容
         */
        get #setData() { return _private_setData_descriptor.value; }
        /**
         * Replace the text
         *
         * 替换字符串
         * @param text new text / 替换的字符串
         */
        replaceData(text) {
            this.#setData(text);
        }
        /**
         * Split the text node into two parts
         *
         * 将文本子节点分裂为两部分
         * @param offset position to be splitted at / 分裂位置
         * @throws `RangeError` 错误的断开位置
         * @throws `Error` 没有父节点
         */
        splitText(offset) {
            LSP: {
                /* NOT FOR BROWSER */
                /* c8 ignore next 3 */
                if (offset > this.length || offset < -this.length) {
                    throw new RangeError(`Wrong offset to split: ${offset}`);
                }
                /* NOT FOR BROWSER END */
                const { parentNode, data } = this;
                /* c8 ignore next 3 */
                if (!parentNode) {
                    throw new Error('The text node to be split has no parent node!');
                }
                const newText = new AstText(data.slice(offset));
                (0, debug_1.setChildNodes)(parentNode, parentNode.childNodes.indexOf(this) + 1, 0, [newText]);
                this.setAttribute('data', data.slice(0, offset));
                return newText;
            }
        }
        /**
         * Escape `=` and `|`
         *
         * 转义 `=` 和 `|`
         * @since v1.1.4
         * @throws `Error` 没有父节点
         */
        escape() {
            LSP: {
                const { parentNode } = this;
                /* c8 ignore next 3 */
                if (!parentNode) {
                    throw new Error('The text node to be escaped has no parent node!');
                }
                const { TranscludeToken } = require('../src/transclude');
                const config = parentNode.getAttribute('config'), index = parentNode.childNodes.indexOf(this) + 1;
                /**
                 * Get the last index of `=` or `|`
                 * @param j start position from the end
                 */
                const lastIndexOf = (j) => Math.max(this.data.lastIndexOf('=', j), this.data.lastIndexOf('|', j));
                let i = lastIndexOf();
                const callback = /** @ignore */ () => 
                // @ts-expect-error abstract class
                new TranscludeToken(this.data[i] === '=' ? '=' : '!', [], config);
                for (; i >= 0; i = lastIndexOf(i - 1)) {
                    if (i < this.data.length - 1) {
                        this.splitText(i + 1);
                    }
                    parentNode.insertAt(debug_1.Shadow.run(callback), index);
                    this.#setData(this.data.slice(0, i));
                }
            }
        }
        /** @private */
        print() {
            PRINT: return (0, string_1.escape)(this.data);
        }
        /* NOT FOR BROWSER */
        /**
         * Clone the node
         *
         * 复制
         */
        cloneNode() {
            return new AstText(this.data);
        }
        /**
         * Insert text at the end
         *
         * 在后方添加字符串
         * @param text text to be inserted / 添加的字符串
         */
        appendData(text) {
            this.#setData(this.data + text);
        }
        /**
         * Delete text
         *
         * 删减字符串
         * @param offset start position / 起始位置
         * @param count number of characters to be deleted / 删减字符数
         */
        deleteData(offset, count = Infinity) {
            this.#setData(this.data.slice(0, offset)
                + (offset < 0 && offset + count >= 0 ? '' : this.data.slice(offset + count)));
        }
        /**
         * Insert text
         *
         * 插入字符串
         * @param offset position to be inserted at / 插入位置
         * @param text text to be inserted / 待插入的字符串
         */
        insertData(offset, text) {
            this.#setData(this.data.slice(0, offset) + text + this.data.slice(offset));
        }
        /**
         * Get the substring
         *
         * 提取子串
         * @param offset start position / 起始位置
         * @param count number of characters / 字符数
         */
        substringData(offset, count) {
            return this.data.substr(offset, count);
        }
        /** @private */
        getRelativeIndex(j) {
            if (j === undefined) {
                return super.getRelativeIndex();
            }
            /* c8 ignore next 3 */
            if (j < 0 || j > this.length) {
                throw new RangeError('Exceeding the text length range!');
            }
            return j;
        }
        /**
         * Generate HTML
         *
         * 生成HTML
         * @param nowrap whether to disable line-wrapping / 是否不换行
         * @since v1.10.0
         */
        toHtml(nowrap) {
            const { data } = this;
            return (0, string_1.sanitize)(nowrap ? data.replaceAll('\n', ' ') : data);
        }
        /** @private */
        removeBlankLines() {
            if (/\s$/u.test(this.data)) {
                const spaces = [], mt = /\n[^\S\n]*$/u.exec(this.data);
                let { nextSibling } = this, mt2 = null;
                while (nextSibling && (nextSibling.is('comment')
                    || nextSibling.is('category')
                    || nextSibling.type === 'text')) {
                    if (nextSibling.type === 'text') {
                        mt2 = mt && /^[^\S\n]*(?=\n)/u.exec(nextSibling.data);
                        if (mt2 || nextSibling.data.trim()) {
                            break;
                        }
                        else {
                            spaces.push(nextSibling);
                        }
                    }
                    else if (mt && nextSibling.is('category')) {
                        const trimmed = this.data.trimEnd();
                        if (this.data !== trimmed) {
                            const { length } = trimmed;
                            this.deleteData(length + this.data.slice(length).indexOf('\n'));
                        }
                        for (const space of spaces) {
                            space.#setData('');
                        }
                        spaces.length = 0;
                    }
                    ({ nextSibling } = nextSibling);
                }
                if (mt2 || nextSibling?.is('table')) {
                    if (mt) {
                        this.deleteData(mt.index + (mt2 ? 0 : 1));
                        if (mt2) {
                            nextSibling.deleteData(0, mt2[0].length);
                        }
                    }
                    else {
                        this.#setData(this.data.trimEnd());
                    }
                    for (const space of spaces) {
                        space.#setData('');
                    }
                }
            }
        }
        /** @private */
        toHtmlInternal(opt) {
            this.removeBlankLines();
            return this.toHtml(opt?.nowrap);
        }
    };
})();
exports.AstText = AstText;
constants_1.classes['AstText'] = __filename;
