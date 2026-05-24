"use strict";
// PHP解析器的步骤：
// -2. 替换签名和`{{subst:}}`，参见Parser::preSaveTransform；这在revision中不可能保留，可以跳过
// -1. 移除特定字符`\0`和`\x7F`，参见Parser::parse
// 0. 重定向，参见WikitextContentHandler::extractRedirectTargetAndText
// 1. 注释/扩展标签（'<'相关），参见Preprocessor_Hash::buildDomTreeArrayFromText和Sanitizer::decodeTagAttributes
// 2. 模板/模板变量/标题，注意rightmost法则，以及`-{`和`[[`可以破坏`{{`或`{{{`语法，
//    参见Preprocessor_Hash::buildDomTreeArrayFromText
// 3. HTML标签（允许不匹配），参见Sanitizer::internalRemoveHtmlTags
// 4. 表格，参见Parser::handleTables
// 5. 水平线、状态开关和余下的标题，参见Parser::internalParse
// 6. 内链，含文件和分类，参见Parser::handleInternalLinks2
// 7. `'`，参见Parser::doQuotes
// 8. 外链，参见Parser::handleExternalLinks
// 9. ISBN、RFC和自由外链，参见Parser::handleMagicLinks
// 10. 段落和列表，参见BlockLevelPass::execute
// 11. 转换，参见LanguageConverter::recursiveConvertTopLevel
// \0\d+.\x7F标记Token：
// !: `{{!}}`专用
// {: `{{(!}}`专用
// }: `{{!)}}`专用
// -: `{{!-}}`专用
// +: `{{!!}}`专用
// ~: `{{=}}`专用
// a: AttributeToken
// b: TableToken
// c: CommentToke
// d: ListToken
// e: ExtToken
// f: ImageParameterToken内的MagicLinkToken
// g: TranslateToken或OnlyincludeToken
// h: HeadingToken
// i: RFC/PMID/ISBN
// l: LinkToken
// m: `{{server}}`、`{{fullurl:}}`、`{{canonicalurl:}}`或`{{filepath:}}`
// n: NoIncludeToken、IncludeToken、TvarToken、DoubleUnderscoreToken或`{{#vardefine:}}`
// o: RedirectToken
// q: QuoteToken
// r: HrToken
// s: `{{{|subst:}}}`
// t: ArgToken或TranscludeToken
// u: `__toc__`
// v: ConverterToken
// w: ExtLinkToken或free-ext-link
// x: HtmlToken
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
exports.Token = void 0;
const string_1 = require("../util/string");
const constants_1 = require("../util/constants");
const lint_1 = require("../util/lint");
const debug_1 = require("../util/debug");
const index_1 = __importDefault(require("../index"));
const element_1 = require("../lib/element");
const text_1 = require("../lib/text");
const fs_1 = __importDefault(require("fs"));
const path_1 = __importDefault(require("path"));
/* NOT FOR BROWSER */
const strict_1 = __importDefault(require("assert/strict"));
const html_1 = require("../util/html");
const ranges_1 = require("../lib/ranges");
const readOnly_1 = require("../mixin/readOnly");
const cached_1 = require("../mixin/cached");
const lintSelectors = ['category', 'html-attr#id,ext-attr#id,table-attr#id'];
/* NOT FOR BROWSER */
/**
 * 可接受的Token类型
 * @param value 可接受的Token类型
 */
const getAcceptable = (value) => {
    const acceptable = {};
    for (const k in value) {
        if (k.startsWith('Stage-')) {
            for (let i = 0; i <= Number(k.slice(6)); i++) {
                for (const type of constants_1.aliases[i]) {
                    acceptable[type] = new ranges_1.Ranges(value[k]);
                }
            }
        }
        else if (k.startsWith('!')) { // `!`项必须放在最后
            delete acceptable[k.slice(1)];
        }
        else {
            acceptable[k] = new ranges_1.Ranges(value[k]);
        }
    }
    return acceptable;
};
/* NOT FOR BROWSER END */
/**
 * base class for all tokens
 *
 * 所有节点的基类
 * @classdesc `{childNodes: (AstText|Token)[]}`
 */
let Token = (() => {
    let _classSuper = element_1.AstElement;
    let _instanceExtraInitializers = [];
    let _lint_decorators;
    let _safeReplaceWith_decorators;
    let _toHtmlInternal_decorators;
    return class Token extends _classSuper {
        static {
            const _metadata = typeof Symbol === "function" && Symbol.metadata ? Object.create(_classSuper[Symbol.metadata] ?? null) : void 0;
            _lint_decorators = [(0, readOnly_1.readOnly)(true)];
            _safeReplaceWith_decorators = [(0, readOnly_1.readOnly)()];
            _toHtmlInternal_decorators = [(0, cached_1.cached)()];
            __esDecorate(this, null, _lint_decorators, { kind: "method", name: "lint", static: false, private: false, access: { has: obj => "lint" in obj, get: obj => obj.lint }, metadata: _metadata }, null, _instanceExtraInitializers);
            __esDecorate(this, null, _safeReplaceWith_decorators, { kind: "method", name: "safeReplaceWith", static: false, private: false, access: { has: obj => "safeReplaceWith" in obj, get: obj => obj.safeReplaceWith }, metadata: _metadata }, null, _instanceExtraInitializers);
            __esDecorate(this, null, _toHtmlInternal_decorators, { kind: "method", name: "toHtmlInternal", static: false, private: false, access: { has: obj => "toHtmlInternal" in obj, get: obj => obj.toHtmlInternal }, metadata: _metadata }, null, _instanceExtraInitializers);
            if (_metadata) Object.defineProperty(this, Symbol.metadata, { enumerable: true, configurable: true, writable: true, value: _metadata });
        }
        #type = (__runInitializers(this, _instanceExtraInitializers), 'plain');
        /** 解析阶段，参见顶部注释。只对plain Token有意义。 */
        #stage = 0;
        #config;
        /** 这个数组起两个作用：1. 数组中的Token会在build时替换`/\0\d+.\x7F/`标记；2. 数组中的Token会依次执行parseOnce和build方法。 */
        #accum;
        #include;
        #built = false;
        #string;
        #pageName;
        /* NOT FOR BROWSER */
        #acceptable;
        #protectedChildren = new ranges_1.Ranges();
        /* NOT FOR BROWSER END */
        get type() {
            return this.#type;
        }
        set type(value) {
            /* NOT FOR BROWSER */
            const plainTypes = [
                'root',
                'plain',
                'translate-inner',
                'attr-value',
                'ext-inner',
                'arg-default',
                'parameter-key',
                'parameter-value',
                'heading-title',
                'table-inter',
                'td-inner',
                'link-text',
                'ext-link-text',
                'list-range',
                'converter-rule-to',
                'converter-rule-from',
            ];
            /* c8 ignore next 3 */
            if (!plainTypes.includes(value)) {
                throw new RangeError(`"${value}" is not a valid type for ${this.constructor.name}!`);
            }
            /* NOT FOR BROWSER END */
            this.#type = value;
        }
        /**
         * page name
         *
         * 页面名称
         * @since v1.29.0
         */
        get pageName() {
            return this.getRootNode().#pageName;
        }
        set pageName(value) {
            if (value) {
                const title = this.normalizeTitle(value, 0, { temporary: true, page: '' });
                this.#pageName = title.valid ? title.title : undefined;
            }
            else {
                this.#pageName = value === '' ? '' : undefined;
            }
        }
        /** @class */
        constructor(wikitext, config = index_1.default.getConfig(), accum = [], acceptable) {
            super();
            if (typeof wikitext === 'string') {
                this.insertAt(wikitext);
            }
            this.#config = config;
            this.#accum = accum;
            accum.push(this);
            /* NOT FOR BROWSER */
            this.setAttribute('acceptable', acceptable);
        }
        /** @private */
        parseOnce(n = this.#stage, include = false, tidy) {
            if (n < this.#stage || this.length !== 1 || !this.isPlain()) {
                return this;
            }
            /* c8 ignore start */
            if (this.#stage >= constants_1.MAX_STAGE) {
                /* NOt FOR BROWSER */
                if (this.type === 'root') {
                    index_1.default.error('Fully parsed!');
                }
                /* NOT FOR BROWSER END */
                return this;
            }
            /* c8 ignore stop */
            // Optional stage-level debugging: set env WIKI_STAGE_LOG to enable.
            const _escapeStageStr = (s) => {
                if (s == null) return '';
                return String(s).replace(/\0/g, '\\0').replace(/\x7F/g, '\\x7F').replace(/\n/g, '\\n');
            };
            const _tokenToJson = (node) => {
                if (node.type === 'text') {
                    const text = String(node.data || '');
                    const nodes = [];
                    let pos = 0;
                    while (pos < text.length) {
                        if (text[pos] === '\0') {
                            const numStart = pos + 1;
                            let numEnd = numStart;
                            while (numEnd < text.length && text[numEnd] >= '0' && text[numEnd] <= '9') numEnd++;
                            if (numEnd === numStart) {
                                nodes.push({ type: 'text', data: '\0' });
                                pos = numEnd;
                                continue;
                            }
                            const idx = Number(text.slice(numStart, numEnd));
                            pos = numEnd;
                            if (pos < text.length) pos++;
                            if (pos < text.length && text[pos] === '\x7F') pos++;
                            const token = this.#accum[idx];
                            if (token) {
                                const childJson = _tokenToJson(token);
                                if (Array.isArray(childJson)) {
                                    nodes.push(...childJson);
                                }
                                else {
                                    nodes.push(childJson);
                                }
                            }
                            else {
                                nodes.push({ type: 'null' });
                            }
                        }
                        else {
                            const start = pos;
                            while (pos < text.length && text[pos] !== '\0') pos++;
                            nodes.push({ type: 'text', data: text.slice(start, pos) });
                        }
                    }
                    return nodes.length === 1 ? nodes[0] : nodes;
                }
                const output = { type: String(node.type) };
                if (node.name != null) {
                    output.name = String(node.name);
                }
                if (node.childNodes && node.childNodes.length) {
                    const childNodes = [];
                    for (const child of node.childNodes) {
                        const childJson = _tokenToJson(child);
                        if (Array.isArray(childJson)) {
                            childNodes.push(...childJson);
                        }
                        else {
                            childNodes.push(childJson);
                        }
                    }
                    if (childNodes.length) output.childNodes = childNodes;
                }
                return output;
            };
            const _stageJson = () => {
                const rootNode = typeof this.getRootNode === 'function' ? this.getRootNode() : this;
                return _tokenToJson(rootNode);
            };

            switch (n) {
                case 0:
                    if (this.type === 'root') {
                        this.#accum.pop();
                        const isRedirect = this.#parseRedirect();
                        include &&= !isRedirect;
                    }
                    this.#include = include;
                    this.#parseCommentAndExt(include);
                    break;
                case 1:
                    this.#parseBraces();
                    break;
                case 2:
                    this.#parseHtml();
                    break;
                case 3:
                    this.#parseTable();
                    break;
                case 4:
                    this.#parseHrAndDoubleUnderscore();
                    break;
                case 5:
                    this.#parseLinks(tidy);
                    break;
                case 6:
                    this.#parseQuotes(tidy);
                    break;
                case 7:
                    this.#parseExternalLinks();
                    break;
                case 8:
                    this.#parseMagicLinks();
                    break;
                case 9:
                    this.#parseList();
                    break;
                case 10:
                    this.#parseConverter();
                // no default
            }
            if (this.type === 'root') {
                for (const token of this.#accum) {
                    token?.parseOnce(n, include, tidy); // eslint-disable-line @typescript-eslint/no-unnecessary-condition
                }
                // When a per-run directory is set via WIKI_STAGE_LOG_DIR, append
                // a JSON snapshot of the token tree to js-stage.log.
                try {
                    const _stageLogDir = typeof process !== 'undefined' && process && process.env && process.env.WIKI_STAGE_LOG_DIR
                        ? process.env.WIKI_STAGE_LOG_DIR
                        : undefined;
                    if (_stageLogDir) {
                        const jsonObj = _stageJson();
                        if (jsonObj) {
                            const filePath = path_1.default.join(_stageLogDir, 'js-stage.log');
                            fs_1.default.appendFileSync(filePath, `Stage ${n}: ${JSON.stringify(jsonObj)}\n`);
                        }
                    }
                } catch (e) {
                    /* swallow debug errors */
                }
            }

            this.#stage++;
            return this;
        }
        /** @private */
        buildFromStr(str, type) {
            const nodes = str.split(/[\0\x7F]/u).map((s, i) => {
                if (i % 2 === 0) {
                    return s && new text_1.AstText(s);
                }
                const n = Number(s.slice(0, -1));
                if (isNaN(s.slice(-1))
                    && Number.isInteger(n) && n >= 0 && n < this.#accum.length) {
                    return this.#accum[n];
                }
                /* c8 ignore next */
                throw new Error(`Failed to build! Unrecognized token: ${s}`);
            }).filter(node => node !== '');
            if (type === constants_1.BuildMethod.String) {
                return nodes.map(String).join('');
            }
            else if (type === constants_1.BuildMethod.Text) {
                return (0, string_1.text)(nodes);
            }
            return nodes;
        }
        /** @private */
        build() {
            this.#stage = constants_1.MAX_STAGE;
            const { length, firstChild } = this, str = firstChild?.toString();
            if (length === 1 && firstChild.type === 'text' && str.includes('\0')) {
                (0, debug_1.setChildNodes)(this, 0, 1, this.buildFromStr(str));
                if (this.type === 'root') {
                    for (const token of this.#accum) {
                        token?.build(); // eslint-disable-line @typescript-eslint/no-unnecessary-condition
                    }
                }
            }
        }
        /** @private */
        afterBuild() {
            if (this.type === 'root') {
                for (const token of this.#accum) {
                    token?.afterBuild(); // eslint-disable-line @typescript-eslint/no-unnecessary-condition
                }
            }
            this.#built = true;
        }
        /** @private */
        parse(n = constants_1.MAX_STAGE, include, tidy) {
            n = Math.min(n, constants_1.MAX_STAGE);
            while (this.#stage < n) {
                this.parseOnce(this.#stage, include, tidy);
            }
            if (n) {
                this.build();
                this.afterBuild();
            }
            return this;
        }
        /** 解析重定向 */
        #parseRedirect() {
            const { parseRedirect } = require('../parser/redirect');
            const wikitext = this.firstChild.toString(), parsed = parseRedirect(wikitext, this.#config, this.#accum);
            if (parsed !== false) {
                this.setText(parsed);
            }
            return Boolean(parsed);
        }
        /**
         * 解析HTML注释和扩展标签
         * @param includeOnly 是否嵌入
         */
        #parseCommentAndExt(includeOnly) {
            const { parseCommentAndExt } = require('../parser/commentAndExt');
            this.setText(parseCommentAndExt(this.firstChild.toString(), this.#config, this.#accum, includeOnly));
        }
        /** 解析花括号 */
        #parseBraces() {
            const { parseBraces } = require('../parser/braces');
            const str = this.type === 'root' ? this.firstChild.toString() : `\0${this.firstChild.toString()}`, parsed = parseBraces(str, this.#config, this.#accum);
            this.setText(this.type === 'root' ? parsed : parsed.slice(1));
        }
        /** 解析HTML标签 */
        #parseHtml() {
            if (this.#config.excludes.includes('html')) {
                return;
            }
            const { parseHtml } = require('../parser/html');
            this.setText(parseHtml(this.firstChild.toString(), this.#config, this.#accum));
        }
        /** 解析表格 */
        #parseTable() {
            if (this.#config.excludes.includes('table')) {
                return;
            }
            const { parseTable } = require('../parser/table');
            this.setText(parseTable(this, this.#config, this.#accum));
        }
        /** 解析`<hr>`和状态开关 */
        #parseHrAndDoubleUnderscore() {
            if (this.#config.excludes.includes('hr')) {
                return;
            }
            const { parseHrAndDoubleUnderscore } = require('../parser/hrAndDoubleUnderscore');
            this.setText(parseHrAndDoubleUnderscore(this, this.#config, this.#accum));
        }
        /**
         * 解析内部链接
         * @param tidy 是否整理
         */
        #parseLinks(tidy) {
            const { parseLinks } = require('../parser/links');
            this.setText(parseLinks(this.firstChild.toString(), this.#config, this.#accum, this.pageName, tidy));
        }
        /**
         * 解析单引号
         * @param tidy 是否整理
         */
        #parseQuotes(tidy) {
            const { parseQuotes } = require('../parser/quotes');
            const lines = this.firstChild.toString().split('\n');
            for (let i = 0; i < lines.length; i++) {
                lines[i] = parseQuotes(lines[i], this.#config, this.#accum, tidy);
            }
            this.setText(lines.join('\n'));
        }
        /** 解析外部链接 */
        #parseExternalLinks() {
            const { parseExternalLinks } = require('../parser/externalLinks');
            this.setText(parseExternalLinks(this.firstChild.toString(), this.#config, this.#accum));
        }
        /** 解析自由外链 */
        #parseMagicLinks() {
            const { parseMagicLinks } = require('../parser/magicLinks');
            this.setText(parseMagicLinks(this.firstChild.toString(), this.#config, this.#accum));
        }
        /** 解析列表 */
        #parseList() {
            if (this.#config.excludes.includes('list')) {
                return;
            }
            const { parseList } = require('../parser/list');
            const { firstChild, type, name } = this, lines = firstChild.toString().split('\n'), state = { lastPrefix: '' };
            let i = type === 'root' || type === 'ext-inner' && name === 'poem' ? 0 : 1;
            for (; i < lines.length; i++) {
                lines[i] = parseList(lines[i], state, this.#config, this.#accum);
            }
            this.setText(lines.join('\n'));
        }
        /** 解析语言变体转换 */
        #parseConverter() {
            if (this.#config.variants.length > 0) {
                const { parseConverter } = require('../parser/converter');
                this.setText(parseConverter(this.firstChild.toString(), this.#config, this.#accum));
            }
        }
        /** @private */
        isPlain() {
            return this.constructor === Token;
        }
        /** @private */
        getAttribute(key) {
            switch (key) {
                case 'config':
                    return this.#config;
                case 'include':
                    return (this.#include ?? Boolean(this.getRootNode().#include));
                case 'accum':
                    return this.#accum;
                case 'built':
                    return this.#built;
                /* c8 ignore next 2 */
                case 'stage':
                    return this.#stage;
                /* PRINT ONLY */
                case 'invalid':
                    PRINT: return (this.type === 'table-inter' && (0, lint_1.isFostered)(this) === 2);
                /* PRINT ONLY END */
                /* NOT FOR BROWSER */
                case 'protectedChildren':
                    return this.#protectedChildren;
                /* NOT FOR BROWSER END */
                default:
                    return super.getAttribute(key);
            }
        }
        /** @private */
        setAttribute(key, value) {
            switch (key) {
                case 'stage':
                    if (this.#stage === 0 && this.type === 'root') {
                        this.#accum.shift();
                    }
                    this.#stage = value;
                    break;
                /* NOT FOR BROWSER */
                case 'acceptable':
                    this.#acceptable = value
                        && (() => getAcceptable(value));
                    break;
                case 'include':
                    this.#include = value;
                    break;
                /* NOT FOR BROWSER END */
                default:
                    super.setAttribute(key, value);
            }
        }
        insertAt(child, i = this.length) {
            const token = typeof child === 'string' ? new text_1.AstText(child) : child, { length } = this;
            i += i < 0 ? length : 0;
            /* NOT FOR BROWSER */
            const acceptable = this.getAcceptable();
            if (!debug_1.Shadow.running && acceptable) {
                const nodesAfter = this.childNodes.slice(i), insertedName = token.constructor.name;
                if (!acceptable[insertedName]?.has(i, length + 1)) {
                    this.constructorError(`cannot insert a ${insertedName} at position ${i}`);
                }
                else if (nodesAfter.some(({ constructor: { name } }, j) => !acceptable[name]?.has(i + j + 1, length + 1))) {
                    this.constructorError(`violates the order of acceptable nodes by inserting a child node at position ${i}`);
                }
            }
            /* NOT FOR BROWSER END */
            super.insertAt(token, i);
            const { type, 
            /* NOT FOR BROWSER */
            constructor, } = token;
            /* NOT FOR BROWSER */
            const e = new Event('insert', { bubbles: true });
            this.dispatchEvent(e, { type: 'insert', position: i });
            if (type !== 'list-range' && constructor === Token && this.isPlain()) {
                index_1.default.warn('You are inserting a plain token as a child of another plain token. '
                    + 'Consider calling Token.flatten method afterwards.');
            }
            /* NOT FOR BROWSER END */
            if (type === 'root') {
                token.type = 'plain';
            }
            return token;
        }
        // eslint-disable-next-line jsdoc/require-param
        /**
         * Normalize page title
         *
         * 规范化页面标题
         * @param title title (with or without the namespace prefix) / 标题（含或不含命名空间前缀）
         * @param defaultNs default namespace number / 命名空间
         */
        normalizeTitle(title, defaultNs = 0, opt) {
            return index_1.default.normalizeTitle(title, defaultNs, this.getAttribute('include'), this.#config, { page: this.pageName, ...opt });
        }
        /** @private */
        inTableAttrs() {
            return this.isInside('table-attrs') && (this.closest('table-attrs,arg,parameter')?.is('table-attrs')
                ? 2
                : 1);
        }
        /** @private */
        inHtmlAttrs() {
            return this.isInside('html-attrs') ? 2 : this.inTableAttrs();
        }
        /** @private */
        lint(start = this.getAbsoluteIndex(), re) {
            LINT: {
                const { lintConfig } = index_1.default, { computeEditInfo, fix: needFix, ignoreDisables, configurationComment } = lintConfig;
                let errors = super.lint(start, re);
                if (this.type === 'root') {
                    const record = new Map(), r = 'no-duplicate', s = ['category', 'id'].map(key => lintConfig.getSeverity(r, key)), wikitext = this.toString(), selector = lintSelectors.filter((_, i) => s[i]).join();
                    if (selector) {
                        for (const cat of this.querySelectorAll(selector)) {
                            let key;
                            if (cat.is('category')) {
                                key = cat.name;
                            }
                            else {
                                const value = cat.getValue();
                                if (value && value !== true) {
                                    key = `#${value}`;
                                }
                            }
                            if (key) {
                                const thisCat = record.get(key);
                                if (thisCat) {
                                    thisCat.add(cat);
                                }
                                else {
                                    record.set(key, new Set([cat]));
                                }
                            }
                        }
                        for (const [key, value] of record) {
                            if (value.size > 1 && !key.startsWith('#mw-customcollapsible-')) {
                                const isCat = !key.startsWith('#'), msg = `duplicate-${isCat ? 'category' : 'id'}`, severity = s[isCat ? 0 : 1];
                                errors.push(...[...value].map(cat => {
                                    const e = (0, lint_1.generateForSelf)(cat, { start: cat.getAbsoluteIndex() }, r, msg, severity);
                                    if (computeEditInfo && isCat) {
                                        e.suggestions = [(0, lint_1.fixByRemove)(e)];
                                    }
                                    return e;
                                }));
                            }
                        }
                    }
                    if (!ignoreDisables) {
                        const regex = new RegExp(String.raw `<!--\s*${configurationComment}-(disable(?:(?:-next)?-line)?|enable)(\s[\sa-z,-]*)?-->`, 'gu'), ignores = [];
                        let mt = regex.exec(wikitext);
                        while (mt) {
                            const { 1: type, index } = mt, detail = mt[2]?.trim();
                            let line;
                            if (type === 'disable-line' || type === 'disable-next-line') {
                                line = this.posFromIndex(index).top + (type === 'disable-line' ? 0 : 1);
                            }
                            ignores.push({
                                line,
                                from: type === 'disable' ? regex.lastIndex : undefined,
                                to: type === 'enable' ? regex.lastIndex : undefined,
                                rules: detail ? new Set(detail.split(',').map(rule => rule.trim())) : undefined,
                            });
                            mt = regex.exec(wikitext);
                        }
                        errors = errors.filter(({ rule, startLine, startIndex }) => {
                            const nearest = { pos: 0 };
                            for (const { line, from, to, rules } of ignores) {
                                if (line && line > startLine + 1) {
                                    break;
                                }
                                else if (rules && !rules.has(rule)) {
                                    continue;
                                }
                                else if (line === startLine) {
                                    return false;
                                }
                                else if (from <= startIndex && from > nearest.pos) {
                                    nearest.pos = from;
                                    nearest.type = 'from';
                                }
                                else if (to <= startIndex && to > nearest.pos) {
                                    nearest.pos = to;
                                    nearest.type = 'to';
                                }
                            }
                            return nearest.type !== 'from';
                        });
                    }
                    if (needFix && errors.some(({ fix }) => fix)) {
                        // 倒序修复，跳过嵌套的修复
                        const fixable = errors.map(({ fix }) => fix).filter(fix => fix !== undefined).sort(({ range: [aFrom, aTo] }, { range: [bFrom, bTo] }) => aTo === bTo ? bFrom - aFrom : bTo - aTo);
                        let i = Infinity, output = wikitext;
                        for (const { range: [from, to], text: t } of fixable) {
                            if (to <= i) {
                                output = output.slice(0, from) + t + output.slice(to);
                                i = from;
                            }
                        }
                        Object.assign(errors, { output });
                    }
                    if (!computeEditInfo) {
                        for (const e of errors) {
                            delete e.fix;
                            delete e.suggestions;
                        }
                    }
                }
                return errors;
            }
        }
        /** @private */
        toString(skip, separator) {
            return skip
                ? super.toString(true, separator)
                : (0, lint_1.cache)(this.#string, () => super.toString(false, separator), value => {
                    const root = this.getRootNode();
                    if (root.type === 'root' && root.#built) {
                        this.#string = value;
                    }
                });
        }
        /* NOT FOR BROWSER */
        /** @private */
        print(opt) {
            return this.is('list-range') ? (0, string_1.print)(this.childNodes) : super.print(opt);
        }
        /** @private */
        getAcceptable() {
            if (typeof this.#acceptable === 'function') {
                this.#acceptable = this.#acceptable();
            }
            return this.#acceptable;
        }
        /** @private */
        dispatchEvent(e, data) {
            if (this.#built) {
                super.dispatchEvent(e, data);
            }
        }
        /** @private */
        protectChildren(args) {
            this.#protectedChildren.push(...new ranges_1.Ranges(args));
        }
        /** @private */
        concat(elements) {
            if (elements.length === 0) {
                return;
            }
            const { childNodes, lastChild } = this, first = elements[0], last = elements.at(-1);
            for (const element of elements) {
                element.setAttribute('parentNode', this);
            }
            lastChild?.setAttribute('nextSibling', first);
            first.setAttribute('previousSibling', lastChild);
            last.setAttribute('nextSibling', undefined);
            this.setAttribute('childNodes', [...childNodes, ...elements]);
        }
        /** @private */
        removeAt(i) {
            const { length, childNodes } = this;
            i += i < 0 ? length : 0;
            if (!debug_1.Shadow.running) {
                if (this.#protectedChildren.has(i, length)) {
                    this.constructorError(`cannot remove the child node at position ${i}`);
                }
                const acceptable = this.getAcceptable();
                if (acceptable) {
                    const nodesAfter = childNodes.slice(i + 1);
                    if (nodesAfter.some(({ constructor: { name } }, j) => !acceptable[name]?.has(i + j, length - 1))) {
                        this.constructorError(`violates the order of acceptable nodes by removing the child node at position ${i}`);
                    }
                }
            }
            const node = super.removeAt(i);
            const e = new Event('remove', { bubbles: true });
            this.dispatchEvent(e, { type: 'remove', position: i, removed: node });
            return node;
        }
        /**
         * Replace with a token of the same type
         *
         * 替换为同类节点
         * @param token token to be replaced with / 待替换的节点
         * @throws `Error` 不存在父节点
         */
        safeReplaceWith(token) {
            const { parentNode } = this;
            /* c8 ignore start */
            if (!parentNode) {
                throw new Error('The node does not have a parent node!');
            }
            else if (token.constructor !== this.constructor) {
                this.typeError('safeReplaceWith', this.constructor.name);
            }
            try {
                strict_1.default.deepEqual(token.getAcceptable(), this.getAcceptable());
            }
            catch (e) {
                if (e instanceof strict_1.default.AssertionError) {
                    this.constructorError('has a different #acceptable property');
                }
                throw e;
            }
            /* c8 ignore stop */
            const i = parentNode.childNodes.indexOf(this);
            super.removeAt.call(parentNode, i);
            super.insertAt.call(parentNode, token, i);
            if (token.type === 'root') {
                token.type = 'plain';
            }
            const e = new Event('replace', { bubbles: true });
            token.dispatchEvent(e, { type: 'replace', position: i, oldToken: this });
        }
        /* c8 ignore start */
        /**
         * Create an HTML comment
         *
         * 创建HTML注释
         * @param data comment content / 注释内容
         */
        createComment(data) {
            require('../addon/token');
            return this.createComment(data);
        }
        /* c8 ignore stop */
        /**
         * Create a tag
         *
         * 创建标签
         * @param tagName tag name / 标签名
         * @param options options / 选项
         * @param options.selfClosing whether to be a self-closing tag / 是否自封闭
         * @param options.closing whether to be a closing tag / 是否是闭合标签
         */
        createElement(tagName, options) {
            require('../addon/token');
            return this.createElement(tagName, options);
        }
        /**
         * Create a text node
         *
         * 创建纯文本节点
         * @param data text content / 文本内容
         */
        createTextNode(data = '') {
            return new text_1.AstText(data);
        }
        /**
         * Create an AstRange object
         *
         * 创建AstRange对象
         */
        createRange() {
            const { AstRange } = require('../lib/range');
            return new AstRange();
        }
        /**
         * Check if a title is an interwiki link
         *
         * 判断标题是否是跨维基链接
         * @param title title / 标题
         */
        isInterwiki(title) {
            return (0, string_1.isInterwiki)(title, this.#config);
        }
        /** @private */
        cloneChildNodes() {
            return this.childNodes.map(child => child.cloneNode());
        }
        /**
         * Deep clone the node
         *
         * 深拷贝节点
         */
        cloneNode() {
            /* c8 ignore next 3 */
            if (this.constructor !== Token) {
                this.constructorError('does not specify a cloneNode method');
            }
            const cloned = this.cloneChildNodes();
            return debug_1.Shadow.run(() => {
                const token = new Token(undefined, this.#config, [], this.getAcceptable());
                token.type = this.type;
                token.pageName = this.pageName;
                token.setAttribute('stage', this.#stage);
                token.setAttribute('include', Boolean(this.#include));
                token.setAttribute('name', this.name);
                token.safeAppend(cloned);
                token.protectChildren(this.#protectedChildren);
                return token;
            });
        }
        /* c8 ignore start */
        /**
         * Get all sections
         *
         * 获取全部章节
         */
        sections() {
            require('../addon/token');
            return this.sections();
        }
        /* c8 ignore stop */
        /**
         * Get a section
         *
         * 获取指定章节
         * @param n rank of the section / 章节序号
         */
        section(n) {
            return this.sections()?.[n];
        }
        /* c8 ignore start */
        /**
         * Get the enclosing HTML tags
         *
         * 获取指定的外层HTML标签
         * @param tag HTML tag name / HTML标签名
         */
        findEnclosingHtml(tag) {
            require('../addon/token');
            return this.findEnclosingHtml(tag);
        }
        /* c8 ignore stop */
        /**
         * Get all categories
         *
         * 获取全部分类
         */
        getCategories() {
            return this.querySelectorAll('category').map(({ name, sortkey }) => [name, sortkey]);
        }
        /**
         * Expand templates
         *
         * 展开模板
         * @since v1.10.0
         */
        expand() {
            const { expandToken } = require('../render/expand');
            return debug_1.Shadow.run(() => expandToken(this).parse());
        }
        /**
         * Parse some magic words
         *
         * 解析部分魔术字
         */
        solveConst() {
            const { expandToken } = require('../render/expand');
            return debug_1.Shadow.run(() => expandToken(this, false).parse());
        }
        /**
         * Merge plain child tokens of a plain token
         *
         * 合并普通节点的普通子节点
         */
        flatten() {
            if (this.isPlain()) {
                for (const child of this.childNodes) {
                    if (child.type !== 'text' && child.isPlain()) {
                        child.insertAdjacent([...child.childNodes], 1);
                        child.remove();
                    }
                }
            }
        }
        /**
         * Generate HTML
         *
         * 生成HTML
         * @since v1.10.0
         */
        toHtml() {
            const { viewOnly, internal } = index_1.default;
            let output;
            index_1.default.internal = true;
            if (this.type === 'root') {
                const { expandToken } = require('../render/expand'), { toHtml } = require('../render/html');
                index_1.default.viewOnly = true;
                const expanded = debug_1.Shadow.run(() => expandToken(this).parse(undefined, false, true)), e = new Event('expand');
                this.dispatchEvent(e, { type: 'expand', token: expanded });
                index_1.default.viewOnly = false;
                output = toHtml(expanded);
            }
            else {
                index_1.default.viewOnly = false;
                output = this.cloneNode().toHtmlInternal();
            }
            index_1.default.viewOnly = viewOnly;
            index_1.default.internal = internal;
            return output;
        }
        /**
         * 构建列表
         * @param recursive 是否递归
         */
        #buildLists(recursive) {
            for (let i = 0; i < this.length; i++) {
                const child = this.childNodes[i];
                if (child.is('list') || child.is('dd')) {
                    child.getRange();
                }
                else if (recursive && child.type !== 'text') {
                    child.#buildLists(true);
                }
            }
        }
        /**
         * Build lists
         *
         * 构建列表
         * @since v1.17.1
         */
        buildLists() {
            this.#buildLists(true);
        }
        /** @private */
        toHtmlInternal(opt) {
            for (const child of this.childNodes) {
                if (child.type === 'text') {
                    child.removeBlankLines();
                }
            }
            this.#buildLists();
            this.normalize();
            return (0, html_1.html)(this.childNodes, '', opt);
        }
    };
})();
exports.Token = Token;
constants_1.classes['Token'] = __filename;
