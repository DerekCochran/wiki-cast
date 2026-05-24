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
exports.TagPairToken = void 0;
const gapped_1 = require("../../mixin/gapped");
const noEscape_1 = require("../../mixin/noEscape");
const index_1 = require("../index");
/* NOT FOR BROWSER */
const constants_1 = require("../../util/constants");
const debug_1 = require("../../util/debug");
const fixed_1 = require("../../mixin/fixed");
const index_2 = __importDefault(require("../../index"));
/* NOT FOR BROWSER END */
/**
 * Paired tags
 *
 * 成对标签
 */
let TagPairToken = (() => {
    let _classDecorators = [fixed_1.fixedToken, (0, gapped_1.gapped)(), noEscape_1.noEscape];
    let _classDescriptor;
    let _classExtraInitializers = [];
    let _classThis;
    let _classSuper = index_1.Token;
    var TagPairToken = class extends _classSuper {
        static { _classThis = this; }
        static {
            const _metadata = typeof Symbol === "function" && Symbol.metadata ? Object.create(_classSuper[Symbol.metadata] ?? null) : void 0;
            __esDecorate(null, _classDescriptor = { value: _classThis }, _classDecorators, { kind: "class", name: _classThis.name, metadata: _metadata }, null, _classExtraInitializers);
            TagPairToken = _classThis = _classDescriptor.value;
            if (_metadata) Object.defineProperty(_classThis, Symbol.metadata, { enumerable: true, configurable: true, writable: true, value: _metadata });
            __runInitializers(_classThis, _classExtraInitializers);
        }
        #tags;
        #selfClosing;
        closed;
        /** inner wikitext / 内部wikitext */
        get innerText() {
            return this.#selfClosing ? undefined : this.lastChild.text();
        }
        /** whether to be self-closing / 是否自封闭 */
        get selfClosing() {
            return this.#selfClosing;
        }
        /* NOT FOR BROWSER */
        set selfClosing(value) {
            this.#selfClosing = Boolean(value);
            if (value) {
                const { lastChild } = this;
                if (lastChild.type === 'text') {
                    lastChild.replaceData('');
                }
                else {
                    lastChild.replaceChildren();
                }
            }
        }
        /* NOT FOR BROWSER END */
        /**
         * @param name 标签名
         * @param attr 标签属性
         * @param inner 内部wikitext
         * @param closed 是否封闭；约定`undefined`表示自封闭，`''`表示未闭合
         */
        constructor(name, attr, inner, closed, config, accum = []) {
            super(undefined, config);
            this.setAttribute('name', name.toLowerCase());
            this.#tags = [name, closed || name];
            this.closed = closed !== '';
            this.#selfClosing = closed === undefined;
            this.append(attr, inner);
            const index = typeof attr === 'string' ? -1 : accum.indexOf(attr);
            accum.splice(index === -1 ? Infinity : index, 0, this);
        }
        /** @private */
        toString(skip) {
            const { firstChild, lastChild, 
            /* NOT FOR BROWSER */
            nextSibling, name, closed, type, } = this, [opening, closing] = this.#tags;
            /* NOT FOR BROWSER */
            /* c8 ignore start */
            if (!closed && nextSibling && type === 'include') {
                index_2.default.error(`Auto-closing <${name}>`, lastChild);
                this.closed = true;
            }
            /* c8 ignore stop */
            /* NOT FOR BROWSER END */
            return this.#selfClosing
                ? `<${opening}${firstChild.toString(skip)}/>`
                : `<${opening}${firstChild.toString(skip)}>${lastChild.toString(skip)}${this.closed ? `</${closing}>` : ''}`;
        }
        /** @private */
        text() {
            const [opening, closing] = this.#tags;
            return this.#selfClosing
                ? `<${opening}${this.firstChild.text()}/>`
                : `<${opening}${super.text('>')}${this.closed ? `</${closing}>` : ''}`;
        }
        /** @private */
        getAttribute(key) {
            /* NOT FOR BROWSER */
            if (key === 'tags') {
                return this.#tags;
            }
            /* NOT FOR BROWSER END */
            return key === 'padding' ? this.#tags[0].length + 1 : super.getAttribute(key);
        }
        /** @private */
        print() {
            PRINT: {
                const [opening, closing] = this.#tags;
                return super.print(this.#selfClosing
                    ? { pre: `&lt;${opening}`, post: '/&gt;' }
                    : { pre: `&lt;${opening}`, sep: '&gt;', post: this.closed ? `&lt;/${closing}&gt;` : '' });
            }
        }
        /** @private */
        json(_, depth, start = this.getAbsoluteIndex()) {
            LSP: {
                const json = super.json(undefined, depth, start);
                json['selfClosing'] = this.#selfClosing;
                return json;
            }
        }
        /* NOT FOR BROWSER */
        /** @private */
        afterBuild() {
            super.afterBuild();
            const /** @implements */ tagPairListener = (e, data) => {
                /* c8 ignore start */
                if (this.#selfClosing && e.prevTarget === this.lastChild && this.lastChild.toString()) {
                    (0, debug_1.undo)(e, data);
                    throw new Error('A self-closing tag does not have inner content.');
                }
                /* c8 ignore stop */
            };
            this.addEventListener(['insert', 'replace', 'text'], tagPairListener);
        }
    };
    return TagPairToken = _classThis;
})();
exports.TagPairToken = TagPairToken;
constants_1.classes['TagPairToken'] = __filename;
