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
var __importDefault = (this && this.__importDefault) || function (mod) {
    return (mod && mod.__esModule) ? mod : { "default": mod };
};
Object.defineProperty(exports, "__esModule", { value: true });
exports.AttributeToken = exports.stages = void 0;
const lint_1 = require("../util/lint");
const string_1 = require("../util/string");
const constants_1 = require("../util/constants");
const sharable_1 = require("../util/sharable");
const rect_1 = require("../lib/rect");
const index_1 = __importDefault(require("../index"));
const index_2 = require("./index");
const atom_1 = require("./atom");
/* NOT FOR BROWSER ONLY */
const document_1 = require("../lib/document");
/* NOT FOR BROWSER ONLY END */
/* NOT FOR BROWSER */
const debug_1 = require("../util/debug");
const fixed_1 = require("../mixin/fixed");
const cached_1 = require("../mixin/cached");
exports.stages = { 'ext-attr': 0, 'html-attr': 2, 'table-attr': 3 };
const ariaAttrs = new Set(['aria-describedby', 'aria-flowto', 'aria-labelledby', 'aria-owns']);
const insecureStyle = /expression|(?:accelerator|-o-link(?:-source)?|-o-replace)\s*:|(?:url|src|image(?:-set)?)\s*\(|attr\s*\([^)]+[\s,]url/u, evil = /(?:^|\s|\*\/)(?:java|vb)script(?:\W|$)/iu, complexTypes = new Set(['ext', 'arg', 'magic-word', 'template']), urlAttrs = new Set([
    'about',
    'property',
    'resource',
    'datatype',
    'typeof',
    'itemid',
    'itemprop',
    'itemref',
    'itemscope',
    'itemtype',
]);
/**
 * attribute of extension and HTML tags
 *
 * 扩展和HTML标签属性
 * @classdesc `{childNodes: [AtomToken, Token|AtomToken]}`
 */
let AttributeToken = (() => {
    let _classDecorators = [fixed_1.fixedToken];
    let _classDescriptor;
    let _classExtraInitializers = [];
    let _classThis;
    let _classSuper = index_2.Token;
    let _instanceExtraInitializers = [];
    let _toHtmlInternal_decorators;
    var AttributeToken = class extends _classSuper {
        static { _classThis = this; }
        static {
            const _metadata = typeof Symbol === "function" && Symbol.metadata ? Object.create(_classSuper[Symbol.metadata] ?? null) : void 0;
            _toHtmlInternal_decorators = [(0, cached_1.cached)()];
            __esDecorate(this, null, _toHtmlInternal_decorators, { kind: "method", name: "toHtmlInternal", static: false, private: false, access: { has: obj => "toHtmlInternal" in obj, get: obj => obj.toHtmlInternal }, metadata: _metadata }, null, _instanceExtraInitializers);
            __esDecorate(null, _classDescriptor = { value: _classThis }, _classDecorators, { kind: "class", name: _classThis.name, metadata: _metadata }, null, _classExtraInitializers);
            AttributeToken = _classThis = _classDescriptor.value;
            if (_metadata) Object.defineProperty(_classThis, Symbol.metadata, { enumerable: true, configurable: true, writable: true, value: _metadata });
            __runInitializers(_classThis, _classExtraInitializers);
        }
        #type = __runInitializers(this, _instanceExtraInitializers);
        #tag;
        #equal;
        #quotes;
        /* NOT FOR BROWSER END */
        get type() {
            return this.#type;
        }
        /** tag name / 标签名 */
        get tag() {
            return this.#tag;
        }
        /** whether the quotes are balanced / 引号是否匹配 */
        get balanced() {
            LINT: return !this.#equal || this.#quotes[0] === this.#quotes[1];
        }
        /* NOT FOR BROWSER */
        set balanced(value) {
            if (value) {
                this.close();
            }
        }
        /** attribute value / 属性值 */
        get value() {
            return this.getValue();
        }
        set value(value) {
            this.setValue(value);
        }
        /* NOT FOR BROWSER END */
        /**
         * @param type 标签类型
         * @param tag 标签名
         * @param key 属性名
         * @param quotes 引号
         * @param equal 等号
         * @param value 属性值
         */
        constructor(type, tag, key, quotes, config, equal = '', value, accum = []) {
            const keyToken = new atom_1.AtomToken(key, 'attr-key', config, accum, type === 'ext-attr' ? { AstText: ':' } : { 'Stage-2': ':', '!ExtToken': '', '!HeadingToken': '' });
            let valueToken;
            if (key === 'title' || tag === 'img' && key === 'alt') {
                valueToken = new index_2.Token(value, config, accum, {
                    [`Stage-${exports.stages[type]}`]: ':', ConverterToken: ':',
                });
                valueToken.type = 'attr-value';
                valueToken.setAttribute('stage', constants_1.MAX_STAGE - 1);
            }
            else if (tag === 'gallery' && key === 'caption'
                || tag === 'ref' && key === 'details'
                || (tag === 'mapframe' || tag === 'maplink') && key === 'text'
                || tag === 'choose' && (key === 'before' || key === 'after')) {
                const newConfig = {
                    ...config,
                    excludes: [...config.excludes, 'heading', 'html', 'table', 'hr', 'list'],
                };
                valueToken = new index_2.Token(value, newConfig, accum, {
                    'Stage-2': ':',
                    '!HeadingToken': '',
                    LinkToken: ':',
                    FileToken: ':',
                    CategoryToken: ':',
                    QuoteToken: ':',
                    MagicLinkToken: ':',
                    ConverterToken: ':',
                });
                valueToken.type = 'attr-value';
                valueToken.setAttribute('stage', 1);
            }
            else {
                valueToken = new atom_1.AtomToken(value, 'attr-value', config, accum, {
                    [`Stage-${exports.stages[type]}`]: ':',
                });
            }
            super(undefined, config, accum);
            this.#type = type;
            this.append(keyToken, valueToken);
            this.#equal = equal;
            this.#quotes = [...quotes];
            this.#tag = tag;
            this.setAttribute('name', (0, string_1.trimLc)((0, string_1.removeComment)(key)));
        }
        /** 更新name */
        #setName() {
            this.setAttribute('name', (0, string_1.trimLc)(this.firstChild.text()));
        }
        /** @private */
        afterBuild() {
            if (this.#equal.includes('\0')) {
                this.#equal = this.buildFromStr(this.#equal, constants_1.BuildMethod.String);
            }
            if (this.parentNode) {
                this.#tag = this.parentNode.name;
            }
            this.#setName();
            super.afterBuild();
            /* NOT FOR BROWSER */
            const /** @implements */ attributeListener = ({ prevTarget }) => {
                if (prevTarget === this.firstChild) {
                    this.#setName();
                }
            };
            this.addEventListener(['remove', 'insert', 'replace', 'text'], attributeListener);
        }
        /** @private */
        toString(skip) {
            const [quoteStart = '', quoteEnd = ''] = this.#quotes;
            return this.#equal ? super.toString(skip, this.#equal + quoteStart) + quoteEnd : this.firstChild.toString(skip);
        }
        /** @private */
        text() {
            return this.#equal ? `${super.text(`${this.#equal.trim()}"`)}"` : this.firstChild.text();
        }
        /** @private */
        getGaps() {
            return this.#equal ? this.#equal.length + (this.#quotes[0]?.length ?? 0) : 0;
        }
        #lint(start, rect) {
            const { firstChild, lastChild, type, name, tag, parentNode } = this, simple = !lastChild.childNodes.some(({ type: t }) => complexTypes.has(t)), value = this.getValue(), attrs = sharable_1.extAttrs[tag], attrs2 = sharable_1.htmlAttrs[tag], { length } = this.toString();
            let rule = 'illegal-attr', lintConfig, computeEditInfo;
            LINT: {
                ({ lintConfig } = index_1.default);
                ({ computeEditInfo } = lintConfig);
            }
            if (!attrs?.has(name)
                && !attrs2?.has(name)
                // 已知定义的扩展标签或不包含嵌入的HTML标签
                && (type === 'ext-attr' ? attrs || attrs2 : !/\{\{[^{]+\}\}/u.test(name))
                && (
                // 不支持通用HTML属性的扩展标签
                type === 'ext-attr' && !attrs2
                    // 或非通用HTML属性
                    || !/^(?:xmlns:[\w:.-]+|data-(?!ooui|mw|parsoid)[^:]*)$/u.test(name)
                        && (tag === 'meta' || tag === 'link' || !sharable_1.commonHtmlAttrs.has(name)))
                || (name === 'itemtype' || name === 'itemid' || name === 'itemref')
                    && !parentNode?.hasAttr('itemscope')) {
                /* PRINT ONLY */
                if (start === undefined) {
                    return true;
                }
                /* PRINT ONLY END */
                LINT: {
                    const s = lintConfig.getSeverity(rule, 'unknown');
                    if (s) {
                        const e = (0, lint_1.generateForChild)(firstChild, rect, rule, 'illegal-attribute-name', s);
                        if (computeEditInfo) {
                            e.suggestions = [(0, lint_1.fixByRemove)(start, length)];
                        }
                        return e;
                    }
                }
            }
            else if (name === 'style' && typeof value === 'string' && insecureStyle.test(value)) {
                /* PRINT ONLY */
                if (start === undefined) {
                    return true;
                }
                /* PRINT ONLY END */
                LINT: {
                    rule = 'insecure-style';
                    const s = lintConfig.getSeverity(rule);
                    return s && (0, lint_1.generateForChild)(lastChild, rect, rule, 'insecure-style', s);
                }
            }
            else if (name === 'tabindex' && typeof value === 'string' && value !== '0') {
                /* PRINT ONLY */
                if (start === undefined) {
                    return true;
                }
                /* PRINT ONLY END */
                LINT: {
                    const s = lintConfig.getSeverity(rule, 'tabindex');
                    if (s) {
                        const e = (0, lint_1.generateForChild)(lastChild, rect, rule, 'nonzero-tabindex', s);
                        if (computeEditInfo) {
                            e.suggestions = [
                                (0, lint_1.fixByRemove)(start, length),
                                (0, lint_1.fixBy)(e, '0 tabindex', '0'),
                            ];
                        }
                        return e;
                    }
                }
            }
            else if (typeof value === 'string' && ((/^xmlns:[\w:.-]+$/u.test(name) || urlAttrs.has(name)) && evil.test(value)
                || simple
                    && (name === 'href' || tag === 'img' && name === 'src')
                    && !new RegExp(String.raw `^(?:${this.getAttribute('config').protocol}|//)\S+$`, 'iu')
                        .test(value))) {
                /* PRINT ONLY */
                if (start === undefined) {
                    return true;
                }
                /* PRINT ONLY END */
                LINT: {
                    const s = lintConfig.getSeverity(rule, 'value');
                    return s && (0, lint_1.generateForChild)(lastChild, rect, rule, 'illegal-attribute-value', s);
                }
            }
            else if (simple && type !== 'ext-attr') {
                const data = (0, lint_1.provideValues)(tag, name), v = String(value).toLowerCase();
                if (data.length > 0 && data.every(n => n !== v)) {
                    /* PRINT ONLY */
                    if (start === undefined) {
                        return true;
                    }
                    /* PRINT ONLY END */
                    LINT: {
                        const s = lintConfig.getSeverity(rule, 'value');
                        return s && (0, lint_1.generateForChild)(lastChild, rect, rule, 'illegal-attribute-value', s);
                    }
                }
            }
            return false;
        }
        /** @private */
        lint(start = this.getAbsoluteIndex(), re) {
            LINT: {
                const errors = super.lint(start, re), { balanced, firstChild, lastChild, name, tag } = this, rect = new rect_1.BoundingRect(this, start), rules = ['unclosed-quote', 'obsolete-attr'], { lintConfig } = index_1.default, s = rules.map(rule => lintConfig.getSeverity(rule, name));
                if (s[0] && !balanced) {
                    const e = (0, lint_1.generateForChild)(lastChild, rect, rules[0], 'unclosed-quotes', s[0]);
                    e.startIndex--;
                    e.startCol--;
                    if (lintConfig.computeEditInfo) {
                        e.suggestions = [(0, lint_1.fixByClose)(e.endIndex, this.#quotes[0])];
                    }
                    errors.push(e);
                }
                const e = this.#lint(start, rect);
                if (e) {
                    errors.push(e);
                }
                if (s[1] && sharable_1.obsoleteAttrs[tag]?.has(name)) {
                    errors.push((0, lint_1.generateForChild)(firstChild, rect, rules[1], 'obsolete-attribute', s[1]));
                }
                /* NOT FOR BROWSER ONLY */
                const rule = 'invalid-css', sError = lintConfig.getSeverity(rule), sWarn = lintConfig.getSeverity(rule, 'warn');
                if ((sError || sWarn)
                    && name === 'style'
                    && lastChild.length === 1 && lastChild.firstChild.type === 'text') {
                    const cssLSP = (0, document_1.loadCssLSP)();
                    if (cssLSP) {
                        const root = this.getRootNode(), textDoc = new document_1.EmbeddedCSSDocument(root, lastChild);
                        Array.prototype.push.apply(errors, cssLSP.doValidation(textDoc, textDoc.styleSheet)
                            .filter(({ code, severity }) => code !== 'css-ruleorselectorexpected' && code !== 'emptyRules'
                            && (severity === 1 ? sError : sWarn))
                            .map(({ range: { start: { line, character }, end }, message, severity, code }) => ({
                            code: code,
                            rule,
                            message,
                            severity: (severity === 1 ? sError : sWarn),
                            startLine: line,
                            startCol: character,
                            startIndex: root.indexFromPos(line, character),
                            endLine: end.line,
                            endCol: end.character,
                            endIndex: root.indexFromPos(end.line, end.character),
                        })));
                    }
                }
                /* NOT FOR BROWSER ONLY END */
                return errors;
            }
        }
        /**
         * Get the attribute value
         *
         * 获取属性值
         */
        getValue() {
            return this.#equal ? this.lastChild.text().trim() : this.type === 'ext-attr' || '';
        }
        /** @private */
        escape() {
            LSP: if (this.type !== 'ext-attr') {
                this.#equal = '{{=}}';
                this.lastChild.escape();
            }
        }
        /* PRINT ONLY */
        /** @private */
        getAttribute(key) {
            /* NOT FOR BROWSER */
            if (key === 'quotes') {
                return this.#quotes;
            }
            /* NOT FOR BROWSER END */
            return key === 'invalid' ? this.#lint() : super.getAttribute(key);
        }
        /** @private */
        print() {
            PRINT: {
                const [quoteStart = '', quoteEnd = ''] = this.#quotes;
                return this.#equal ? super.print({ sep: (0, string_1.escape)(this.#equal) + quoteStart, post: quoteEnd }) : super.print();
            }
        }
        /** @private */
        json(_, depth, start = this.getAbsoluteIndex()) {
            LSP: {
                const json = super.json(undefined, depth, start);
                json['tag'] = this.tag;
                return json;
            }
        }
        /* PRINT ONLY END */
        /* NOT FOR BROWSER */
        /** @private */
        setAttribute(key, value) {
            if (key === 'equal') {
                this.#equal = value && (this.#equal || value);
            }
            else if (key === 'quotes') {
                this.#quotes = value;
            }
            else {
                super.setAttribute(key, value);
            }
        }
        cloneNode() {
            const [key, value] = this.cloneChildNodes(), k = key.toString();
            return debug_1.Shadow.run(() => {
                // @ts-expect-error abstract class
                const token = new AttributeToken(this.type, this.tag, k, this.#quotes, this.getAttribute('config'), this.#equal, undefined);
                token.firstChild.safeReplaceWith(key);
                token.lastChild.safeReplaceWith(value);
                return token;
            });
        }
        /**
         * Close the quote
         *
         * 闭合引号
         */
        close() {
            const [opening] = this.#quotes;
            if (opening) {
                this.#quotes[1] = opening;
            }
        }
        /**
         * Set the attribute value
         *
         * 设置属性值
         * @param value attribute value / 属性值
         * @throws `RangeError` 扩展标签属性不能包含 ">"
         * @throws `RangeError` 同时包含单引号和双引号
         */
        setValue(value) {
            require('../addon/attribute');
            this.setValue(value);
        }
        /* c8 ignore start */
        /**
         * Rename the attribute
         *
         * 修改属性名
         * @param key new attribute name / 新属性名
         * @throws `Error` title和alt属性不能更名
         */
        rename(key) {
            require('../addon/attribute');
            this.rename(key);
        }
        /* c8 ignore stop */
        /** @private */
        asHtmlAttr() {
            this.#type = 'html-attr';
        }
        /** @private */
        toHtmlInternal() {
            const { type, name, tag, lastChild } = this;
            if (type === 'ext-attr' && sharable_1.extAttrs[tag]?.has(name) || this.#lint()) {
                return '';
            }
            const value = lastChild.toHtmlInternal().trim();
            if (name === 'id' && !value) {
                return '';
            }
            const sanitized = ariaAttrs.has(name)
                ? value.split(/\s+/u).filter(v => v !== '').map(v => (0, string_1.sanitizeAttr)(v, true))
                    .join(' ')
                : (0, string_1.sanitizeAttr)(value, name === 'id');
            return `${name}="${sanitized}"`;
        }
        /* c8 ignore start */
        /**
         * Get or set the value of a style property
         *
         * 获取或设置某一样式属性的值
         * @param key style property / 样式属性
         * @param value style property value / 样式属性值
         * @since v1.17.1
         * @throws `Error` 不是style属性
         * @throws `Error` 复杂的style属性
         * @throws `Error` 无CSS语言服务
         */
        css(key, value) {
            require('../addon/attribute');
            return this.css(key, value);
        }
    };
    return AttributeToken = _classThis;
})();
exports.AttributeToken = AttributeToken;
constants_1.classes['AttributeToken'] = __filename;
