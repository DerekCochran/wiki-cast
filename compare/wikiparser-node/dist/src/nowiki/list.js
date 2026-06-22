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
exports.ListToken = void 0;
const lint_1 = require("../../util/lint");
const index_1 = __importDefault(require("../../index"));
const listBase_1 = require("./listBase");
/* NOT FOR BROWSER */
const constants_1 = require("../../util/constants");
const sol_1 = require("../../mixin/sol");
const syntax_1 = require("../../mixin/syntax");
/* NOT FOR BROWSER END */
const linkTypes = new Set(['link', 'category', 'file']);
/**
 * `;:*#` at the start of a line
 *
 * 位于行首的`;:*#`
 */
let ListToken = (() => {
    let _classDecorators = [(0, sol_1.sol)(true), (0, syntax_1.syntax)(/^[;:*#]+[^\S\n]*$/u)];
    let _classDescriptor;
    let _classExtraInitializers = [];
    let _classThis;
    let _classSuper = listBase_1.ListBaseToken;
    var ListToken = class extends _classSuper {
        static { _classThis = this; }
        static {
            const _metadata = typeof Symbol === "function" && Symbol.metadata ? Object.create(_classSuper[Symbol.metadata] ?? null) : void 0;
            __esDecorate(null, _classDescriptor = { value: _classThis }, _classDecorators, { kind: "class", name: _classThis.name, metadata: _metadata }, null, _classExtraInitializers);
            ListToken = _classThis = _classDescriptor.value;
            if (_metadata) Object.defineProperty(_classThis, Symbol.metadata, { enumerable: true, configurable: true, writable: true, value: _metadata });
            __runInitializers(_classThis, _classExtraInitializers);
        }
        get type() {
            return 'list';
        }
        /** @private */
        lint(start = this.getAbsoluteIndex()) {
            LINT: {
                const rule = 'syntax-like', s = index_1.default.lintConfig.getSeverity(rule, 'redirect'), { innerText } = this;
                if (s && innerText === '#') {
                    let { nextSibling } = this;
                    /* NOT FOR BROWSER */
                    if (nextSibling?.is('list-range')) {
                        nextSibling = nextSibling.firstChild;
                    }
                    /* NOT FOR BROWSER END */
                    if (nextSibling?.type === 'text' && linkTypes.has(nextSibling.nextSibling?.type)) {
                        /^redirect\s*(?::\s*)?$/iu; // eslint-disable-line @typescript-eslint/no-unused-expressions
                        const re = new RegExp(String.raw `^(?:${this.getAttribute('config').redirection.join('|')})\s*(?::\s*)?$`, 'iu');
                        if (re.test(`#${nextSibling.data}`)) {
                            const e = (0, lint_1.generateForSelf)(nextSibling, { start: start + 1 }, rule, 'redirect-like', s);
                            e.startIndex--;
                            e.startCol--;
                            e.endIndex += 2;
                            e.endCol += 2;
                            return [e];
                        }
                    }
                }
                return [];
            }
        }
    };
    return ListToken = _classThis;
})();
exports.ListToken = ListToken;
constants_1.classes['ListToken'] = __filename;
