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
exports.QuoteToken = void 0;
const lint_1 = require("../../util/lint");
const rect_1 = require("../../lib/rect");
const index_1 = __importDefault(require("../../index"));
const base_1 = require("./base");
/* NOT FOR BROWSER */
const constants_1 = require("../../util/constants");
const debug_1 = require("../../util/debug");
const syntax_1 = require("../../mixin/syntax");
const cached_1 = require("../../mixin/cached");
/* NOT FOR BROWSER END */
/**
 * `''` and `'''`
 *
 * `''`和`'''`
 */
let QuoteToken = (() => {
    let _classDecorators = [(0, syntax_1.syntax)(/^(?:'{5}|'{2,3})$/u)];
    let _classDescriptor;
    let _classExtraInitializers = [];
    let _classThis;
    let _classSuper = base_1.NowikiBaseToken;
    let _instanceExtraInitializers = [];
    let _toHtmlInternal_decorators;
    var QuoteToken = class extends _classSuper {
        static { _classThis = this; }
        static {
            const _metadata = typeof Symbol === "function" && Symbol.metadata ? Object.create(_classSuper[Symbol.metadata] ?? null) : void 0;
            _toHtmlInternal_decorators = [(0, cached_1.cached)()];
            __esDecorate(this, null, _toHtmlInternal_decorators, { kind: "method", name: "toHtmlInternal", static: false, private: false, access: { has: obj => "toHtmlInternal" in obj, get: obj => obj.toHtmlInternal }, metadata: _metadata }, null, _instanceExtraInitializers);
            __esDecorate(null, _classDescriptor = { value: _classThis }, _classDecorators, { kind: "class", name: _classThis.name, metadata: _metadata }, null, _classExtraInitializers);
            QuoteToken = _classThis = _classDescriptor.value;
            if (_metadata) Object.defineProperty(_classThis, Symbol.metadata, { enumerable: true, configurable: true, writable: true, value: _metadata });
            __runInitializers(_classThis, _classExtraInitializers);
        }
        #closing = __runInitializers(this, _instanceExtraInitializers);
        /* NOT FOR BROWSER */
        #match = {};
        /* NOT FOR BROWSER END */
        get type() {
            return 'quote';
        }
        /** @private */
        get bold() {
            return this.innerText.length !== 2;
        }
        /** @private */
        get italic() {
            return this.innerText.length !== 3;
        }
        /**
         * whether to be closing quotes
         *
         * 是否闭合
         * @since v1.16.5
         */
        get closing() {
            LINT: return {
                ...this.bold ? { bold: this.#closing.bold } : undefined,
                ...this.italic ? { italic: this.#closing.italic } : undefined,
            };
        }
        /* NOT FOR BROWSER */
        /** @private */
        get font() {
            return { bold: this.bold, italic: this.italic };
        }
        /* NOT FOR BROWSER END */
        /** @param closing 是否闭合 */
        constructor(wikitext, closing, config, accum) {
            super(wikitext, config, accum);
            this.#closing = closing;
        }
        /** @private */
        text() {
            const { parentNode, innerText } = this;
            return parentNode?.is('image-parameter') && parentNode.name !== 'caption'
                ? ''
                : innerText;
        }
        /** @private */
        lint(start = this.getAbsoluteIndex()) {
            LINT: {
                const { previousSibling, bold, closing } = this, previousData = previousSibling?.type === 'text' ? previousSibling.data : undefined, errors = [], rect = new rect_1.BoundingRect(this, start), rules = ['lonely-apos', 'bold-header'], { lintConfig } = index_1.default, { computeEditInfo } = lintConfig, severities = [undefined, 'word'].map(key => lintConfig.getSeverity(rules[0], key)), s = lintConfig.getSeverity(rules[1]);
                if (previousData?.endsWith(`'`)) {
                    const severity = severities[(closing.bold || closing.italic) && /[a-z\d]'$/iu.test(previousData) ? 1 : 0];
                    if (severity) {
                        const e = (0, lint_1.generateForSelf)(this, rect, rules[0], index_1.default.msg('lonely', `'`), severity), { startLine: endLine, startCol: endCol } = e, [, { length }] = /(?:^|[^'])('+)$/u.exec(previousData), startIndex = start - length, eNew = {
                            ...e,
                            startIndex,
                            endIndex: start,
                            endLine,
                            startCol: endCol - length,
                            endCol,
                        };
                        if (computeEditInfo && bold) {
                            eNew.suggestions = [
                                (0, lint_1.fixByEscape)(startIndex, '&apos;', length),
                                (0, lint_1.fixByRemove)(eNew),
                            ];
                        }
                        errors.push(eNew);
                    }
                }
                if (s && bold && this.isInside('heading-title')) {
                    const e = (0, lint_1.generateForSelf)(this, rect, rules[1], 'bold-in-header', s);
                    if (computeEditInfo) {
                        e.suggestions = [(0, lint_1.fixByRemove)(e)];
                    }
                    errors.push(e);
                }
                return errors;
            }
        }
        /** @private */
        json(_, depth, start = this.getAbsoluteIndex()) {
            LSP: {
                const json = super.json(undefined, depth, start);
                Object.assign(json, { bold: this.bold, italic: this.italic });
                return json;
            }
        }
        /* NOT FOR BROWSER */
        cloneNode() {
            return debug_1.Shadow.run(
            // @ts-expect-error abstract class
            () => new QuoteToken(this.innerText, this.#closing, this.getAttribute('config')));
        }
        /** @private */
        setAttribute(key, value) {
            if (key === 'bold') {
                this.#match.bold = value;
            }
            else if (key === 'italic') {
                this.#match.italic = value;
            }
            else {
                super.setAttribute(key, value);
            }
        }
        /** @private */
        toHtmlInternal() {
            const { bold, italic } = this.closing;
            return (bold ? '</b>' : '') + (italic ? '</i>' : '')
                + (italic === false ? '<i>' : '') + (bold === false ? '<b>' : '');
        }
        /**
         * Find the matching apostrophes
         *
         * 搜索匹配的直引号
         * @since v1.30.0
         * @param type type of apostrophes to match / 匹配的直引号类型
         * @throws `RangeError` ambiguous or wrong apostrophe type
         */
        findMatchingQuote(type) {
            if (type) {
                // eslint-disable-next-line @typescript-eslint/no-unnecessary-condition
                if (type !== 'bold' && type !== 'italic') {
                    this.typeError('findMatchingQuote', "'bold'", "'italic'");
                }
                else if (!this[type]) {
                    throw new RangeError(`Not ${type} apostrophes!`);
                }
            }
            else {
                const { bold, italic } = this;
                if (bold && italic) {
                    throw new RangeError('Ambiguous apostrophe type to match!');
                }
                type = bold ? 'bold' : 'italic';
            }
            return this.#match[type];
        }
        /**
         * Try to get the range of bold/italic text
         *
         * 尝试获取粗体/斜体文本范围
         * @param type type of apostrophes / 直引号类型
         */
        getRange(type) {
            const matched = this.findMatchingQuote(type), { parentNode } = this;
            if (matched && parentNode && matched.parentNode === parentNode) {
                const range = this.createRange(), { childNodes } = parentNode, i = childNodes.indexOf(this), j = childNodes.indexOf(matched);
                range.setStart(parentNode, Math.min(i, j) + 1);
                range.setEnd(parentNode, Math.max(i, j));
                return range;
            }
            return undefined;
        }
    };
    return QuoteToken = _classThis;
})();
exports.QuoteToken = QuoteToken;
constants_1.classes['QuoteToken'] = __filename;
