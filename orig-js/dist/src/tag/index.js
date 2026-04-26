"use strict";
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
var __runInitializers = (this && this.__runInitializers) || function (thisArg, initializers, value) {
    var useValue = arguments.length > 2;
    for (var i = 0; i < initializers.length; i++) {
        value = useValue ? initializers[i].call(thisArg, value) : initializers[i].call(thisArg);
    }
    return useValue ? value : void 0;
};
var __importDefault = (this && this.__importDefault) || function (mod) {
    return (mod && mod.__esModule) ? mod : { "default": mod };
};
Object.defineProperty(exports, "__esModule", { value: true });
exports.TagToken = void 0;
const lint_1 = require("../../util/lint");
const debug_1 = require("../../util/debug");
const index_1 = require("../index");
/* PRINT ONLY */
const index_2 = __importDefault(require("../../index"));
/* PRINT ONLY END */
/* NOT FOR BROWSER */
const constants_1 = require("../../util/constants");
const fixed_1 = require("../../mixin/fixed");
/* NOT FOR BROWSER END */
/**
 * HTML tag
 *
 * HTML标签
 * @classdesc `{childNodes: [AttributesToken]}`
 */
let TagToken = (() => {
    let _classDecorators = [fixed_1.fixedToken];
    let _classDescriptor;
    let _classExtraInitializers = [];
    let _classThis;
    let _classSuper = index_1.Token;
    var TagToken = class extends _classSuper {
        static { _classThis = this; }
        static {
            const _metadata = typeof Symbol === "function" && Symbol.metadata ? Object.create(_classSuper[Symbol.metadata] ?? null) : void 0;
            __esDecorate(null, _classDescriptor = { value: _classThis }, _classDecorators, { kind: "class", name: _classThis.name, metadata: _metadata }, null, _classExtraInitializers);
            TagToken = _classThis = _classDescriptor.value;
            if (_metadata) Object.defineProperty(_classThis, Symbol.metadata, { enumerable: true, configurable: true, writable: true, value: _metadata });
            __runInitializers(_classThis, _classExtraInitializers);
        }
        #closing;
        #tag;
        #match;
        /* NOT FOR BROWSER END */
        /** whether to be a closing tag / 是否是闭合标签 */
        get closing() {
            return this.#closing;
        }
        /* NOT FOR BROWSER */
        set closing(value) {
            this.#closing = Boolean(value);
        }
        /** @private */
        get tag() {
            return this.#tag;
        }
        /** @private */
        set tag(value) {
            this.#tag = value;
        }
        /* NOT FOR BROWSER END */
        /**
         * @param tag 标签名
         * @param attr 标签属性
         * @param closing 是否闭合
         */
        constructor(tag, attr, closing, config, accum) {
            super(undefined, config, accum);
            this.insertAt(attr);
            this.#closing = closing;
            this.#tag = tag;
        }
        /** @private */
        toString(skip) {
            return `<${this.#closing ? '/' : ''}${this.#tag}${super.toString(skip)}${this.selfClosing ? '/' : ''}>`;
        }
        /** @private */
        text(separator = '') {
            const { closing } = this;
            return `<${closing && !separator ? '/' : ''}${this.#tag}${closing ? '' : super.text()}${separator}>`;
        }
        /** @private */
        getAttribute(key) {
            return key === 'padding'
                ? this.#tag.length + (this.#closing ? 2 : 1)
                : super.getAttribute(key);
        }
        /**
         * Find the matching tag
         *
         * 搜索匹配的标签
         */
        findMatchingTag() {
            LINT: return (0, lint_1.cache)(this.#match, () => {
                const { type, name, parentNode, closing, selfClosing, 
                /* NOT FOR BROWSER */
                legacy, } = this;
                let isVoid = false, isFlexible = false;
                if (type === 'html') {
                    const [, flexibleTags, voidTags] = this.getAttribute('config').html;
                    isVoid = voidTags.includes(name);
                    isFlexible = flexibleTags.includes(name);
                }
                if (isVoid || isFlexible && selfClosing) { // 自封闭标签
                    return this;
                }
                /* c8 ignore next 3 */
                if (!parentNode) {
                    return undefined;
                }
                const { childNodes } = parentNode, i = childNodes.indexOf(this), siblings = closing ? childNodes.slice(0, i).reverse() : childNodes.slice(i + 1), stack = [this], { rev } = debug_1.Shadow;
                for (const token of siblings) {
                    if (!token.is(type)
                        || type === 'html' && (token.name !== name || isFlexible && token.selfClosing)
                        || type === 'tvar' && token.legacy !== legacy) {
                        continue;
                    }
                    else if (token.#closing === closing) {
                        /* c8 ignore next 3 */
                        if (type === 'tvar') {
                            return undefined;
                        }
                        stack.push(token);
                    }
                    else {
                        const top = stack.pop();
                        if (top === this) {
                            return token;
                        }
                        if (index_2.default.viewOnly) {
                            top.#match = [rev, token];
                            token.#match = [rev, top];
                        }
                    }
                }
                if (index_2.default.viewOnly) {
                    for (const token of stack) {
                        token.#match = [rev, undefined];
                    }
                }
                return undefined;
            }, value => {
                this.#match = value;
                if (value[1] && value[1] !== this) {
                    value[1].#match = [debug_1.Shadow.rev, this];
                }
            });
        }
        /** @private */
        print() {
            PRINT: return super.print({
                pre: `&lt;${this.#closing ? '/' : ''}${this.#tag}`,
                post: `${this.selfClosing ? '/' : ''}&gt;`,
            });
        }
        /** @private */
        json(_, depth, start = this.getAbsoluteIndex()) {
            LSP: {
                const json = super.json(undefined, depth, start);
                json['closing'] = this.#closing;
                return json;
            }
        }
        /* NOT FOR BROWSER */
        /**
         * Get the range of the tag pair
         *
         * 获取标签对的范围
         * @since v1.23.0
         */
        getRange() {
            const matched = this.findMatchingTag();
            if (!matched || matched === this) {
                return undefined;
            }
            const { closing } = this, range = this.createRange();
            range.setStartAfter(closing ? matched : this);
            range.setEndBefore(closing ? this : matched);
            return range;
        }
    };
    return TagToken = _classThis;
})();
exports.TagToken = TagToken;
constants_1.classes['TagToken'] = __filename;
