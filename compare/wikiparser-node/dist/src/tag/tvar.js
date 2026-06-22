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
Object.defineProperty(exports, "__esModule", { value: true });
exports.TvarToken = void 0;
const hidden_1 = require("../../mixin/hidden");
const noEscape_1 = require("../../mixin/noEscape");
const index_1 = require("./index");
const syntax_1 = require("../syntax");
/* NOT FOR BROWSER */
const constants_1 = require("../../util/constants");
const debug_1 = require("../../util/debug");
const legacyPattern = /^\|([^>]+)$/u, newPattern = /^\s+name\s*=(?:\s*(?:(["'])([\s\S]*?)\1|([^"'\s>]+)))?\s*$/iu, legacyClosingPattern = /^$/u, newClosingPattern = /^\s*$/u;
/* NOT FOR BROWSER END */
/**
 * `<tvar>`
 * @classdesc `{childNodes: [SyntaxToken]}`
 */
let TvarToken = (() => {
    let _classDecorators = [(0, hidden_1.hiddenToken)(), noEscape_1.noEscape];
    let _classDescriptor;
    let _classExtraInitializers = [];
    let _classThis;
    let _classSuper = index_1.TagToken;
    var TvarToken = class extends _classSuper {
        static { _classThis = this; }
        static {
            const _metadata = typeof Symbol === "function" && Symbol.metadata ? Object.create(_classSuper[Symbol.metadata] ?? null) : void 0;
            __esDecorate(null, _classDescriptor = { value: _classThis }, _classDecorators, { kind: "class", name: _classThis.name, metadata: _metadata }, null, _classExtraInitializers);
            TvarToken = _classThis = _classDescriptor.value;
            if (_metadata) Object.defineProperty(_classThis, Symbol.metadata, { enumerable: true, configurable: true, writable: true, value: _metadata });
            __runInitializers(_classThis, _classExtraInitializers);
        }
        /* NOT FOR BROWSER END */
        get type() {
            return 'tvar';
        }
        /* NOT FOR BROWSER */
        /** @private */
        get closing() {
            return super.closing;
        }
        /** whether to use the legacy syntax / 是否使用旧语法 */
        get legacy() {
            const { pattern } = this.firstChild;
            return pattern === legacyPattern || pattern === legacyClosingPattern;
        }
        /* NOT FOR BROWSER END */
        /**
         * @param tag 标签名
         * @param attr 标签属性
         * @param closing 是否闭合
         */
        constructor(tag, attr, closing, config, accum) {
            /* NOT FOR BROWSER */
            let pattern;
            if (closing) {
                pattern = tag ? newClosingPattern : legacyClosingPattern;
            }
            else {
                pattern = legacyPattern.test(attr) ? legacyPattern : newPattern;
            }
            /* NOT FOR BROWSER END */
            const attrToken = new syntax_1.SyntaxToken(attr, pattern, 'tvar-name', config, accum, { AstText: ':' });
            super(tag, attrToken, closing, config, accum);
            /* NOT FOR BROWSER */
            if (!closing) {
                this.#setName(attr, pattern);
            }
        }
        /* NOT FOR BROWSER */
        /**
         * 设置name
         * @param attr 标签属性
         * @param pattern 标签属性模式
         */
        #setName(attr, pattern) {
            let name;
            if (pattern === legacyPattern) {
                [, name] = legacyPattern.exec(attr);
            }
            else {
                const mt = newPattern.exec(attr);
                name = mt[2] ?? mt[3];
            }
            this.setAttribute('name', name);
        }
        /** @private */
        afterBuild() {
            super.afterBuild();
            if (!this.closing) {
                const /** @implements */ tvarListener = ({ prevTarget }) => {
                    const { firstChild } = this;
                    if (prevTarget === firstChild) {
                        const { pattern } = firstChild, attr = firstChild.toString();
                        if (pattern.test(attr)) {
                            this.#setName(attr, pattern);
                        }
                    }
                };
                this.addEventListener(['remove', 'insert', 'replace', 'text'], tvarListener);
            }
        }
        cloneNode() {
            // @ts-expect-error abstract class
            return debug_1.Shadow.run(() => new TvarToken(this.tag, this.firstChild.toString(), this.closing, this.getAttribute('config')));
        }
        /**
         * Set the tvar name.
         *
         * 设置tvar变量名。
         * @param name name / 变量名
         * @since v1.28.0
         * @throws `Error` 闭合标签
         * @throws `SyntaxError` 同时包含单引号和双引号
         */
        setName(name) {
            const { closing, firstChild } = this;
            /* c8 ignore next 3 */
            if (closing) {
                throw new Error('Cannot set name of a closing tvar tag');
            }
            if (firstChild.pattern === legacyPattern) {
                firstChild.replaceChildren(`|${name}`);
            }
            else if (name.includes('"') && name.includes("'")) {
                /* c8 ignore next */
                throw new SyntaxError('Tvar name cannot contain both single and double quotes');
            }
            else {
                const quote = name.includes('"') ? "'" : '"';
                firstChild.replaceChildren(` name=${quote}${name}${quote}`);
            }
        }
    };
    return TvarToken = _classThis;
})();
exports.TvarToken = TvarToken;
constants_1.classes['TvarToken'] = __filename;
