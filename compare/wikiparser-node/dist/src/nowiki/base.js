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
exports.NowikiBaseToken = void 0;
const noEscape_1 = require("../../mixin/noEscape");
const index_1 = require("../index");
/* NOT FOR BROWSER */
const debug_1 = require("../../util/debug");
const constants_1 = require("../../util/constants");
const fixed_1 = require("../../mixin/fixed");
/**
 * text-only token that will not be parsed
 *
 * 纯文字Token，不会被解析
 * @classdesc `{childNodes: [AstText]}`
 */
let NowikiBaseToken = (() => {
    let _classDecorators = [fixed_1.fixedToken, noEscape_1.noEscape];
    let _classDescriptor;
    let _classExtraInitializers = [];
    let _classThis;
    let _classSuper = index_1.Token;
    var NowikiBaseToken = class extends _classSuper {
        static { _classThis = this; }
        static {
            const _metadata = typeof Symbol === "function" && Symbol.metadata ? Object.create(_classSuper[Symbol.metadata] ?? null) : void 0;
            __esDecorate(null, _classDescriptor = { value: _classThis }, _classDecorators, { kind: "class", name: _classThis.name, metadata: _metadata }, null, _classExtraInitializers);
            NowikiBaseToken = _classThis = _classDescriptor.value;
            if (_metadata) Object.defineProperty(_classThis, Symbol.metadata, { enumerable: true, configurable: true, writable: true, value: _metadata });
            __runInitializers(_classThis, _classExtraInitializers);
        }
        /* NOT FOR BROWSER END */
        /** text content / 纯文本部分 */
        get innerText() {
            return this.firstChild.data;
        }
        /** @param wikitext default: `''` */
        constructor(wikitext = '', config, accum) {
            super(wikitext, config, accum);
        }
        /* NOT FOR BROWSER */
        cloneNode() {
            return debug_1.Shadow.run(() => {
                const C = this.constructor, token = new C(this.innerText, this.getAttribute('config'));
                if (this.name && !token.name) {
                    token.setAttribute('name', this.name);
                }
                return token;
            });
        }
        /**
         * @override
         * @param str new text / 新文本
         */
        setText(str) {
            return super.setText(str);
        }
    };
    return NowikiBaseToken = _classThis;
})();
exports.NowikiBaseToken = NowikiBaseToken;
constants_1.classes['NowikiBaseToken'] = __filename;
