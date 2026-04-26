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
exports.MagicLinkToken = void 0;
const lint_1 = require("../util/lint");
const string_1 = require("../util/string");
const rect_1 = require("../lib/rect");
const index_1 = __importDefault(require("../index"));
const index_2 = require("./index");
/* NOT FOR BROWSER */
const common_1 = require("@bhsd/common");
const debug_1 = require("../util/debug");
const constants_1 = require("../util/constants");
const clone_1 = require("../mixin/clone");
const syntax_1 = require("../mixin/syntax");
const cached_1 = require("../mixin/cached");
const space = String.raw `(?:[${string_1.zs}\t]|&nbsp;|&#0*160;|&#[xX]0*[aA]0;)`;
// eslint-disable-next-line @typescript-eslint/no-unused-expressions
/(?:[\p{Zs}\t]|&nbsp;|&#0*160;|&#[xX]0*[aA]0;)+/gu;
const spaceRegex = new RegExp(`${space}+`, 'gu');
/* NOT FOR BROWSER */
const spdash = String.raw `(?:[\p{Zs}\t-]|&nbsp;|&#0*160;|&#[xX]0*[aA]0;)`;
// eslint-disable-next-line @typescript-eslint/no-unused-expressions
/^(ISBN)[\p{Zs}\t]+(?:97[89][\p{Zs}\t-]?)?(?:\d[\p{Zs}\t-]?){9}[\dxX]$/u;
const isbnPattern = new RegExp(String.raw `^(ISBN)${space}+(?:97[89]${spdash}?)?(?:\d${spdash}?){9}[\dxX]$`, 'u');
// eslint-disable-next-line @typescript-eslint/no-unused-expressions
/^(RFC|PMID)[\p{Zs}\t]+\d+$/u;
const rfcPattern = new RegExp(String.raw `^(RFC|PMID)${space}+\d+$`, 'u');
/^(ftp:\/\/|\/\/)/iu; // eslint-disable-line @typescript-eslint/no-unused-expressions
const getUrlRegex = (0, common_1.getRegex)(protocol => new RegExp(`^(${protocol})`, 'iu'));
/* NOT FOR BROWSER END */
/**
 * free external link
 *
 * 自由外链
 * @classdesc `{childNodes: (AstText|CommentToken|IncludeToken|NoincludeToken)[]}`
 */
let MagicLinkToken = (() => {
    let _classDecorators = [(0, syntax_1.syntax)()];
    let _classDescriptor;
    let _classExtraInitializers = [];
    let _classThis;
    let _classSuper = index_2.Token;
    let _instanceExtraInitializers = [];
    let _cloneNode_decorators;
    let _toHtmlInternal_decorators;
    var MagicLinkToken = class extends _classSuper {
        static { _classThis = this; }
        static {
            const _metadata = typeof Symbol === "function" && Symbol.metadata ? Object.create(_classSuper[Symbol.metadata] ?? null) : void 0;
            _cloneNode_decorators = [clone_1.clone];
            _toHtmlInternal_decorators = [(0, cached_1.cached)()];
            __esDecorate(this, null, _cloneNode_decorators, { kind: "method", name: "cloneNode", static: false, private: false, access: { has: obj => "cloneNode" in obj, get: obj => obj.cloneNode }, metadata: _metadata }, null, _instanceExtraInitializers);
            __esDecorate(this, null, _toHtmlInternal_decorators, { kind: "method", name: "toHtmlInternal", static: false, private: false, access: { has: obj => "toHtmlInternal" in obj, get: obj => obj.toHtmlInternal }, metadata: _metadata }, null, _instanceExtraInitializers);
            __esDecorate(null, _classDescriptor = { value: _classThis }, _classDecorators, { kind: "class", name: _classThis.name, metadata: _metadata }, null, _classExtraInitializers);
            MagicLinkToken = _classThis = _classDescriptor.value;
            if (_metadata) Object.defineProperty(_classThis, Symbol.metadata, { enumerable: true, configurable: true, writable: true, value: _metadata });
            __runInitializers(_classThis, _classExtraInitializers);
        }
        #type = __runInitializers(this, _instanceExtraInitializers);
        /* NOT FOR BROWSER END */
        get type() {
            return this.#type;
        }
        /**
         * text of the link
         *
         * 链接显示文字
         * @since v1.10.0
         */
        get innerText() {
            const map = new Map([['!', '|'], ['=', '=']]);
            let link = (0, string_1.text)(this.childNodes.map(child => {
                const { type } = child, name = String(child.name);
                return type === 'magic-word' && map.has(name) ? map.get(name) : child;
            }));
            if (this.type === 'magic-link') {
                link = link.replace(spaceRegex, ' ');
            }
            return link;
        }
        /** link / 链接 */
        get link() {
            const { innerText } = this;
            if (this.type === 'magic-link') {
                return innerText.startsWith('ISBN')
                    ? `ISBN ${innerText.slice(5).replace(/[- ]/gu, '')
                        .replace(/x$/u, 'X')}`
                    : innerText;
            }
            return (0, string_1.decodeNumber)(innerText).replace(/\n/gu, '%0A');
        }
        /* NOT FOR BROWSER */
        set link(url) {
            this.setTarget(url);
        }
        /** URL protocol / 协议 */
        get protocol() {
            return this.pattern.exec(this.text())?.[1];
        }
        /** @throws `Error` 特殊外链无法更改协议n */
        set protocol(value) {
            const { link, pattern, type } = this;
            if (type === 'magic-link' || !pattern.test(link)) {
                throw new Error(`Special external link cannot change protocol: ${link}`);
            }
            this.setTarget(link.replace(pattern, value));
        }
        /* NOT FOR BROWSER END */
        /**
         * @param url 网址
         * @param type 类型
         */
        constructor(url, type = 'free-ext-link', config = index_1.default.getConfig(), accum) {
            super(url, config, accum, {
                'Stage-1': '1:', '!ExtToken': '', AstText: ':', TranscludeToken: ':',
            });
            this.#type = type;
            /* NOT FOR BROWSER */
            let pattern;
            if (type === 'magic-link') {
                pattern = url?.startsWith('ISBN') ? isbnPattern : rfcPattern;
            }
            else {
                pattern = getUrlRegex(config.protocol + (type === 'ext-link-url' ? '|//' : ''));
            }
            this.setAttribute('pattern', pattern);
        }
        /** 判定无效的ISBN */
        #lint() {
            if (this.type === 'magic-link') {
                const { link } = this;
                if (link.startsWith('ISBN')) {
                    // eslint-disable-next-line unicorn/no-useless-spread
                    const digits = [...link.slice(5)].map(s => s === 'X' ? 10 : Number(s));
                    return digits.length === 10
                        ? digits.reduce((sum, d, i) => sum + d * (10 - i), 0) % 11 !== 0
                        : digits.length === 13 && (digits[12] === 10
                            || digits.reduce((sum, d, i) => sum + d * (i % 2 ? 3 : 1), 0) % 10 !== 0);
                }
            }
            return false;
        }
        /** @private */
        lint(start = this.getAbsoluteIndex(), re) {
            LINT: {
                const errors = super.lint(start, re), rect = new rect_1.BoundingRect(this, start), { lintConfig } = index_1.default, { type, childNodes } = this;
                if (type === 'magic-link') {
                    const rule = 'invalid-isbn', s = lintConfig.getSeverity(rule);
                    if (s && this.#lint()) {
                        errors.push((0, lint_1.generateForSelf)(this, rect, rule, 'invalid-isbn', s));
                    }
                    return errors;
                }
                let rule = 'invalid-url', severity = lintConfig.getSeverity(rule);
                if (severity && !this.querySelector('magic-word')) {
                    try {
                        this.getUrl();
                    }
                    catch {
                        errors.push((0, lint_1.generateForSelf)(this, rect, rule, 'invalid-url', severity));
                    }
                }
                const pipe = type === 'ext-link-url';
                rule = 'unterminated-url';
                severity = lintConfig.getSeverity(rule, pipe ? 'pipe' : 'punctuation');
                if (severity) {
                    const regex = pipe ? /\|/u : /[，；。：！？（）]+/u, child = childNodes.find((c) => c.type === 'text' && regex.test(c.data));
                    if (child) {
                        const { data } = child, e = (0, lint_1.generateForChild)(child, rect, rule, index_1.default.msg('in-url', pipe ? '"|"' : 'full-width-punctuation'), severity);
                        if (lintConfig.computeEditInfo) {
                            const { index, 0: s } = regex.exec(data), i = e.startIndex + index;
                            e.suggestions = pipe
                                ? [(0, lint_1.fixBySpace)(i, 1)]
                                : [
                                    (0, lint_1.fixBySpace)(i),
                                    { desc: index_1.default.msg('encode'), range: [i, i + s.length], text: encodeURI(s) },
                                ];
                        }
                        errors.push(e);
                    }
                }
                return errors;
            }
        }
        /**
         * Get the URL
         *
         * 获取网址
         * @param articlePath article path / 条目路径
         */
        getUrl(articlePath) {
            LSP: {
                const { type } = this;
                let { link } = this;
                if (type === 'magic-link') {
                    if (link.startsWith('ISBN')) {
                        return this
                            .normalizeTitle(`BookSources/${link.slice(5)}`, -1, { temporary: true })
                            .getUrl(articlePath);
                    }
                    link = link.startsWith('RFC')
                        ? `https://datatracker.ietf.org/doc/html/rfc${link.slice(4)}`
                        : `https://pubmed.ncbi.nlm.nih.gov/${link.slice(5)}`;
                }
                else if (link.startsWith('//')) {
                    link = `https:${link}`;
                }
                return new URL(link);
            }
        }
        /* PRINT ONLY */
        /** @private */
        getAttribute(key) {
            return key === 'invalid' ? this.#lint() : super.getAttribute(key);
        }
        /* PRINT ONLY END */
        /* NOT FOR BROWSER */
        cloneNode() {
            // @ts-expect-error abstract class
            const token = new MagicLinkToken(undefined, this.type, this.getAttribute('config'));
            token.setAttribute('pattern', this.pattern);
            return token;
        }
        insertAt(token, i) {
            if (typeof token !== 'string') {
                const { type, name } = token;
                if (type === 'template') {
                    this.constructorError('cannot insert a template');
                }
                else if (!debug_1.Shadow.running && type === 'magic-word' && name !== '!' && name !== '=') {
                    this.constructorError('cannot insert magic words other than "{{!}}" or "{{=}}"');
                }
            }
            return super.insertAt(token, i);
        }
        /**
         * Set the target of the link
         *
         * 设置外链目标
         * @param url URL containing the protocol / 含协议的网址
         */
        setTarget(url) {
            const { childNodes } = index_1.default.parseWithRef(url, this, 2);
            this.safeReplaceChildren(childNodes);
        }
        /**
         * Check if it is a parameter of a template or magic word
         *
         * 是否是模板或魔术字参数
         */
        isParamValue() {
            return this.closest('parameter')?.getValue() === this.text();
        }
        /** @private */
        toHtmlInternal() {
            const { type, innerText, protocol } = this;
            let url;
            try {
                url = this.getUrl();
            }
            catch { }
            const attrs = type === 'free-ext-link' || type === 'ext-link-url'
                ? ` rel="nofollow" class="external${type === 'free-ext-link' ? ' free' : ''}"${typeof url === 'object' ? ` href="${url.href}"` : ''}`
                : (protocol === 'ISBN' ? '' : ' class="external" rel="nofollow"')
                    + (url === undefined ? '' : ` href="${typeof url === 'string' ? url : url.href}"`);
            return `<a${attrs}>${innerText}</a>`;
        }
    };
    return MagicLinkToken = _classThis;
})();
exports.MagicLinkToken = MagicLinkToken;
constants_1.classes['MagicLinkToken'] = __filename;
