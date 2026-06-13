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
exports.ExtToken = void 0;
const lint_1 = require("../../util/lint");
const rect_1 = require("../../lib/rect");
const index_1 = __importDefault(require("../../index"));
const attributesParent_1 = require("../../mixin/attributesParent");
const index_2 = require("../index");
const index_3 = require("./index");
const attributes_1 = require("../attributes");
/* NOT FOR BROWSER */
const debug_1 = require("../../util/debug");
const constants_1 = require("../../util/constants");
const string_1 = require("../../util/string");
const cached_1 = require("../../mixin/cached");
/**
 * extension tag
 *
 * 扩展标签
 * @classdesc `{childNodes: [AttributesToken, Token]}`
 */
let ExtToken = (() => {
    let _classDecorators = [(0, attributesParent_1.attributesParent)()];
    let _classDescriptor;
    let _classExtraInitializers = [];
    let _classThis;
    let _classSuper = index_3.TagPairToken;
    let _instanceExtraInitializers = [];
    let _toHtmlInternal_decorators;
    var ExtToken = class extends _classSuper {
        static { _classThis = this; }
        static {
            const _metadata = typeof Symbol === "function" && Symbol.metadata ? Object.create(_classSuper[Symbol.metadata] ?? null) : void 0;
            _toHtmlInternal_decorators = [(0, cached_1.cached)()];
            __esDecorate(this, null, _toHtmlInternal_decorators, { kind: "method", name: "toHtmlInternal", static: false, private: false, access: { has: obj => "toHtmlInternal" in obj, get: obj => obj.toHtmlInternal }, metadata: _metadata }, null, _instanceExtraInitializers);
            __esDecorate(null, _classDescriptor = { value: _classThis }, _classDecorators, { kind: "class", name: _classThis.name, metadata: _metadata }, null, _classExtraInitializers);
            ExtToken = _classThis = _classDescriptor.value;
            if (_metadata) Object.defineProperty(_classThis, Symbol.metadata, { enumerable: true, configurable: true, writable: true, value: _metadata });
            __runInitializers(_classThis, _classExtraInitializers);
        }
        /* NOT FOR BROWSER END */
        get type() {
            return 'ext';
        }
        /**
         * @param name 标签名
         * @param attr 标签属性
         * @param inner 内部wikitext
         * @param closed 是否封闭
         * @param include 是否嵌入
         */
        constructor(name, attr, inner, closed, config = index_1.default.getConfig(), include = false, accum = []) {
            const lcName = name.toLowerCase(), 
            // @ts-expect-error abstract class
            attrToken = new attributes_1.AttributesToken(!attr || /^\s/u.test(attr) ? attr : ` ${attr}`, 'ext-attrs', lcName, config, accum), newConfig = {
                ...config,
                ext: config.ext.filter(e => e !== lcName),
                excludes: [...config.excludes],
                inExt: true,
            };
            let innerToken;
            switch (lcName) {
                case 'pre':
                    if (attrToken.getAttr('format') !== 'wikitext') {
                        const { PreToken } = require('../pre');
                        // @ts-expect-error abstract class
                        innerToken = new PreToken(inner, newConfig, accum);
                        break;
                    }
                // fall through
                case 'indicator':
                case 'poem':
                case 'ref':
                case 'option':
                case 'combooption':
                case 'tab':
                case 'tabs':
                case 'poll':
                case 'seo':
                case 'langconvert':
                case 'phonos':
                    if (lcName === 'poem') {
                        newConfig.excludes.push('heading');
                    }
                    else if (lcName === 'tab') {
                        newConfig.ext = newConfig.ext.filter(e => e !== 'tabs');
                    }
                    innerToken = new index_2.Token(inner, newConfig, accum);
                    break;
                case 'dynamicpagelist': {
                    const { ParamTagToken } = require('../multiLine/paramTag');
                    // @ts-expect-error abstract class
                    innerToken = new ParamTagToken(lcName, include, inner, newConfig, accum);
                    break;
                }
                case 'inputbox': {
                    const { InputboxToken } = require('../multiLine/inputbox');
                    // @ts-expect-error abstract class
                    innerToken = new InputboxToken(lcName, include, inner, newConfig, accum);
                    break;
                }
                case 'references': {
                    // NestedToken 依赖 ExtToken
                    const { NestedToken } = require('../nested');
                    newConfig.excludes.push('heading');
                    // @ts-expect-error abstract class
                    innerToken = new NestedToken(inner, include, ['ref'], newConfig, accum);
                    break;
                }
                case 'choose': {
                    // NestedToken 依赖 ExtToken
                    const { NestedToken } = require('../nested');
                    // @ts-expect-error abstract class
                    innerToken = new NestedToken(inner, /<(option|choicetemplate)(\s[^>]*?)?(?:\/>|>([\s\S]*?)<\/(\1)>)/gu, ['option', 'choicetemplate'], newConfig, accum);
                    break;
                }
                case 'combobox': {
                    // NestedToken 依赖 ExtToken
                    const { NestedToken } = require('../nested');
                    // @ts-expect-error abstract class
                    innerToken = new NestedToken(inner, /<(combooption)(\s[^>]*?)?(?:\/>|>([\s\S]*?)<\/(combooption\s*)>)/giu, ['combooption'], newConfig, accum);
                    break;
                }
                case 'gallery': {
                    const { GalleryToken } = require('../multiLine/gallery');
                    // @ts-expect-error abstract class
                    innerToken = new GalleryToken(inner, newConfig, accum);
                    break;
                }
                case 'imagemap': {
                    const { ImagemapToken } = require('../multiLine/imagemap');
                    // @ts-expect-error abstract class
                    innerToken = new ImagemapToken(inner, newConfig, accum);
                    break;
                }
                case 'hiero': {
                    const { CommentedToken } = require('../commented');
                    // @ts-expect-error abstract class
                    innerToken = new CommentedToken(inner, newConfig, accum);
                    break;
                }
                case 'categorytree': {
                    const { CategorytreeToken } = require('../link/categorytree');
                    // @ts-expect-error abstract class
                    innerToken = new CategorytreeToken(inner, undefined, newConfig, accum);
                    break;
                }
                // 更多定制扩展的代码示例：
                // ```
                // case 'extensionName': {
                //   const {ExtensionToken}: typeof import('../extension') = require('../extension');
                //   innerToken = new ExtensionToken(inner, newConfig, accum);
                //   break;
                // }
                // ```
                default: {
                    const { NowikiToken } = require('../nowiki/index');
                    // @ts-expect-error abstract class
                    innerToken = new NowikiToken(inner, newConfig, accum);
                }
            }
            innerToken.setAttribute('name', lcName);
            if (innerToken.type === 'plain') {
                innerToken.type = 'ext-inner';
            }
            super(name, attrToken, innerToken, closed, config, accum);
            __runInitializers(this, _instanceExtraInitializers);
            /* PRINT ONLY */
            this.seal('closed', true);
        }
        /** @private */
        lint(start = this.getAbsoluteIndex(), re) {
            LINT: {
                const errors = super.lint(start, re), { lintConfig } = index_1.default, rect = new rect_1.BoundingRect(this, start);
                if (this.name !== 'nowiki') {
                    const s = this.inHtmlAttrs(), rule = 'parsing-order', severity = s && lintConfig.getSeverity(rule, s === 2 ? 'ext' : 'templateInTable');
                    if (severity) {
                        errors.push((0, lint_1.generateForSelf)(this, rect, rule, 'ext-in-html', severity));
                    }
                }
                if (this.name === 'ref') {
                    let rule = 'var-anchor', s = lintConfig.getSeverity(rule, 'ref');
                    if (s && this.closest('heading-title')) {
                        errors.push((0, lint_1.generateForSelf)(this, rect, rule, 'variable-anchor', s));
                    }
                    rule = 'nested-link';
                    s = lintConfig.getSeverity(rule, 'ref');
                    if (s && this.closest('link-text,ext-link-text')) {
                        errors.push((0, lint_1.generateForSelf)(this, rect, rule, 'ref-in-link', s));
                    }
                }
                return errors;
            }
        }
        /* NOT FOR BROWSER */
        cloneNode() {
            const attrs = this.firstChild.cloneNode(), inner = this.lastChild.cloneNode(), tags = this.getAttribute('tags');
            return debug_1.Shadow.run(() => {
                // @ts-expect-error abstract class
                const token = new ExtToken(tags[0], undefined, undefined, this.selfClosing ? undefined : tags[1], this.getAttribute('config'), this.getAttribute('include'));
                token.firstChild.safeReplaceWith(attrs);
                token.lastChild.safeReplaceWith(inner);
                return token;
            });
        }
        /** @private */
        toHtmlInternal(opt) {
            const { name, firstChild, lastChild } = this;
            if (constants_1.tagHooks.has(name)) {
                return constants_1.tagHooks.get(name)(this);
            }
            else if (name === 'nowiki') {
                const html = lastChild.toHtmlInternal();
                return this.closest('ext-inner')?.name === 'poem' ? html : (0, string_1.newline)(html);
            }
            else if (name === 'pre') {
                const html = lastChild.toHtmlInternal({
                    ...opt,
                    nowrap: false,
                });
                return `<pre${firstChild.toHtmlInternal()}>${this.closest('ext-inner')?.name === 'poem' ? html : (0, string_1.newline)(html)}</pre>`;
            }
            const { renderExt } = require('../../render/extension');
            return renderExt(this, opt);
        }
    };
    return ExtToken = _classThis;
})();
exports.ExtToken = ExtToken;
constants_1.classes['ExtToken'] = __filename;
