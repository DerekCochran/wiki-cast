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
exports.ConverterRuleToken = void 0;
const constants_1 = require("../util/constants");
const noEscape_1 = require("../mixin/noEscape");
const index_1 = __importDefault(require("../index"));
const index_2 = require("./index");
const atom_1 = require("./atom");
/* NOT FOR BROWSER */
const debug_1 = require("../util/debug");
const html_1 = require("../util/html");
const cached_1 = require("../mixin/cached");
/* NOT FOR BROWSER END */
/**
 * 生成转换原文或目标的节点
 * @param text 文本
 * @param type 节点类型
 * @param config
 * @param accum
 */
const getRuleFromTo = (text, type, config, accum) => {
    const token = new index_2.Token(text, config, accum);
    token.type = `converter-rule-${type}`;
    token.setAttribute('stage', constants_1.MAX_STAGE);
    return token;
};
/**
 * language conversion rule
 *
 * 转换规则
 * @classdesc `{childNodes: [Token?, AtomToken?, Token]}`
 */
let ConverterRuleToken = (() => {
    let _classDecorators = [noEscape_1.noEscape];
    let _classDescriptor;
    let _classExtraInitializers = [];
    let _classThis;
    let _classSuper = index_2.Token;
    let _instanceExtraInitializers = [];
    let _toHtmlInternal_decorators;
    var ConverterRuleToken = class extends _classSuper {
        static { _classThis = this; }
        static {
            const _metadata = typeof Symbol === "function" && Symbol.metadata ? Object.create(_classSuper[Symbol.metadata] ?? null) : void 0;
            _toHtmlInternal_decorators = [(0, cached_1.cached)()];
            __esDecorate(this, null, _toHtmlInternal_decorators, { kind: "method", name: "toHtmlInternal", static: false, private: false, access: { has: obj => "toHtmlInternal" in obj, get: obj => obj.toHtmlInternal }, metadata: _metadata }, null, _instanceExtraInitializers);
            __esDecorate(null, _classDescriptor = { value: _classThis }, _classDecorators, { kind: "class", name: _classThis.name, metadata: _metadata }, null, _classExtraInitializers);
            ConverterRuleToken = _classThis = _classDescriptor.value;
            if (_metadata) Object.defineProperty(_classThis, Symbol.metadata, { enumerable: true, configurable: true, writable: true, value: _metadata });
            __runInitializers(_classThis, _classExtraInitializers);
        }
        /* NOT FOR BROWSER END */
        get type() {
            return 'converter-rule';
        }
        /* PRINT ONLY */
        /** language variant / 语言变体 */
        get variant() {
            LSP: return this.childNodes[this.length - 2]?.text().trim().toLowerCase() ?? '';
        }
        /* PRINT ONLY END */
        /* NOT FOR BROWSER */
        set variant(variant) {
            this.setVariant(variant);
        }
        /** whether to be unidirectional conversion / 是否是单向转换 */
        get unidirectional() {
            return this.length === 3;
        }
        /** @throws `Error` 不能用于将双向转换或不转换更改为单向转换 */
        set unidirectional(flag) {
            const { length } = this;
            if (length === 3 && !flag) {
                this.makeBidirectional();
            }
            else if (length === 2 && flag) {
                throw new Error('If you want to change to unidirectional, '
                    + 'please use ConverterRuleToken.makeUnidirectional method!');
            }
            else if (length === 1 && flag) {
                throw new Error('If you want to change to unidirectional, '
                    + 'please use ConverterRuleToken.setVariant method to specify the language variant first!');
            }
        }
        /** whether to be bidirectional conversion / 是否是双向转换 */
        get bidirectional() {
            return this.length === 2;
        }
        /** @throws `Error` 不能用于将双向转换更改为单向转换或将不转换更改为双向转换 */
        set bidirectional(flag) {
            const { length } = this;
            if (length === 3 && flag) {
                this.makeBidirectional();
            }
            else if (length === 2 && !flag) {
                throw new Error('If you want to change to unidirectional, '
                    + 'please use ConverterRuleToken.makeUnidirectional method!');
            }
            else if (length === 1 && flag) {
                throw new Error('If you want to change to bidirectional, '
                    + 'please use ConverterRuleToken.setVariant method!');
            }
        }
        /* NOT FOR BROWSER END */
        /**
         * @param rule 转换规则
         * @param hasColon 是否带有":"
         */
        constructor(rule, hasColon = true, config = index_1.default.getConfig(), accum = []) {
            super(undefined, config, accum, {
                Token: '0:3', AtomToken: '0:2',
            });
            __runInitializers(this, _instanceExtraInitializers);
            const i = rule.indexOf(':'), j = rule.slice(0, i).indexOf('=>'), v = j === -1 ? rule.slice(0, i) : rule.slice(j + 2, i);
            if (hasColon && config.variants.includes(v.trim().toLowerCase())) {
                super.insertAt(new atom_1.AtomToken(v, 'converter-rule-variant', config, accum));
                super.insertAt(getRuleFromTo(rule.slice(i + 1), 'to', config, accum));
                if (j !== -1) {
                    super.insertAt(getRuleFromTo(rule.slice(0, j), 'from', config, accum), 0);
                }
            }
            else {
                super.insertAt(getRuleFromTo(rule, 'to', config, accum));
            }
            /* NOT FOR BROWSER */
            this.protectChildren('1:');
        }
        /** @private */
        toString(skip) {
            const { childNodes, firstChild, lastChild } = this;
            return childNodes.length === 3
                ? `${firstChild.toString(skip)}=>${childNodes[1].toString(skip)}:${lastChild.toString(skip)}`
                : super.toString(skip, ':');
        }
        /** @private */
        text() {
            const { childNodes, firstChild, lastChild } = this;
            return childNodes.length === 3
                ? `${firstChild.text()}=>${childNodes[1].text()}:${lastChild.text()}`
                : super.text(':');
        }
        /** @private */
        getGaps(i) {
            return i === 0 && this.length === 3 ? 2 : 1;
        }
        /** @private */
        print() {
            PRINT: {
                const { childNodes } = this;
                if (childNodes.length === 3) {
                    const [from, variant, to] = childNodes;
                    return `<span class="wpb-converter-rule">${from.print()}=>${variant.print()}:${to.print()}</span>`;
                }
                return super.print({ sep: ':' });
            }
        }
        /** @private */
        json(_, depth, start = this.getAbsoluteIndex()) {
            LSP: {
                const json = super.json(undefined, depth, start);
                json['variant'] = this.variant;
                return json;
            }
        }
        /* NOT FOR BROWSER */
        cloneNode() {
            const cloned = this.cloneChildNodes(), placeholders = ['', 'zh:', '=>zh:'], placeholder = placeholders[cloned.length - 1];
            return debug_1.Shadow.run(() => {
                // @ts-expect-error abstract class
                const token = new ConverterRuleToken(placeholder, Boolean(placeholder), this.getAttribute('config'));
                for (let i = 0; i < cloned.length; i++) {
                    token.childNodes[i].safeReplaceWith(cloned[i]);
                }
                return token;
            });
        }
        /** @private */
        afterBuild() {
            super.afterBuild();
            if (!index_1.default.internal) {
                const /** @implements */ converterRuleListener = (e, data) => {
                    const { prevTarget } = e;
                    if (this.length > 1 && this.childNodes[this.length - 2] === prevTarget) {
                        const { variant } = this;
                        if (!this.getAttribute('config').variants.includes(variant)) {
                            (0, debug_1.undo)(e, data);
                            throw new Error(`Invalid language variant: ${variant}`);
                        }
                    }
                };
                this.addEventListener(['remove', 'insert', 'text', 'replace'], converterRuleListener);
            }
        }
        /**
         * @override
         * @param i position of the child node / 移除位置
         */
        removeAt(i) {
            if (this.length === 1) {
                this.constructorError('needs at least 1 child node');
            }
            return super.removeAt(i);
        }
        insertAt() {
            this.constructorError('has complex syntax. Do not try to insert child nodes manually');
        }
        /**
         * Prevent language conversion
         *
         * 修改为不转换
         */
        noConvert() {
            const { length } = this;
            for (let i = 0; i < length - 1; i++) { // ConverterRuleToken只能从前往后删除子节点
                this.removeAt(0);
            }
        }
        /**
         * Set the target of language conversion
         *
         * 设置转换目标
         * @param to target of language conversion / 转换目标
         */
        setTo(to) {
            const { childNodes } = index_1.default.parseWithRef(to, this);
            this.lastChild.safeReplaceChildren(childNodes);
        }
        /**
         * Set the language variant
         *
         * 设置语言变体
         * @param variant language variant / 语言变体
         */
        setVariant(variant) {
            if (this.length === 1) {
                super.insertAt(debug_1.Shadow.run(() => new atom_1.AtomToken(variant, 'converter-rule-variant', this.getAttribute('config'))), 0);
            }
            else {
                this.childNodes[this.length - 2].setText(variant);
            }
        }
        /**
         * Set the source of language conversion
         *
         * 设置转换原文
         * @param from source of language conversion / 转换原文
         * @throws `Error` 尚未指定语言变体
         */
        setFrom(from) {
            const { variant, unidirectional } = this;
            if (!variant) {
                throw new Error('Please specify the language variant first!');
            }
            const { childNodes } = index_1.default.parseWithRef(from, this);
            if (!unidirectional) {
                super.insertAt(debug_1.Shadow.run(() => getRuleFromTo(undefined, 'from', this.getAttribute('config'))), 0);
            }
            this.firstChild.safeReplaceChildren(childNodes);
        }
        /**
         * Make the language conversion unidirectional
         *
         * 修改为单向转换
         * @param from source of language conversion / 转换原文
         */
        makeUnidirectional(from) {
            this.setFrom(from);
        }
        /**
         * Make the language conversion bidirectional
         *
         * 修改为双向转换
         */
        makeBidirectional() {
            if (this.unidirectional) {
                this.removeAt(0);
            }
        }
        /** @private */
        toHtmlInternal(opt) {
            const { childNodes, firstChild, lastChild } = this;
            return childNodes.length === 3
                ? `${firstChild.toHtmlInternal(opt)}=>${childNodes[1].text()}:${lastChild.toHtmlInternal(opt)}`
                : (0, html_1.html)(childNodes, ':', opt);
        }
    };
    return ConverterRuleToken = _classThis;
})();
exports.ConverterRuleToken = ConverterRuleToken;
constants_1.classes['ConverterRuleToken'] = __filename;
