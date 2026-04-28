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
exports.LinkBaseToken = void 0;
const lint_1 = require("../../util/lint");
const constants_1 = require("../../util/constants");
const debug_1 = require("../../util/debug");
const rect_1 = require("../../lib/rect");
const padded_1 = require("../../mixin/padded");
const noEscape_1 = require("../../mixin/noEscape");
const index_1 = __importDefault(require("../../index"));
const index_2 = require("../index");
const atom_1 = require("../atom");
/* NOT FOR BROWSER */
const string_1 = require("../../util/string");
const cached_1 = require("../../mixin/cached");
/* NOT FOR BROWSER END */
/**
 * internal link
 *
 * 内链
 * @classdesc `{childNodes: [AtomToken, ...Token[]]}`
 */
let LinkBaseToken = (() => {
    let _classDecorators = [noEscape_1.noEscape, (0, padded_1.padded)('[[')];
    let _classDescriptor;
    let _classExtraInitializers = [];
    let _classThis;
    let _classSuper = index_2.Token;
    let _instanceExtraInitializers = [];
    let _toHtmlInternal_decorators;
    var LinkBaseToken = class extends _classSuper {
        static { _classThis = this; }
        static {
            const _metadata = typeof Symbol === "function" && Symbol.metadata ? Object.create(_classSuper[Symbol.metadata] ?? null) : void 0;
            _toHtmlInternal_decorators = [(0, cached_1.cached)()];
            __esDecorate(this, null, _toHtmlInternal_decorators, { kind: "method", name: "toHtmlInternal", static: false, private: false, access: { has: obj => "toHtmlInternal" in obj, get: obj => obj.toHtmlInternal }, metadata: _metadata }, null, _instanceExtraInitializers);
            __esDecorate(null, _classDescriptor = { value: _classThis }, _classDecorators, { kind: "class", name: _classThis.name, metadata: _metadata }, null, _classExtraInitializers);
            LinkBaseToken = _classThis = _classDescriptor.value;
            if (_metadata) Object.defineProperty(_classThis, Symbol.metadata, { enumerable: true, configurable: true, writable: true, value: _metadata });
            __runInitializers(_classThis, _classExtraInitializers);
        }
        #bracket = (__runInitializers(this, _instanceExtraInitializers), true);
        #delimiter;
        // @ts-expect-error lazy initialization
        #title;
        /* NOT FOR BROWSER END */
        /** full link / 完整链接 */
        get link() {
            LSP: return this.#title;
        }
        /* PRINT ONLY */
        /** 片段标识符 */
        get fragment() {
            LSP: return this.#title.fragment;
        }
        /* PRINT ONLY END */
        /* NOT FOR BROWSER */
        set fragment(fragment) {
            this.setFragment(fragment);
        }
        set link(link) {
            this.setTarget(link);
        }
        /** interwiki */
        get interwiki() {
            return this.#title.interwiki;
        }
        /** @throws `RangeError` 非法的跨维基前缀 */
        set interwiki(interwiki) {
            if ((0, debug_1.isLink)(this.type)) {
                const { prefix, main, fragment } = this.#title, link = `${interwiki}:${prefix}${main}${fragment === undefined ? '' : `#${fragment}`}`;
                /* c8 ignore next 3 */
                if (interwiki && !this.isInterwiki(link)) {
                    throw new RangeError(`${interwiki} is not a valid interwiki prefix!`);
                }
                this.setTarget(link);
            }
        }
        /* NOT FOR BROWSER END */
        /**
         * @param link 链接标题
         * @param linkText 链接显示文字
         * @param delimiter `|`
         */
        constructor(link, linkText, config, accum = [], delimiter = '|') {
            super(undefined, config, accum, {
                AtomToken: 0, Token: 1,
            });
            this.insertAt(new atom_1.AtomToken(link, 'link-target', config, accum, {
                'Stage-2': ':', '!ExtToken': '', '!HeadingToken': '',
            }));
            if (linkText !== undefined) {
                const inner = new index_2.Token(linkText, config, accum, {
                    'Stage-5': ':', QuoteToken: ':', ConverterToken: ':',
                });
                inner.type = 'link-text';
                inner.setAttribute('stage', constants_1.MAX_STAGE - 1);
                this.insertAt(inner);
            }
            this.#delimiter = delimiter;
            /* NOT FOR BROWSER */
            this.protectChildren(0);
        }
        /** 更新name */
        #setName() {
            if (this.type !== 'ext-inner') {
                this.setAttribute('name', this.#title.title);
            }
        }
        /** @private */
        afterBuild() {
            this.#title = this.getTitle();
            if (this.#delimiter.includes('\0')) {
                this.#delimiter = this.buildFromStr(this.#delimiter, constants_1.BuildMethod.String);
            }
            this.#setName();
            super.afterBuild();
            /* NOT FOR BROWSER */
            const /** @implements */ linkListener = (e, data) => {
                const { prevTarget } = e, { type } = this;
                if (prevTarget?.is('link-target')
                    && !(this.is('ext-inner') && this.parentNode?.selfClosing)) {
                    const name = prevTarget.text(), titleObj = this.getTitle(), { interwiki, ns, valid } = titleObj;
                    if (!valid) {
                        (0, debug_1.undo)(e, data);
                        throw new Error(`Invalid link target: ${name}`);
                    }
                    else if ((type === 'category' || type === 'ext-inner') && (interwiki || ns !== 14)
                        || (type === 'file' || type === 'gallery-image' || type === 'imagemap-image')
                            && (interwiki || ns !== 6)) {
                        (0, debug_1.undo)(e, data);
                        throw new Error(`${type === 'category' ? 'Category' : 'File'} link cannot change namespace: ${name}`);
                    }
                    else if (type === 'link' && !interwiki && (ns === 6 || ns === 14)
                        && !name.trim().startsWith(':')) {
                        const { firstChild } = prevTarget;
                        if (firstChild?.type === 'text') {
                            firstChild.insertData(0, ':');
                        }
                        else {
                            prevTarget.prepend(':');
                        }
                    }
                    this.#title = titleObj;
                    this.#setName();
                }
            };
            this.addEventListener(['remove', 'insert', 'replace', 'text'], linkListener);
        }
        /** @private */
        setAttribute(key, value) {
            if (key === 'bracket') {
                this.#bracket = value;
            }
            else if (key === 'title') {
                /* c8 ignore next */
                this.#title = value;
            }
            else {
                super.setAttribute(key, value);
            }
        }
        /** @private */
        toString(skip) {
            const str = super.toString(skip, this.#delimiter);
            return this.#bracket ? `[[${str}]]` : str;
        }
        /** @private */
        text() {
            const str = super.text('|');
            return this.#bracket ? `[[${str}]]` : str;
        }
        /** @private */
        getAttribute(key) {
            return key === 'title' ? this.#title : super.getAttribute(key);
        }
        /** @private */
        getGaps(i) {
            return i === 0 ? this.#delimiter.length : 1;
        }
        /** @private */
        lint(start = this.getAbsoluteIndex(), re) {
            LINT: {
                const errors = super.lint(start, re), { childNodes: [target, linkText], type } = this, { encoded, fragment } = this.#title, { lintConfig } = index_1.default, { computeEditInfo, fix } = lintConfig, rect = new rect_1.BoundingRect(this, start);
                let rule = 'unknown-page', s = lintConfig.getSeverity(rule);
                if (s && target.childNodes.some(({ type: t }) => t === 'template')) {
                    errors.push((0, lint_1.generateForChild)(target, rect, rule, 'template-in-link', s));
                }
                rule = 'url-encoding';
                s = lintConfig.getSeverity(rule);
                if (s && encoded) {
                    const e = (0, lint_1.generateForChild)(target, rect, rule, 'unnecessary-encoding', s);
                    if (computeEditInfo || fix) {
                        e.fix = (0, lint_1.fixByDecode)(e, target);
                    }
                    errors.push(e);
                }
                rule = 'pipe-like';
                s = lintConfig.getSeverity(rule, 'link');
                if (s && (type === 'link' || type === 'category')) {
                    const j = linkText?.childNodes.findIndex(c => c.type === 'text' && c.data.includes('|')), textNode = linkText?.childNodes[j];
                    if (textNode) {
                        const e = (0, lint_1.generateForChild)(linkText, rect, rule, 'pipe-in-link', s);
                        if (computeEditInfo) {
                            const i = e.startIndex + linkText.getRelativeIndex(j);
                            e.suggestions = [(0, lint_1.fixByPipe)(i, textNode.data)];
                        }
                        errors.push(e);
                    }
                }
                rule = 'no-ignored';
                s = lintConfig.getSeverity(rule, 'fragment');
                if (s && fragment !== undefined && !(0, debug_1.isLink)(type)) {
                    const e = (0, lint_1.generateForChild)(target, rect, rule, 'useless-fragment', s);
                    if (computeEditInfo || fix) {
                        const j = target.childNodes.findIndex(c => c.type === 'text' && c.data.includes('#')), textNode = target.childNodes[j];
                        if (textNode) {
                            e.fix = (0, lint_1.fixByRemove)(e, target.getRelativeIndex(j) + textNode.data.indexOf('#'));
                        }
                    }
                    errors.push(e);
                }
                return errors;
            }
        }
        /** @private */
        getTitle(temporary, halfParsed) {
            return this.normalizeTitle(this.firstChild.text(), 0, { halfParsed, temporary, decode: true, selfLink: true });
        }
        /** @private */
        print() {
            PRINT: return super.print(this.#bracket ? { pre: '[[', post: ']]', sep: this.#delimiter } : { sep: this.#delimiter });
        }
        /** @private */
        json(_, depth, start = this.getAbsoluteIndex()) {
            LSP: {
                const json = super.json(undefined, depth, start), { type, fragment } = this;
                if (fragment !== undefined && (type === 'link' || type === 'redirect-target')) {
                    json['fragment'] = fragment;
                }
                return json;
            }
        }
        /* NOT FOR BROWSER */
        cloneNode() {
            const [link, ...linkText] = this.cloneChildNodes();
            return debug_1.Shadow.run(() => {
                const C = this.constructor, token = new C('', undefined, this.getAttribute('config'));
                token.firstChild.safeReplaceWith(link);
                token.safeAppend(linkText);
                return token;
            });
        }
        /* c8 ignore start */
        /**
         * Set the link target
         *
         * 设置链接目标
         * @param link link target / 链接目标
         */
        setTarget(link) {
            require('../../addon/link');
            this.setTarget(link);
        }
        /* c8 ignore stop */
        /* c8 ignore start */
        /**
         * Set the fragment
         *
         * 设置片段标识符
         * @param fragment URI fragment / 片段标识符
         */
        setFragment(fragment) {
            require('../../addon/link');
            this.setFragment(fragment);
        }
        /* c8 ignore stop */
        /**
         * Set the link text
         *
         * 设置链接显示文字
         * @param linkStr link text / 链接显示文字
         */
        setLinkText(linkStr) {
            require('../../addon/link');
            this.setLinkText(linkStr);
        }
        /** @private */
        toHtmlInternal(opt) {
            if (this.is('link')
                || this.is('redirect-target')
                || this.is('category')) {
                const { link, length, lastChild, type, pageName } = this;
                let attr;
                if (type === 'link' && link.title === pageName && !link.fragment) {
                    attr = 'class="mw-selflink"';
                }
                else {
                    const title = link.getTitleAttr();
                    attr = `${link.interwiki && 'class="extiw" '}href="${link.getUrl()}"${title && ` title="${title}"`}`;
                }
                return `<a ${attr}>${type === 'link' && length > 1
                    ? lastChild.toHtmlInternal({
                        ...opt,
                        nowrap: true,
                    })
                    : (0, string_1.sanitize)(this.innerText)}</a>`;
            }
            /* c8 ignore next */
            return '';
        }
    };
    return LinkBaseToken = _classThis;
})();
exports.LinkBaseToken = LinkBaseToken;
constants_1.classes['LinkBaseToken'] = __filename;
