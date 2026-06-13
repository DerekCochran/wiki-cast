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
exports.PreToken = void 0;
const constants_1 = require("../util/constants");
const index_1 = require("./index");
const noinclude_1 = require("./nowiki/noinclude");
/* NOT FOR BROWSER */
const clone_1 = require("../mixin/clone");
/* NOT FOR BROWSER END */
/**
 * `<pre>`
 * @classdesc `{childNodes: (AstText|NoincludeToken|ConverterToken)[]}`
 */
let PreToken = (() => {
    let _classSuper = index_1.Token;
    let _instanceExtraInitializers = [];
    let _cloneNode_decorators;
    return class PreToken extends _classSuper {
        static {
            const _metadata = typeof Symbol === "function" && Symbol.metadata ? Object.create(_classSuper[Symbol.metadata] ?? null) : void 0;
            _cloneNode_decorators = [clone_1.clone];
            __esDecorate(this, null, _cloneNode_decorators, { kind: "method", name: "cloneNode", static: false, private: false, access: { has: obj => "cloneNode" in obj, get: obj => obj.cloneNode }, metadata: _metadata }, null, _instanceExtraInitializers);
            if (_metadata) Object.defineProperty(this, Symbol.metadata, { enumerable: true, configurable: true, writable: true, value: _metadata });
        }
        /* NOT FOR BROWSER END */
        get type() {
            return 'ext-inner';
        }
        /** @class */
        constructor(wikitext, config, accum = []) {
            if (wikitext) {
                const opening = /<nowiki>/giu, closing = /<\/nowiki>/giu, { length } = opening.source;
                let i = opening.exec(wikitext);
                if (i) {
                    closing.lastIndex = i.index + length;
                }
                let j = closing.exec(wikitext), lastIndex = 0, str = '';
                while (i && j) {
                    // @ts-expect-error abstract class
                    new noinclude_1.NoincludeToken(i[0], config, accum, true);
                    // @ts-expect-error abstract class
                    new noinclude_1.NoincludeToken(j[0], config, accum, true);
                    str += `${wikitext.slice(lastIndex, i.index)}\0${accum.length - 1}n\x7F${wikitext.slice(i.index + length, j.index)}\0${accum.length}n\x7F`;
                    lastIndex = j.index + length + 1;
                    opening.lastIndex = lastIndex;
                    i = opening.exec(wikitext);
                    if (i) {
                        closing.lastIndex = i.index + length;
                    }
                    j = closing.exec(wikitext);
                }
                wikitext = str + wikitext.slice(lastIndex);
            }
            super(wikitext, config, accum, {
                AstText: ':', NoincludeToken: ':', ConverterToken: ':',
            });
            __runInitializers(this, _instanceExtraInitializers);
            this.setAttribute('stage', constants_1.MAX_STAGE - 1);
        }
        /** @private */
        isPlain() {
            return true;
        }
        /** @private */
        lint(start = this.getAbsoluteIndex()) {
            LINT: return super.lint(start, /<\s*\/\s*(pre)\b/giu);
        }
        /* NOT FOR BROWSER */
        cloneNode() {
            // @ts-expect-error abstract class
            return new PreToken(undefined, this.getAttribute('config'));
        }
    };
})();
exports.PreToken = PreToken;
constants_1.classes['PreToken'] = __filename;
