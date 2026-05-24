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
Object.defineProperty(exports, "__esModule", { value: true });
exports.ConverterToken = void 0;
const string_1 = require("../util/string");
const padded_1 = require("../mixin/padded");
const noEscape_1 = require("../mixin/noEscape");
const index_1 = require("./index");
const converterFlags_1 = require("./converterFlags");
const converterRule_1 = require("./converterRule");
/* NOT FOR BROWSER */
const html_1 = require("../util/html");
const debug_1 = require("../util/debug");
const constants_1 = require("../util/constants");
const cached_1 = require("../mixin/cached");
/* NOT FOR BROWSER END */
/**
 * language conversion
 *
 * 转换
 * @classdesc `{childNodes: [ConverterFlagsToken, ...ConverterRuleToken[]]}`
 */
let ConverterToken = (() => {
    let _classDecorators = [noEscape_1.noEscape, (0, padded_1.padded)('-{')];
    let _classDescriptor;
    let _classExtraInitializers = [];
    let _classThis;
    let _classSuper = index_1.Token;
    let _instanceExtraInitializers = [];
    let _toHtmlInternal_decorators;
    var ConverterToken = class extends _classSuper {
        static { _classThis = this; }
        static {
            const _metadata = typeof Symbol === "function" && Symbol.metadata ? Object.create(_classSuper[Symbol.metadata] ?? null) : void 0;
            _toHtmlInternal_decorators = [(0, cached_1.cached)()];
            __esDecorate(this, null, _toHtmlInternal_decorators, { kind: "method", name: "toHtmlInternal", static: false, private: false, access: { has: obj => "toHtmlInternal" in obj, get: obj => obj.toHtmlInternal }, metadata: _metadata }, null, _instanceExtraInitializers);
            __esDecorate(null, _classDescriptor = { value: _classThis }, _classDecorators, { kind: "class", name: _classThis.name, metadata: _metadata }, null, _classExtraInitializers);
            ConverterToken = _classThis = _classDescriptor.value;
            if (_metadata) Object.defineProperty(_classThis, Symbol.metadata, { enumerable: true, configurable: true, writable: true, value: _metadata });
            __runInitializers(_classThis, _classExtraInitializers);
        }
        /* NOT FOR BROWSER END */
        get type() {
            return 'converter';
        }
        /* NOT FOR BROWSER */
        /** whether to prevent language conversion / 是否不转换 */
        get noConvert() {
            return this.hasFlag('R') || this.length === 2 && this.lastChild.length === 1;
        }
        /** all language conversion flags / 所有转换类型标记 */
        get flags() {
            return this.firstChild.flags;
        }
        set flags(value) {
            this.firstChild.flags = value;
        }
        /* NOT FOR BROWSER END */
        /**
         * @param flags 转换类型标记
         * @param rules 转换规则
         */
        constructor(flags, rules, config, accum = []) {
            super(undefined, config, accum);
            __runInitializers(this, _instanceExtraInitializers);
            // @ts-expect-error abstract class
            this.insertAt(new converterFlags_1.ConverterFlagsToken(flags, config, accum));
            const [firstRule] = rules, hasColon = firstRule.includes(':'), 
            // @ts-expect-error abstract class
            firstRuleToken = new converterRule_1.ConverterRuleToken(firstRule, hasColon, config, accum);
            if (hasColon ? firstRuleToken.length === 1 : rules.length === 2 && !(0, string_1.removeComment)(rules[1]).trim()) {
                this.insertAt(
                // @ts-expect-error abstract class
                new converterRule_1.ConverterRuleToken(rules.join(';'), false, config, accum));
            }
            else {
                this.safeAppend([
                    firstRuleToken,
                    ...rules.slice(1)
                        // @ts-expect-error abstract class
                        .map((rule) => new converterRule_1.ConverterRuleToken(rule, true, config, accum)),
                ]);
            }
            /* NOT FOR BROWSER */
            this.protectChildren(0);
        }
        /** @private */
        toString(skip) {
            const [flags, ...rules] = this.childNodes;
            return `-{${flags.toString(skip)}${flags.length > 0 ? '|' : ''}${rules.map(rule => rule.toString(skip)).join(';')}}-`;
        }
        /** @private */
        text() {
            const [flags, ...rules] = this.childNodes;
            return `-{${flags.text()}|${(0, string_1.text)(rules, ';')}}-`;
        }
        /** @private */
        getGaps(i) {
            return i || this.firstChild.length > 0 ? 1 : 0;
        }
        /** @private */
        print() {
            PRINT: {
                const [flags, ...rules] = this.childNodes;
                return `<span class="wpb-converter">-{${flags.print()}${flags.length > 0 ? '|' : ''}${(0, string_1.print)(rules, { sep: ';' })}}-</span>`;
            }
        }
        /* NOT FOR BROWSER */
        cloneNode() {
            const [flags, ...rules] = this.cloneChildNodes();
            return debug_1.Shadow.run(() => {
                // @ts-expect-error abstract class
                const token = new ConverterToken([], [''], this.getAttribute('config'));
                token.firstChild.safeReplaceWith(flags);
                token.removeAt(1);
                token.safeAppend(rules);
                return token;
            });
        }
        /** @private */
        toHtmlInternal(opt) {
            const flags = this.getEffectiveFlags(), nocc = opt?.nocc, [, ...rules] = this.childNodes;
            if (nocc || flags.has('R') || this.getVariantFlags().size > 0) {
                return (nocc ? '-{' : '') + (0, html_1.html)(rules, ';', opt) + (nocc ? '}-' : '');
            }
            else if (flags.has('S')) {
                return rules.find(({ variant }) => variant)?.lastChild.toHtmlInternal(opt).trim()
                    ?? rules[0].lastChild.toHtmlInternal(opt);
            }
            return '';
        }
        /**
         * Get all language conversion flags
         *
         * 获取所有转换类型标记
         */
        getAllFlags() {
            return this.firstChild.getAllFlags();
        }
        /**
         * Get effective language conversion flags
         *
         * 获取有效的转换类型标记
         */
        getEffectiveFlags() {
            return this.firstChild.getEffectiveFlags();
        }
        /**
         * Get unknown language conversion flags
         *
         * 获取未知的转换类型标记
         */
        getUnknownFlags() {
            return this.firstChild.getUnknownFlags();
        }
        /**
         * Get language coversion flags that specify a language variant
         *
         * 获取指定语言变体的转换标记
         */
        getVariantFlags() {
            return this.firstChild.getVariantFlags();
        }
        /**
         * Check if a language conversion flag is present
         *
         * 是否具有某转换类型标记
         * @param flag language conversion flag / 转换类型标记
         */
        hasFlag(flag) {
            return this.firstChild.hasFlag(flag);
        }
        /**
         * Check if an effective language conversion flag is present
         *
         * 是否具有某有效的转换类型标记
         * @param flag language conversion flag / 转换类型标记
         */
        hasEffectiveFlag(flag) {
            return this.firstChild.hasEffectiveFlag(flag);
        }
        /**
         * Remove a language conversion flag
         *
         * 移除某转换类型标记
         * @param flag language conversion flag / 转换类型标记
         */
        removeFlag(flag) {
            this.firstChild.removeFlag(flag);
        }
        /**
         * Set a language conversion flag
         *
         * 设置转换类型标记
         * @param flag language conversion flag / 转换类型标记
         */
        setFlag(flag) {
            this.firstChild.setFlag(flag);
        }
        /**
         * Toggle a language conversion flag
         *
         * 开关转换类型标记
         * @param flag language conversion flag / 转换类型标记
         */
        toggleFlag(flag) {
            this.firstChild.toggleFlag(flag);
        }
    };
    return ConverterToken = _classThis;
})();
exports.ConverterToken = ConverterToken;
constants_1.classes['ConverterToken'] = __filename;
