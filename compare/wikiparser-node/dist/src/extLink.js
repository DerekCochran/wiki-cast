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
exports.ExtLinkToken = void 0;
const constants_1 = require("../util/constants");
const lint_1 = require("../util/lint");
const padded_1 = require("../mixin/padded");
const index_1 = __importDefault(require("../index"));
const index_2 = require("./index");
const magicLink_1 = require("./magicLink");
/* NOT FOR BROWSER */
const string_1 = require("../util/string");
const debug_1 = require("../util/debug");
const cached_1 = require("../mixin/cached");
/* NOT FOR BROWSER END */
/**
 * external link
 *
 * 外链
 * @classdesc `{childNodes: [MagicLinkToken, ?Token]}`
 */
let ExtLinkToken = (() => {
    let _classDecorators = [(0, padded_1.padded)('[')];
    let _classDescriptor;
    let _classExtraInitializers = [];
    let _classThis;
    let _classSuper = index_2.Token;
    let _instanceExtraInitializers = [];
    let _toHtmlInternal_decorators;
    var ExtLinkToken = class extends _classSuper {
        static { _classThis = this; }
        static {
            const _metadata = typeof Symbol === "function" && Symbol.metadata ? Object.create(_classSuper[Symbol.metadata] ?? null) : void 0;
            _toHtmlInternal_decorators = [(0, cached_1.cached)()];
            __esDecorate(this, null, _toHtmlInternal_decorators, { kind: "method", name: "toHtmlInternal", static: false, private: false, access: { has: obj => "toHtmlInternal" in obj, get: obj => obj.toHtmlInternal }, metadata: _metadata }, null, _instanceExtraInitializers);
            __esDecorate(null, _classDescriptor = { value: _classThis }, _classDecorators, { kind: "class", name: _classThis.name, metadata: _metadata }, null, _classExtraInitializers);
            ExtLinkToken = _classThis = _classDescriptor.value;
            if (_metadata) Object.defineProperty(_classThis, Symbol.metadata, { enumerable: true, configurable: true, writable: true, value: _metadata });
            __runInitializers(_classThis, _classExtraInitializers);
        }
        #space = __runInitializers(this, _instanceExtraInitializers);
        /* NOT FOR BROWSER END */
        get type() {
            return 'ext-link';
        }
        /* NOT FOR BROWSER */
        /** text of the link / 链接显示文字 */
        get innerText() {
            return this.length > 1
                ? this.lastChild.text()
                : `[${this.getRootNode().querySelectorAll('ext-link[childElementCount=1]').indexOf(this) + 1}]`;
        }
        set innerText(text) {
            this.setLinkText(text);
        }
        /** URL protocol / 协议 */
        get protocol() {
            return this.firstChild.protocol;
        }
        set protocol(value) {
            this.firstChild.protocol = value;
        }
        /** link / 链接 */
        get link() {
            return this.firstChild.link;
        }
        set link(url) {
            this.firstChild.link = url;
        }
        /* NOT FOR BROWSER END */
        /**
         * @param url 网址
         * @param space 空白字符
         * @param text 链接文字
         */
        constructor(url, space = '', text = '', config, accum = []) {
            super(undefined, config, accum, {
                MagicLinkToken: 0, Token: 1,
            });
            const link = url && /^\0\d+f\x7F$/u.test(url)
                ? accum[Number(url.slice(1, -2))]
                // @ts-expect-error abstract class
                : new magicLink_1.MagicLinkToken(url, 'ext-link-url', config, accum);
            this.insertAt(link);
            this.#space = space;
            if (text) {
                const inner = new index_2.Token(text, config, accum, {
                    'Stage-7': ':', ConverterToken: ':',
                });
                inner.type = 'ext-link-text';
                inner.setAttribute('stage', constants_1.MAX_STAGE - 1);
                this.insertAt(inner);
            }
            /* NOT FOR BROWSER */
            this.protectChildren(0);
        }
        /** @private */
        toString(skip) {
            if (this.length === 1) {
                return `[${super.toString(skip)}${this.#space}]`;
            }
            /* NOT FOR BROWSER */
            this.#correct();
            (0, string_1.normalizeSpace)(this.lastChild);
            /* NOT FOR BROWSER END */
            return `[${super.toString(skip, this.#space)}]`;
        }
        /** @private */
        text() {
            /* NOT FOR BROWSER */
            (0, string_1.normalizeSpace)(this.childNodes[1]);
            /* NOT FOR BROWSER END */
            return `[${super.text(' ')}]`;
        }
        /** @private */
        getGaps() {
            /* NOT FOR BROWSER */
            this.#correct();
            /* NOT FOR BROWSER END */
            return this.#space.length;
        }
        /** @private */
        lint(start = this.getAbsoluteIndex(), re) {
            LINT: {
                const errors = super.lint(start, re), rule = 'var-anchor', s = index_1.default.lintConfig.getSeverity(rule, 'extLink');
                if (s && this.length === 1 && this.isInside('heading-title')) {
                    errors.push((0, lint_1.generateForSelf)(this, { start }, rule, 'variable-anchor', s));
                }
                return errors;
            }
        }
        /** @private */
        print() {
            PRINT: return super.print(this.length === 1 ? { pre: '[', post: `${this.#space}]` } : { pre: '[', sep: this.#space, post: ']' });
        }
        /* NOT FOR BROWSER */
        cloneNode() {
            const [url, text] = this.cloneChildNodes();
            return debug_1.Shadow.run(() => {
                // @ts-expect-error abstract class
                const token = new ExtLinkToken(undefined, this.#space, '', this.getAttribute('config'));
                token.firstChild.safeReplaceWith(url);
                if (text) {
                    token.insertAt(text);
                }
                return token;
            });
        }
        /** 修正空白字符 */
        #correct() {
            const { lastChild, length } = this, { firstChild } = lastChild;
            if (!this.#space
                && length > 1
                && (firstChild?.type === 'text' || firstChild?.is('converter'))
                // 都替换成`<`肯定不对，但无妨
                && /^[^[\]<>"\0-\x1F\x7F\p{Zs}\uFFFD]/u
                    .test(lastChild.text().replace(/&[lg]t;/u, '<'))) {
                this.#space = ' ';
            }
        }
        /**
         * Set the text of the link
         *
         * 设置链接显示文字
         * @param str text of the link / 链接显示文字
         */
        setLinkText(str) {
            const { length, lastChild } = this, root = index_1.default.parseWithRef(str, this, 7);
            if (length === 1) {
                root.type = 'ext-link-text';
                root.setAttribute('acceptable', {
                    'Stage-7': ':', ConverterToken: ':',
                });
                this.insertAt(root);
            }
            else {
                lastChild.safeReplaceChildren(root.childNodes);
            }
            this.#space ||= ' ';
        }
        /** @private */
        toHtmlInternal(opt) {
            const { length, lastChild } = this;
            let innerText, href;
            if (length > 1) {
                lastChild.normalize();
                const { childNodes } = lastChild, i = childNodes.findIndex(child => child.is('link')
                    || child.is('file')
                        && child.getValue('link')?.trim() !== '');
                if (i !== -1) {
                    const after = childNodes.slice(i);
                    this.insertAdjacent(after, 1);
                }
                innerText = lastChild.toHtmlInternal(opt);
            }
            else {
                ({ innerText } = this);
            }
            try {
                ({ href } = this.getUrl());
            }
            catch { }
            return `<a rel="nofollow" class="external"${href === undefined ? '' : ` href="${href}"`}>${innerText}</a>`;
        }
        /**
         * Get the URL
         *
         * 获取网址
         */
        getUrl() {
            return this.firstChild.getUrl();
        }
        /**
         * Set the target of the link
         *
         * 设置外链目标
         * @param url URL containing the protocol / 含协议的网址
         */
        setTarget(url) {
            this.firstChild.setTarget(url);
        }
    };
    return ExtLinkToken = _classThis;
})();
exports.ExtLinkToken = ExtLinkToken;
constants_1.classes['ExtLinkToken'] = __filename;
