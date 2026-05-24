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
exports.RedirectToken = void 0;
const hidden_1 = require("../mixin/hidden");
const noEscape_1 = require("../mixin/noEscape");
const index_1 = require("./index");
const syntax_1 = require("./syntax");
const redirectTarget_1 = require("./link/redirectTarget");
/* NOT FOR BROWSER */
const common_1 = require("@bhsd/common");
const constants_1 = require("../util/constants");
const debug_1 = require("../util/debug");
const fixed_1 = require("../mixin/fixed");
/^(?:#redirect|#重定向)\s*(?::\s*)?$/iu; // eslint-disable-line @typescript-eslint/no-unused-expressions
const getPattern = (0, common_1.getRegex)(redirection => new RegExp(String.raw `^(?:${redirection.join('|')})\s*(?::\s*)?$`, 'iu'));
/* NOT FOR BROWSER END */
/**
 * redirect
 *
 * 重定向
 * @classdesc `{childNodes: [SyntaxToken, LinkToken]}`
 */
let RedirectToken = (() => {
    let _classDecorators = [fixed_1.fixedToken, (0, hidden_1.hiddenToken)(false, false), noEscape_1.noEscape];
    let _classDescriptor;
    let _classExtraInitializers = [];
    let _classThis;
    let _classSuper = index_1.Token;
    var RedirectToken = class extends _classSuper {
        static { _classThis = this; }
        static {
            const _metadata = typeof Symbol === "function" && Symbol.metadata ? Object.create(_classSuper[Symbol.metadata] ?? null) : void 0;
            __esDecorate(null, _classDescriptor = { value: _classThis }, _classDecorators, { kind: "class", name: _classThis.name, metadata: _metadata }, null, _classExtraInitializers);
            RedirectToken = _classThis = _classDescriptor.value;
            if (_metadata) Object.defineProperty(_classThis, Symbol.metadata, { enumerable: true, configurable: true, writable: true, value: _metadata });
            __runInitializers(_classThis, _classExtraInitializers);
        }
        #pre;
        #post;
        /* NOT FOR BROWSER END */
        get type() {
            return 'redirect';
        }
        /**
         * @param pre leading whitespace
         * @param syntax 重定向魔术字
         * @param link 重定向目标
         * @param text 重定向显示文本（无效）
         * @param post trailing whitespace
         */
        constructor(pre, syntax, link, text, post, config, accum = []) {
            super(undefined, config, accum);
            this.#pre = pre;
            this.#post = post;
            this.append(new syntax_1.SyntaxToken(syntax, getPattern(config.redirection), 'redirect-syntax', config, accum, { AstText: ':' }), 
            // @ts-expect-error abstract class
            new redirectTarget_1.RedirectTargetToken(link, text?.slice(1), config, accum));
        }
        /** @private */
        getAttribute(key) {
            return key === 'padding' ? this.#pre.length : super.getAttribute(key);
        }
        /** @private */
        toString(skip) {
            return this.#pre + super.toString(skip) + this.#post;
        }
        /** @private */
        lint(start = this.getAbsoluteIndex()) {
            LINT: {
                const index = start + this.#pre.length + this.firstChild.toString().length;
                this.lastChild.setAttribute('aIndex', index);
                return this.lastChild.lint(index);
            }
        }
        /** @private */
        print() {
            PRINT: return super.print({ pre: this.#pre, post: this.#post });
        }
        /* NOT FOR BROWSER */
        cloneNode() {
            const cloned = this.cloneChildNodes();
            return debug_1.Shadow.run(() => {
                // @ts-expect-error abstract class
                const token = new RedirectToken(this.#pre, undefined, '', undefined, this.#post, this.getAttribute('config'));
                token.firstChild.safeReplaceWith(cloned[0]);
                token.lastChild.safeReplaceWith(cloned[1]);
                return token;
            });
        }
        /** @private */
        toHtmlInternal() {
            return `<ul class="redirectText"><li>${this.lastChild.toHtmlInternal()}</li></ul>`;
        }
    };
    return RedirectToken = _classThis;
})();
exports.RedirectToken = RedirectToken;
constants_1.classes['RedirectToken'] = __filename;
