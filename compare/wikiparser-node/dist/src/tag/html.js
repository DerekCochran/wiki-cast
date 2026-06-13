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
exports.HtmlToken = void 0;
const lint_1 = require("../../util/lint");
const rect_1 = require("../../lib/rect");
const attributesParent_1 = require("../../mixin/attributesParent");
const index_1 = __importDefault(require("../../index"));
const index_2 = require("./index");
/* NOT FOR BROWSER */
const debug_1 = require("../../util/debug");
const constants_1 = require("../../util/constants");
const html_1 = require("../../util/html");
const cached_1 = require("../../mixin/cached");
const magicWords = new Set(['if', 'ifeq', 'ifexpr', 'ifexist', 'iferror', 'switch']), formattingTags = new Set([
    'b',
    'big',
    'center',
    'cite',
    'code',
    'del',
    'dfn',
    'em',
    'font',
    'i',
    'ins',
    'kbd',
    'mark',
    'pre',
    'q',
    's',
    'samp',
    'small',
    'strike',
    'strong',
    'sub',
    'sup',
    'tt',
    'u',
    'var',
]), obsoleteTags = new Set(['strike', 'big', 'center', 'font', 'tt']);
/**
 * HTML tag
 *
 * HTML标签
 * @classdesc `{childNodes: [AttributesToken]}`
 */
let HtmlToken = (() => {
    let _classDecorators = [(0, attributesParent_1.attributesParent)()];
    let _classDescriptor;
    let _classExtraInitializers = [];
    let _classThis;
    let _classSuper = index_2.TagToken;
    let _instanceExtraInitializers = [];
    let _toHtmlInternal_decorators;
    var HtmlToken = class extends _classSuper {
        static { _classThis = this; }
        static {
            const _metadata = typeof Symbol === "function" && Symbol.metadata ? Object.create(_classSuper[Symbol.metadata] ?? null) : void 0;
            _toHtmlInternal_decorators = [(0, cached_1.cached)()];
            __esDecorate(this, null, _toHtmlInternal_decorators, { kind: "method", name: "toHtmlInternal", static: false, private: false, access: { has: obj => "toHtmlInternal" in obj, get: obj => obj.toHtmlInternal }, metadata: _metadata }, null, _instanceExtraInitializers);
            __esDecorate(null, _classDescriptor = { value: _classThis }, _classDecorators, { kind: "class", name: _classThis.name, metadata: _metadata }, null, _classExtraInitializers);
            HtmlToken = _classThis = _classDescriptor.value;
            if (_metadata) Object.defineProperty(_classThis, Symbol.metadata, { enumerable: true, configurable: true, writable: true, value: _metadata });
            __runInitializers(_classThis, _classExtraInitializers);
        }
        #selfClosing = __runInitializers(this, _instanceExtraInitializers);
        /* NOT FOR BROWSER END */
        get type() {
            return 'html';
        }
        /** whether to be self-closing / 是否自封闭 */
        get selfClosing() {
            return this.#selfClosing;
        }
        /* NOT FOR BROWSER */
        /** @throws `Error` 闭合标签或无效自封闭标签 */
        set selfClosing(value) {
            if (!value) {
                this.#selfClosing = false;
                return;
            }
            else if (this.closing) {
                throw new Error('This is a closing tag!');
            }
            const [tags] = this.getAttribute('config').html;
            if (tags.includes(this.name)) {
                throw new Error(`<${this.name}> tag cannot be self-closing!`);
            }
            this.#selfClosing = true;
        }
        /** @private */
        get closing() {
            return super.closing;
        }
        /** @private */
        set closing(value) {
            if (value) {
                if (this.#selfClosing) {
                    throw new Error('This is a self-closing tag!');
                }
                const [, , tags] = this.getAttribute('config').html;
                if (tags.includes(this.name)) {
                    throw new Error('This is a void tag!');
                }
            }
            super.closing = value;
        }
        /* NOT FOR BROWSER END */
        /**
         * @param name 标签名
         * @param attr 标签属性
         * @param closing 是否闭合
         * @param selfClosing 是否自封闭
         */
        constructor(name, attr, closing, selfClosing, config, accum) {
            super(name, attr, closing, config, accum);
            this.setAttribute('name', name.toLowerCase());
            this.#selfClosing = selfClosing;
        }
        /** @private */
        text() {
            const { closing, selfClosing, name } = this, [, , voidTags] = this.getAttribute('config').html;
            if (voidTags.includes(name)) {
                return closing && name !== 'br' ? '' : super.text('/');
            }
            return super.text(selfClosing && !closing ? '/' : '');
        }
        /** @private */
        lint(start = this.getAbsoluteIndex(), re) {
            LINT: {
                const errors = super.lint(start, re), { name, parentNode, closing, selfClosing } = this, rect = new rect_1.BoundingRect(this, start), { lintConfig } = index_1.default, { computeEditInfo, fix } = lintConfig, severity = this.inTableAttrs();
                let rule = 'h1', s = lintConfig.getSeverity(rule, 'html');
                if (s && name === 'h1' && !closing) {
                    const e = (0, lint_1.generateForSelf)(this, rect, rule, '<h1>', s);
                    if (computeEditInfo) {
                        e.suggestions = [{ desc: 'h2', range: [start + 2, start + 3], text: '2' }];
                    }
                    errors.push(e);
                }
                rule = 'parsing-order';
                s = severity && lintConfig.getSeverity(rule, severity === 2 ? 'html' : 'templateInTable');
                if (s) {
                    const e = (0, lint_1.generateForSelf)(this, rect, rule, 'html-in-table', s);
                    if (computeEditInfo && severity === 2) {
                        e.suggestions = [(0, lint_1.fixByRemove)(e)];
                    }
                    errors.push(e);
                }
                rule = 'obsolete-tag';
                s = lintConfig.getSeverity(rule, name);
                if (s && obsoleteTags.has(name)) {
                    errors.push((0, lint_1.generateForSelf)(this, rect, rule, 'obsolete-tag', s));
                }
                rule = 'bold-header';
                s = lintConfig.getSeverity(rule, name);
                if (s && (name === 'b' || name === 'strong') && this.isInside('heading-title')) {
                    const e = (0, lint_1.generateForSelf)(this, rect, rule, 'bold-in-header', s);
                    if (computeEditInfo) {
                        e.suggestions = [(0, lint_1.fixByRemove)(e)];
                    }
                    errors.push(e);
                }
                const [, flexibleTags, voidTags] = this.getAttribute('config').html, isVoid = voidTags.includes(name), isFlexible = flexibleTags.includes(name), isNormal = !isVoid && !isFlexible;
                rule = 'unmatched-tag';
                if (closing && (selfClosing || isVoid) || selfClosing && isNormal) {
                    s = lintConfig.getSeverity(rule, closing ? 'both' : 'selfClosing');
                    if (s) {
                        const e = (0, lint_1.generateForSelf)(this, rect, rule, closing ? 'closing-and-self-closing' : 'invalid-self-closing', s);
                        if (computeEditInfo || fix) {
                            const open = (0, lint_1.fixByOpen)(start), noSelfClosing = {
                                desc: index_1.default.msg('no-self-closing'),
                                range: [e.endIndex - 2, e.endIndex - 1],
                                text: '',
                            };
                            if (isFlexible) {
                                if (computeEditInfo) {
                                    e.suggestions = [open, noSelfClosing];
                                }
                            }
                            else if (closing) {
                                e.fix = isVoid ? open : noSelfClosing;
                            }
                            else if (computeEditInfo) {
                                e.suggestions = [
                                    noSelfClosing,
                                    (0, lint_1.fixByClose)(e.endIndex, `></${name}>`, -2),
                                ];
                            }
                        }
                        errors.push(e);
                    }
                }
                else if (!this.findMatchingTag()) {
                    const error = (0, lint_1.generateForSelf)(this, rect, rule, closing ? 'unmatched-closing' : 'unclosed-tag'), ancestor = this.closest('magic-word');
                    if (ancestor && magicWords.has(ancestor.name)) {
                        s = lintConfig.getSeverity(rule, 'conditional');
                    }
                    else if (closing) {
                        s = lintConfig.getSeverity(rule, 'closing');
                        error.suggestions = [(0, lint_1.fixByRemove)(error)];
                    }
                    else {
                        s = lintConfig.getSeverity(rule, 'opening');
                        const childNodes = parentNode?.childNodes;
                        if (formattingTags.has(name)) {
                            if (childNodes?.slice(0, childNodes.indexOf(this)).some(tag => tag.is('html') && tag.name === name && !tag.findMatchingTag())) {
                                error.suggestions = [(0, lint_1.fixByClose)(start + 1, '/')];
                            }
                            if (this.isInside('heading-title')) {
                                error.rule = 'format-leakage';
                                s = lintConfig.getSeverity('format-leakage', name);
                            }
                        }
                    }
                    if (s) {
                        error.severity = s;
                        errors.push(error);
                    }
                }
                return errors;
            }
        }
        /* PRINT ONLY */
        /** @private */
        getAttribute(key) {
            return key === 'invalid' ? (this.inTableAttrs() === 2) : super.getAttribute(key);
        }
        /** @private */
        json(_, depth, start = this.getAbsoluteIndex()) {
            LSP: {
                const json = super.json(undefined, depth, start);
                json['selfClosing'] = this.#selfClosing;
                return json;
            }
        }
        /* PRINT ONLY END */
        /* NOT FOR BROWSER */
        cloneNode() {
            const [attr] = this.cloneChildNodes();
            // @ts-expect-error abstract class
            return debug_1.Shadow.run(() => new HtmlToken(this.tag, attr, this.closing, this.#selfClosing, this.getAttribute('config')));
        }
        /**
         * Change the tag name
         *
         * 更换标签名
         * @param tag tag name / 标签名
         * @throws `RangeError` 非法的HTML标签
         */
        replaceTag(tag) {
            const name = tag.toLowerCase();
            if (!this.getAttribute('config').html.some(tags => tags.includes(name))) {
                throw new RangeError(`Invalid HTML tag: ${tag}`);
            }
            this.setAttribute('name', name);
            this.tag = tag;
        }
        /**
         * Fix the invalid self-closing tag
         *
         * 修复无效自封闭标签
         * @throws `Error` 无法修复无效自封闭标签
         */
        fix() {
            const [normalTags] = this.getAttribute('config').html, { parentNode, name: tagName, firstChild, selfClosing } = this;
            if (!parentNode || !selfClosing || !normalTags.includes(tagName)) {
                return;
            }
            else if (firstChild.text().trim()) {
                this.#selfClosing = false;
                this.after(index_1.default.parseWithRef(`</${this.name}>`, this, 3, false).firstChild);
                return;
            }
            const { childNodes } = parentNode, i = childNodes.indexOf(this), prevSiblings = childNodes.slice(0, i)
                .filter((child) => child.is('html') && child.name === tagName), imbalance = prevSiblings.reduce((acc, { closing }) => acc + (closing ? 1 : -1), 0);
            if (imbalance < 0) {
                this.#selfClosing = false;
                this.closing = true;
            }
            else {
                throw new Error(`Cannot fix invalid self-closing tag: The previous ${imbalance} closing tag(s) are unmatched`);
            }
        }
        /** @private */
        toHtmlInternal() {
            const { closing, name } = this, [, selfClosingTags, voidTags] = this.getAttribute('config').html, tag = name + (closing ? '' : super.toHtmlInternal());
            if (voidTags.includes(name)) {
                return closing && name !== 'br' ? '' : `<${tag}>`;
            }
            const result = `<${closing ? '/' : ''}${tag}>${this.#selfClosing && !closing && selfClosingTags.includes(name) ? `</${name}>` : ''}`;
            if (/^h\d$/u.test(name) && (this.closing || !this.id)) {
                const matched = this.findMatchingTag();
                if (matched) {
                    if (closing) {
                        return result + (matched.id ? '' : '</div>');
                    }
                    const range = this.createRange();
                    range.setStartAfter(this);
                    range.setEndBefore(matched);
                    return `<div class="mw-heading mw-heading${name.slice(-1)}">${result.slice(0, -1)} id="${(0, html_1.getId)(range.cloneContents())}">`;
                }
            }
            return result;
        }
        /** @private */
        getRange() {
            const { selfClosing, name } = this, [, selfClosingTags, voidTags] = this.getAttribute('config').html;
            if (voidTags.includes(name) || selfClosing && selfClosingTags.includes(name)) {
                return undefined;
            }
            return super.getRange();
        }
    };
    return HtmlToken = _classThis;
})();
exports.HtmlToken = HtmlToken;
constants_1.classes['HtmlToken'] = __filename;
