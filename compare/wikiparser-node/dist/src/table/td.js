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
exports.createTd = exports.TdToken = void 0;
const lint_1 = require("../../util/lint");
const constants_1 = require("../../util/constants");
const rect_1 = require("../../lib/rect");
const cached_1 = require("../../mixin/cached");
const index_1 = __importDefault(require("../../index"));
const index_2 = require("../index");
const base_1 = require("./base");
/* NOT FOR BROWSER */
const debug_1 = require("../../util/debug");
const string_1 = require("../../util/string");
const fixed_1 = require("../../mixin/fixed");
/* NOT FOR BROWSER END */
/**
 * `<td>`, `<th>` or `<caption>`
 *
 * `<td>`、`<th>`或`<caption>`
 * @classdesc `{childNodes: [SyntaxToken, AttributesToken, Token]}`
 */
let TdToken = (() => {
    let _classDecorators = [fixed_1.fixedToken];
    let _classDescriptor;
    let _classExtraInitializers = [];
    let _classThis;
    let _classSuper = base_1.TableBaseToken;
    let _instanceExtraInitializers = [];
    let _private_getSyntax_decorators;
    let _private_getSyntax_descriptor;
    let _toHtmlInternal_decorators;
    var TdToken = class extends _classSuper {
        static { _classThis = this; }
        static {
            const _metadata = typeof Symbol === "function" && Symbol.metadata ? Object.create(_classSuper[Symbol.metadata] ?? null) : void 0;
            _private_getSyntax_decorators = [(0, cached_1.cached)(false)];
            _toHtmlInternal_decorators = [(0, cached_1.cached)()];
            __esDecorate(this, _private_getSyntax_descriptor = { value: __setFunctionName(function () {
                    const syntax = this.firstChild.text(), 
                    /* NOT FOR BROWSER */
                    esc = syntax.includes('{{'), 
                    /* NOT FOR BROWSER END */
                    char = syntax.slice(-1);
                    let subtype = 'td';
                    if (char === '!') {
                        subtype = 'th';
                    }
                    else if (char === '+') {
                        subtype = 'caption';
                    }
                    if (this.isIndependent()) {
                        return {
                            subtype,
                            /* NOT FOR BROWSER */
                            escape: esc,
                            correction: false,
                        };
                    }
                    const { previousSibling } = this;
                    /* NOT FOR BROWSER */
                    if (!(previousSibling instanceof TdToken)) {
                        return { subtype, escape: esc, correction: true };
                    }
                    /* NOT FOR BROWSER END */
                    // eslint-disable-next-line @typescript-eslint/no-unnecessary-type-assertion
                    const result = { ...previousSibling.#getSyntax() };
                    /* NOT FOR BROWSER */
                    const str = previousSibling.lastChild.toString();
                    result.escape ||= esc;
                    result.correction = str.includes('\n') && debug_1.Shadow.run(() => {
                        const token = new index_2.Token(str, this.getAttribute('config'))
                            .parseOnce(0, this.getAttribute('include')).parseOnce().parseOnce();
                        return token.firstChild.toString().includes('\n');
                    }, index_1.default);
                    if (subtype === 'th' && result.subtype !== 'th') {
                        result.subtype = 'th';
                        result.correction = true;
                    }
                    /* NOT FOR BROWSER END */
                    return result;
                }, "#getSyntax") }, _private_getSyntax_decorators, { kind: "method", name: "#getSyntax", static: false, private: true, access: { has: obj => #getSyntax in obj, get: obj => obj.#getSyntax }, metadata: _metadata }, null, _instanceExtraInitializers);
            __esDecorate(this, null, _toHtmlInternal_decorators, { kind: "method", name: "toHtmlInternal", static: false, private: false, access: { has: obj => "toHtmlInternal" in obj, get: obj => obj.toHtmlInternal }, metadata: _metadata }, null, _instanceExtraInitializers);
            __esDecorate(null, _classDescriptor = { value: _classThis }, _classDecorators, { kind: "class", name: _classThis.name, metadata: _metadata }, null, _classExtraInitializers);
            TdToken = _classThis = _classDescriptor.value;
            if (_metadata) Object.defineProperty(_classThis, Symbol.metadata, { enumerable: true, configurable: true, writable: true, value: _metadata });
            __runInitializers(_classThis, _classExtraInitializers);
        }
        #innerSyntax = (__runInitializers(this, _instanceExtraInitializers), '');
        /* NOT FOR BROWSER END */
        get type() {
            return 'td';
        }
        /** rowspan */
        get rowspan() {
            LINT: return this.getAttr('rowspan');
        }
        /** colspan */
        get colspan() {
            LINT: return this.getAttr('colspan');
        }
        /** cell type / 单元格类型 */
        get subtype() {
            return this.#getSyntax().subtype;
        }
        /* NOT FOR BROWSER */
        set subtype(subtype) {
            this.setSyntax(subtype);
        }
        set rowspan(rowspan) {
            this.setAttr('rowspan', rowspan);
        }
        set colspan(colspan) {
            this.setAttr('colspan', colspan);
        }
        /** inner wikitext / 内部wikitext */
        get innerText() {
            return this.lastChild.text().trim();
        }
        set innerText(text) {
            const { childNodes } = index_1.default.parseWithRef(text, this, undefined, true);
            this.lastChild.safeReplaceChildren(childNodes);
        }
        /* NOT FOR BROWSER END */
        /**
         * @param syntax 单元格语法
         * @param inner 内部wikitext
         */
        constructor(syntax, inner, config, accum = []) {
            let innerSyntax = /\||\0\d+!\x7F/u.exec(inner ?? ''), attr = innerSyntax ? inner.slice(0, innerSyntax.index) : '';
            if (/\[\[|-\{/u.test(attr)) {
                innerSyntax = null;
                attr = '';
            }
            super(/^(?:\n[^\S\n]*(?:[|!]|\|\+|\{\{\s*!\s*\}\}\+?)|(?:\||\{\{\s*!\s*\}\}){2}|!!|\{\{\s*!!\s*\}\})$/u, syntax, 'td', attr, config, accum, { SyntaxToken: 0, AttributesToken: 1, Token: 2 });
            if (innerSyntax) {
                [this.#innerSyntax] = innerSyntax;
            }
            const innerToken = new index_2.Token(inner?.slice((innerSyntax?.index ?? NaN) + this.#innerSyntax.length), config, accum);
            innerToken.type = 'td-inner';
            innerToken.setAttribute('stage', 4);
            this.insertAt(innerToken);
        }
        /** 表格语法信息 */
        get #getSyntax() { return _private_getSyntax_descriptor.value; }
        /** @private */
        afterBuild() {
            if (this.#innerSyntax.includes('\0')) {
                this.#innerSyntax = this.buildFromStr(this.#innerSyntax, constants_1.BuildMethod.String);
            }
            super.afterBuild();
        }
        /** @private */
        toString(skip) {
            /* NOT FOR BROWSER */
            this.#correct();
            /* NOT FOR BROWSER END */
            const [syntax, attr, inner] = this.childNodes;
            return syntax.toString(skip) + attr.toString(skip) + this.#innerSyntax + inner.toString(skip);
        }
        /** @private */
        text() {
            /* NOT FOR BROWSER */
            this.#correct();
            /* NOT FOR BROWSER END */
            const [syntax, attr, inner] = this.childNodes;
            return syntax.text() + attr.text() + this.#innerSyntax + inner.text();
        }
        /** @private */
        getGaps(i) {
            if (i === 1) {
                /* NOT FOR BROWSER */
                this.#correct();
                /* NOT FOR BROWSER END */
                return this.#innerSyntax.length;
            }
            return 0;
        }
        /** @private */
        lint(start = this.getAbsoluteIndex(), re) {
            LINT: {
                const errors = super.lint(start, re), rect = new rect_1.BoundingRect(this, start + this.getRelativeIndex(this.length - 1)), rule = 'pipe-like', { lintConfig } = index_1.default, { computeEditInfo, fix } = lintConfig, severities = ['td', 'double'].map(key => lintConfig.getSeverity(rule, key));
                for (const child of this.lastChild.childNodes) {
                    if (child.type === 'text') {
                        const { data } = child;
                        if (data.includes('|')) {
                            const double = data.includes('||'), s = severities[double ? 1 : 0];
                            if (s) {
                                const e = (0, lint_1.generateForChild)(child, rect, rule, 'pipe-in-table', s);
                                if (computeEditInfo || fix) {
                                    if (double) {
                                        const syntax = { caption: '|+', td: '|', th: '!' }[this.subtype];
                                        e.fix = (0, lint_1.fixBy)(e, 'newline', data.replace(/\|\|/gu, `\n${syntax}`));
                                    }
                                    else if (computeEditInfo) {
                                        e.suggestions = [(0, lint_1.fixByPipe)(e.startIndex, data)];
                                    }
                                }
                                errors.push(e);
                            }
                        }
                    }
                }
                return errors;
            }
        }
        /**
         * Check if the cell is at the beginning of a line
         *
         * 是否位于行首
         */
        isIndependent() {
            return this.firstChild.text().startsWith('\n');
        }
        getAttr(key) {
            LINT: {
                /* NOT FOR BROWSER */
                key = (0, string_1.trimLc)(key);
                /* NOT FOR BROWSER END */
                const value = super.getAttr(key);
                return (key === 'rowspan' || key === 'colspan' ? parseInt(value) || 1 : value);
            }
        }
        /** @private */
        escape() {
            LSP: {
                super.escape();
                if (this.childNodes[1].toString()) {
                    this.#innerSyntax ||= '{{!}}';
                }
                if (this.#innerSyntax === '|') {
                    this.#innerSyntax = '{{!}}';
                }
            }
        }
        /** @private */
        print() {
            PRINT: {
                const [syntax, attr, inner] = this.childNodes;
                return `<span class="wpb-td">${syntax.print()}${attr.print()}${this.#innerSyntax}${inner.print()}</span>`;
            }
        }
        /** @private */
        json(_, depth, start = this.getAbsoluteIndex()) {
            LSP: {
                const json = super.json(undefined, depth, start), { rowspan, colspan } = this;
                Object.assign(json, { subtype: this.subtype }, rowspan !== 1 && { rowspan }, colspan !== 1 && { colspan });
                return json;
            }
        }
        /* NOT FOR BROWSER */
        cloneNode() {
            const token = super.cloneNode();
            token.setAttribute('innerSyntax', this.#innerSyntax);
            return token;
        }
        /** @private */
        setAttribute(key, value) {
            if (key === 'innerSyntax') {
                this.#innerSyntax = (value ?? '');
            }
            else {
                super.setAttribute(key, value);
            }
        }
        /** @private */
        setSyntax(subtype, esc) {
            const aliases = { td: '\n|', th: '\n!', caption: '\n|+' }, { firstChild } = this;
            firstChild.replaceChildren(aliases[subtype]);
            if (esc) {
                (0, base_1.escapeTable)(firstChild);
            }
        }
        /** 修复\<td\>语法 */
        #correct() {
            if (this.childNodes[1].toString()) {
                this.#innerSyntax ||= '|';
            }
            const { subtype, escape, correction } = this.#getSyntax();
            if (correction) {
                this.setSyntax(subtype, escape);
            }
        }
        /**
         * Move to a new line
         *
         * 改为独占一行
         */
        independence() {
            if (!this.isIndependent()) {
                const { subtype, escape } = this.#getSyntax();
                this.setSyntax(subtype, escape);
            }
        }
        getAttrs() {
            const attr = super.getAttrs();
            if ('rowspan' in attr) {
                attr.rowspan = parseInt(attr.rowspan);
            }
            if ('colspan' in attr) {
                attr.colspan = parseInt(attr.colspan);
            }
            return attr;
        }
        setAttr(keyOrProp, value) {
            if (typeof keyOrProp !== 'string') {
                for (const key in keyOrProp) {
                    this.setAttr(key, keyOrProp[key]);
                }
                return;
            }
            const key = (0, string_1.trimLc)(keyOrProp);
            let v;
            if (typeof value === 'number' && (key === 'rowspan' || key === 'colspan')) {
                v = value === 1 ? false : String(value);
            }
            else {
                v = String(value);
            }
            super.setAttr(key, v);
            if (!this.childNodes[1].toString()) {
                this.#innerSyntax = '';
            }
        }
        /** @private */
        toHtmlInternal(opt) {
            const { subtype, childNodes: [, attr, inner], nextSibling } = this, notEOL = nextSibling?.toString().startsWith('\n') === false, lf = opt?.nowrap ? ' ' : '\n';
            let html = inner.toHtmlInternal(opt).replace(/^[^\S\n]*/u, '');
            if (notEOL) {
                html = html.replace(/(?<=[\S\n])[^\S\n]*$/u, '');
            }
            return `${lf}<${subtype}${attr.toHtmlInternal()}>${subtype === 'caption' ? (0, string_1.newline)(html) : html + (notEOL ? '' : lf)}</${subtype}>`;
        }
    };
    return TdToken = _classThis;
})();
exports.TdToken = TdToken;
/* NOT FOR BROWSER */
/**
 * 创建新的单元格
 * @param inner 内部wikitext
 * @param ref 参考节点
 * @param subtype 单元格类型
 * @param attr 单元格属性
 */
const createTd = (inner, ref, subtype = 'td', attr = {}) => {
    const innerToken = typeof inner === 'string' ? index_1.default.parseWithRef(inner, ref) : inner, 
    // @ts-expect-error abstract class
    token = debug_1.Shadow.run(() => new TdToken('\n|', undefined, ref.getAttribute('config')));
    token.setSyntax(subtype);
    token.lastChild.safeReplaceWith(innerToken);
    token.setAttr(attr);
    return token;
};
exports.createTd = createTd;
constants_1.classes['TdToken'] = __filename;
