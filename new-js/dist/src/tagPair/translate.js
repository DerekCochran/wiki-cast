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
exports.TranslateToken = void 0;
const index_1 = require("../index");
const index_2 = require("./index");
const syntax_1 = require("../syntax");
const tvar_1 = require("../tag/tvar");
/* NOT FOR BROWSER */
const debug_1 = require("../../util/debug");
const constants_1 = require("../../util/constants");
const string_1 = require("../../util/string");
const cached_1 = require("../../mixin/cached");
/* NOT FOR BROWSER END */
/**
 * `<translate>`
 * @classdesc `{childNodes: [SyntaxToken, Token]}`
 */
let TranslateToken = (() => {
    let _classSuper = index_2.TagPairToken;
    let _instanceExtraInitializers = [];
    let _toHtmlInternal_decorators;
    return class TranslateToken extends _classSuper {
        static {
            const _metadata = typeof Symbol === "function" && Symbol.metadata ? Object.create(_classSuper[Symbol.metadata] ?? null) : void 0;
            _toHtmlInternal_decorators = [(0, cached_1.cached)()];
            __esDecorate(this, null, _toHtmlInternal_decorators, { kind: "method", name: "toHtmlInternal", static: false, private: false, access: { has: obj => "toHtmlInternal" in obj, get: obj => obj.toHtmlInternal }, metadata: _metadata }, null, _instanceExtraInitializers);
            if (_metadata) Object.defineProperty(this, Symbol.metadata, { enumerable: true, configurable: true, writable: true, value: _metadata });
        }
        /* NOT FOR BROWSER END */
        get type() {
            return 'translate';
        }
        get selfClosing() {
            return false;
        }
        /**
         * @param attr 标签属性
         * @param inner 内部wikitext
         */
        constructor(attr, inner, config, accum = []) {
            const attrToken = new syntax_1.SyntaxToken(attr, /^(?: nowrap)?$/u, 'translate-attr', config, accum, { AstText: ':' });
            inner = inner?.replace(/<tvar(\|[^>]+)>([\s\S]*?)<\/>/gu, (_, p1, p2) => {
                // @ts-expect-error abstract class
                new tvar_1.TvarToken('tvar', p1, false, config, accum);
                // @ts-expect-error abstract class
                new tvar_1.TvarToken('', '', true, config, accum);
                return `\0${accum.length - 2}n\x7F${p2}\0${accum.length}n\x7F`;
            }).replace(/<(tvar)(\s+name\s*=(?:\s*(?:(["'])[\s\S]*?\3|[^"'\s>]+))?\s*)>([\s\S]*?)<\/(tvar)(\s*)>/giu, (_, p1, p2, __, p3, p4, p5) => {
                // @ts-expect-error abstract class
                new tvar_1.TvarToken(p1, p2, false, config, accum);
                // @ts-expect-error abstract class
                new tvar_1.TvarToken(p4, p5, true, config, accum);
                return `\0${accum.length - 2}n\x7F${p3}\0${accum.length}n\x7F`;
            });
            const innerToken = new index_1.Token(inner, config, accum);
            innerToken.type = 'translate-inner';
            super('translate', attrToken, innerToken, 'translate', config, accum);
            __runInitializers(this, _instanceExtraInitializers);
            /* PRINT ONLY */
            this.seal('closed', true);
        }
        /** @private */
        toString(skip) {
            return skip ? this.lastChild.toString(true) : super.toString();
        }
        /** @private */
        text() {
            return this.lastChild.text();
        }
        /* PRINT ONLY */
        /** 是否有nowrap属性 */
        #isNowrap() {
            PRINT: return this.firstChild.toString() === ' nowrap';
        }
        /** @private */
        print() {
            PRINT: return `<span class="wpb-ext">&lt;translate${this.#isNowrap()
                ? '<span class="wpb-ext-attrs"> <span class="wpb-ext-attr">'
                    + '<span class="wpb-attr-key">nowrap</span>'
                    + '</span></span>'
                : ''}&gt;${this.lastChild.print({ class: 'ext-inner' })}&lt;/translate&gt;</span>`;
        }
        /* PRINT ONLY END */
        /* NOT FOR BROWSER */
        /**
         * 设置nowrap属性
         * @param nowrap 是否nowrap
         */
        #setNowrap(nowrap) {
            this.firstChild.replaceChildren(nowrap ? ' nowrap' : '');
        }
        /** @implements */
        getAttr(key) {
            return (0, string_1.trimLc)(key) === 'nowrap' && this.#isNowrap() || undefined;
        }
        /** @implements */
        hasAttr(key) {
            return (0, string_1.trimLc)(key) === 'nowrap' && this.#isNowrap();
        }
        /** @implements */
        setAttr(keyOrProp, value) {
            if (typeof keyOrProp === 'object') {
                for (const key in keyOrProp) {
                    this.setAttr(key, keyOrProp[key]);
                }
            }
            else if ((0, string_1.trimLc)(keyOrProp) === 'nowrap') {
                this.#setNowrap(value);
            }
        }
        /** @implements */
        removeAttr(key) {
            if ((0, string_1.trimLc)(key) === 'nowrap') {
                this.firstChild.replaceChildren();
            }
        }
        /** @implements */
        toggleAttr(key, force) {
            if ((0, string_1.trimLc)(key) === 'nowrap') {
                this.#setNowrap(force ?? !this.#isNowrap());
            }
        }
        cloneNode() {
            const inner = this.lastChild.cloneNode();
            return debug_1.Shadow.run(() => {
                // @ts-expect-error abstract class
                const token = new TranslateToken(this.firstChild.toString() || undefined, undefined, this.getAttribute('config'));
                token.lastChild.safeReplaceWith(inner);
                return token;
            });
        }
        /** @private */
        toHtmlInternal(opt) {
            const inner = this.lastChild, { firstChild, lastChild } = inner;
            if (firstChild?.type === 'text' && firstChild.data.startsWith('\n')) {
                firstChild.deleteData(0, 1);
            }
            if (lastChild?.type === 'text' && lastChild.data.endsWith('\n')) {
                lastChild.deleteData(-1);
            }
            for (const { innerText, nextSibling } of this.querySelectorAll('comment')) {
                if (nextSibling?.type === 'text' && /^T:[^_/\n<>~]+$/u.test(innerText) && /^[\n ]/u.test(nextSibling.data)) {
                    nextSibling.deleteData(0, 1);
                }
            }
            return inner.toHtmlInternal(opt);
        }
    };
})();
exports.TranslateToken = TranslateToken;
constants_1.classes['TranslateToken'] = __filename;
