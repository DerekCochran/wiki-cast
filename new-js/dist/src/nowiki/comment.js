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
exports.CommentToken = void 0;
const lint_1 = require("../../util/lint");
const hidden_1 = require("../../mixin/hidden");
const padded_1 = require("../../mixin/padded");
const index_1 = __importDefault(require("../../index"));
const base_1 = require("./base");
/* NOT FOR BROWSER */
const debug_1 = require("../../util/debug");
const constants_1 = require("../../util/constants");
/* NOT FOR BROWSER END */
/**
 * invisible HTML comment
 *
 * HTML注释，不可见
 */
let CommentToken = (() => {
    let _classDecorators = [(0, hidden_1.hiddenToken)(false), (0, padded_1.padded)('<!--')];
    let _classDescriptor;
    let _classExtraInitializers = [];
    let _classThis;
    let _classSuper = base_1.NowikiBaseToken;
    var CommentToken = class extends _classSuper {
        static { _classThis = this; }
        static {
            const _metadata = typeof Symbol === "function" && Symbol.metadata ? Object.create(_classSuper[Symbol.metadata] ?? null) : void 0;
            __esDecorate(null, _classDescriptor = { value: _classThis }, _classDecorators, { kind: "class", name: _classThis.name, metadata: _metadata }, null, _classExtraInitializers);
            CommentToken = _classThis = _classDescriptor.value;
            if (_metadata) Object.defineProperty(_classThis, Symbol.metadata, { enumerable: true, configurable: true, writable: true, value: _metadata });
            __runInitializers(_classThis, _classExtraInitializers);
        }
        closed;
        get type() {
            return 'comment';
        }
        /* NOT FOR BROWSER */
        /** comment content / 内部文本 */
        get innerText() {
            return super.innerText;
        }
        set innerText(text) {
            this.setText(text);
        }
        /* NOT FOR BROWSER END */
        /** @param closed 是否闭合 */
        constructor(wikitext, closed, config, accum) {
            super(wikitext, config, accum);
            this.closed = closed;
        }
        /** @private */
        lint(start = this.getAbsoluteIndex()) {
            LINT: {
                if (this.closed) {
                    return [];
                }
                const rule = 'unclosed-comment', { lintConfig } = index_1.default, s = lintConfig.getSeverity(rule);
                /* c8 ignore next 3 */
                if (!s) {
                    return [];
                }
                const e = (0, lint_1.generateForSelf)(this, { start }, rule, 'unclosed-comment', s);
                if (lintConfig.computeEditInfo) {
                    e.suggestions = [(0, lint_1.fixByClose)(e.endIndex, '-->')];
                }
                return [e];
            }
        }
        /** @private */
        toString(skip) {
            /* NOT FOR BROWSER */
            /* c8 ignore start */
            if (!this.closed && this.nextSibling) {
                index_1.default.error('Auto-closing HTML comment', this);
                this.closed = true;
            }
            /* c8 ignore stop */
            /* NOT FOR BROWSER END */
            return skip ? '' : `<!--${this.innerText}${this.closed ? '-->' : ''}`;
        }
        /** @private */
        print() {
            PRINT: return super.print({ pre: '&lt;!--', post: this.closed ? '--&gt;' : '' });
        }
        /* NOT FOR BROWSER */
        cloneNode() {
            return debug_1.Shadow.run(
            // @ts-expect-error abstract class
            () => new CommentToken(this.innerText, this.closed, this.getAttribute('config')));
        }
        /** @private */
        setText(text) {
            /* c8 ignore next 3 */
            if (text.includes('-->')) {
                throw new RangeError('Do not contain "-->" in the comment!');
            }
            return super.setText(text);
        }
    };
    return CommentToken = _classThis;
})();
exports.CommentToken = CommentToken;
constants_1.classes['CommentToken'] = __filename;
