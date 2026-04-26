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
exports.ParamLineToken = void 0;
const lint_1 = require("../util/lint");
const sharable_1 = require("../util/sharable");
const index_1 = __importDefault(require("../index"));
const index_2 = require("./index");
/* NOT FOR BROWSER */
const constants_1 = require("../util/constants");
const singleLine_1 = require("../mixin/singleLine");
const clone_1 = require("../mixin/clone");
/* NOT FOR BROWSER END */
/**
 * parameter of certain extension tags
 *
 * 某些扩展标签的参数
 */
let ParamLineToken = (() => {
    let _classDecorators = [singleLine_1.singleLine];
    let _classDescriptor;
    let _classExtraInitializers = [];
    let _classThis;
    let _classSuper = index_2.Token;
    let _instanceExtraInitializers = [];
    let _cloneNode_decorators;
    var ParamLineToken = class extends _classSuper {
        static { _classThis = this; }
        static {
            const _metadata = typeof Symbol === "function" && Symbol.metadata ? Object.create(_classSuper[Symbol.metadata] ?? null) : void 0;
            _cloneNode_decorators = [clone_1.clone];
            __esDecorate(this, null, _cloneNode_decorators, { kind: "method", name: "cloneNode", static: false, private: false, access: { has: obj => "cloneNode" in obj, get: obj => obj.cloneNode }, metadata: _metadata }, null, _instanceExtraInitializers);
            __esDecorate(null, _classDescriptor = { value: _classThis }, _classDecorators, { kind: "class", name: _classThis.name, metadata: _metadata }, null, _classExtraInitializers);
            ParamLineToken = _classThis = _classDescriptor.value;
            if (_metadata) Object.defineProperty(_classThis, Symbol.metadata, { enumerable: true, configurable: true, writable: true, value: _metadata });
            __runInitializers(_classThis, _classExtraInitializers);
        }
        /* NOT FOR BROWSER END */
        get type() {
            return 'param-line';
        }
        /** @param name 扩展标签名 */
        constructor(name, wikitext, config, accum, acceptable) {
            super(wikitext, config, accum, acceptable);
            __runInitializers(this, _instanceExtraInitializers);
            this.setAttribute('name', name);
        }
        /** @private */
        lint(start = this.getAbsoluteIndex()) {
            LINT: {
                const rule = 'no-ignored', { lintConfig } = index_1.default, { name, childNodes } = this, s = lintConfig.getSeverity(rule, name);
                if (!s) {
                    return [];
                }
                const msg = index_1.default.msg('invalid-parameter', name);
                if (childNodes.some(({ type }) => type === 'ext')) {
                    return [(0, lint_1.generateForSelf)(this, { start }, rule, msg, s)];
                }
                const children = childNodes
                    .filter(({ type }) => type !== 'comment' && type !== 'include' && type !== 'noinclude'), isInputbox = name === 'inputbox', i = isInputbox ? children.findIndex(({ type }) => type !== 'text') : -1;
                let str = children.slice(0, i === -1 ? undefined : i).map(String).join('').trim();
                if (str) {
                    if (isInputbox) {
                        str = str.toLowerCase();
                    }
                    const j = str.indexOf('='), key = str.slice(0, j === -1 ? undefined : j).trim(), params = sharable_1.extParams[name];
                    if (j === -1 ? i === -1 || !params.some(p => p.startsWith(key)) : !params.includes(key)) {
                        const e = (0, lint_1.generateForSelf)(this, { start }, rule, msg, s);
                        if (lintConfig.computeEditInfo) {
                            e.suggestions = [(0, lint_1.fixByRemove)(e)];
                        }
                        return [e];
                    }
                }
                return super.lint(start, false);
            }
        }
        /* NOT FOR BROWSER */
        cloneNode() {
            // @ts-expect-error abstract class
            return new ParamLineToken(this.name, undefined, this.getAttribute('config'), [], this.getAcceptable());
        }
    };
    return ParamLineToken = _classThis;
})();
exports.ParamLineToken = ParamLineToken;
constants_1.classes['ParamLineToken'] = __filename;
