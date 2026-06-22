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
exports.NoincludeToken = void 0;
const lint_1 = require("../../util/lint");
const hidden_1 = require("../../mixin/hidden");
const index_1 = __importDefault(require("../../index"));
const base_1 = require("./base");
/* NOT FOR BROWSER */
const constants_1 = require("../../util/constants");
const debug_1 = require("../../util/debug");
/* NOT FOR BROWSER END */
/**
 * `<noinclude>` or `</noinclude>` that allows no modification
 *
 * `<noinclude>`或`</noinclude>`，不可进行任何更改
 */
let NoincludeToken = (() => {
    let _classDecorators = [(0, hidden_1.hiddenToken)(false)];
    let _classDescriptor;
    let _classExtraInitializers = [];
    let _classThis;
    let _classSuper = base_1.NowikiBaseToken;
    var NoincludeToken = class extends _classSuper {
        static { _classThis = this; }
        static {
            const _metadata = typeof Symbol === "function" && Symbol.metadata ? Object.create(_classSuper[Symbol.metadata] ?? null) : void 0;
            __esDecorate(null, _classDescriptor = { value: _classThis }, _classDecorators, { kind: "class", name: _classThis.name, metadata: _metadata }, null, _classExtraInitializers);
            NoincludeToken = _classThis = _classDescriptor.value;
            if (_metadata) Object.defineProperty(_classThis, Symbol.metadata, { enumerable: true, configurable: true, writable: true, value: _metadata });
            __runInitializers(_classThis, _classExtraInitializers);
        }
        /* NOT FOR BROWSER */
        #fixed;
        /* NOT FOR BROWSER END */
        get type() {
            return 'noinclude';
        }
        /* NOT FOR BROWSER */
        /** @param fixed 是否不可更改 */
        constructor(wikitext, config, accum, fixed = false) {
            super(wikitext, config, accum);
            this.#fixed = fixed;
        }
        /* NOT FOR BROWSER END */
        /** @private */
        toString(skip) {
            return skip ? '' : super.toString();
        }
        /** @private */
        lint(start = this.getAbsoluteIndex()) {
            LINT: {
                const { lintConfig } = index_1.default, rule = 'no-ignored', s = lintConfig.getSeverity(rule, 'include');
                if (s) {
                    const { innerText } = this, mt = /^<(includeonly|(?:no|only)include)\s+(?:[^\s>/]|\/(?!>))[^>]*>$/iu.exec(innerText);
                    if (mt) {
                        const e = (0, lint_1.generateForSelf)(this, { start }, rule, 'useless-attribute', s), { computeEditInfo } = lintConfig, before = mt[1].length + 1, after = innerText.endsWith('/>') ? 2 : 1;
                        e.startIndex += before;
                        e.startCol += before;
                        e.endIndex -= after;
                        e.endCol -= after;
                        if (computeEditInfo) {
                            e.suggestions = [(0, lint_1.fixByRemove)(e)];
                        }
                        return [e];
                    }
                }
                return [];
            }
        }
        /* NOT FOR BROWSER */
        cloneNode() {
            return debug_1.Shadow.run(() => {
                const C = this.constructor;
                return new C(this.innerText, this.getAttribute('config'), [], this.#fixed);
            });
        }
        /* c8 ignore start */
        setText(str) {
            return this.#fixed ? this.constructorError('cannot change the text content') : super.setText(str);
        }
    };
    return NoincludeToken = _classThis;
})();
exports.NoincludeToken = NoincludeToken;
constants_1.classes['NoincludeToken'] = __filename;
