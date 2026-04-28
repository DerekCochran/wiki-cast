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
exports.OnlyincludeToken = void 0;
const padded_1 = require("../mixin/padded");
const noEscape_1 = require("../mixin/noEscape");
const index_1 = require("./index");
/* NOT FOR BROWSER */
const constants_1 = require("../util/constants");
const clone_1 = require("../mixin/clone");
const index_2 = __importDefault(require("../index"));
/* NOT FOR BROWSER END */
/**
 * `<onlyinclude>` during transclusion
 *
 * 嵌入时的`<onlyinclude>`
 * @classdesc `{childNodes: (AstText|Token)[]}`
 */
let OnlyincludeToken = (() => {
    let _classDecorators = [noEscape_1.noEscape, (0, padded_1.padded)('<onlyinclude>')];
    let _classDescriptor;
    let _classExtraInitializers = [];
    let _classThis;
    let _classSuper = index_1.Token;
    let _instanceExtraInitializers = [];
    let _cloneNode_decorators;
    var OnlyincludeToken = class extends _classSuper {
        static { _classThis = this; }
        static {
            const _metadata = typeof Symbol === "function" && Symbol.metadata ? Object.create(_classSuper[Symbol.metadata] ?? null) : void 0;
            _cloneNode_decorators = [clone_1.clone];
            __esDecorate(this, null, _cloneNode_decorators, { kind: "method", name: "cloneNode", static: false, private: false, access: { has: obj => "cloneNode" in obj, get: obj => obj.cloneNode }, metadata: _metadata }, null, _instanceExtraInitializers);
            __esDecorate(null, _classDescriptor = { value: _classThis }, _classDecorators, { kind: "class", name: _classThis.name, metadata: _metadata }, null, _classExtraInitializers);
            OnlyincludeToken = _classThis = _classDescriptor.value;
            if (_metadata) Object.defineProperty(_classThis, Symbol.metadata, { enumerable: true, configurable: true, writable: true, value: _metadata });
            __runInitializers(_classThis, _classExtraInitializers);
        }
        get type() {
            return 'onlyinclude';
        }
        /* NOT FOR BROWSER */
        /** inner wikitext / 内部wikitext */
        get innerText() {
            return this.text();
        }
        /** @throws `RangeError` 不允许包含`</onlyinclude>` */
        set innerText(text) {
            /* c8 ignore next 3 */
            if (text.includes('</onlyinclude>')) {
                throw new RangeError('"</onlyinclude>" is not allowed in the text!');
            }
            const { childNodes } = index_2.default.parseWithRef(text, this, undefined, true);
            this.safeReplaceChildren(childNodes);
        }
        /* NOT FOR BROWSER END */
        /** @private */
        toString(skip) {
            return `<onlyinclude>${super.toString(skip)}</onlyinclude>`;
        }
        /** @private */
        isPlain() {
            return true;
        }
        /** @private */
        print() {
            PRINT: return super.print({
                pre: '<span class="wpb-ext">&lt;onlyinclude&gt;</span>',
                post: '<span class="wpb-ext">&lt;/onlyinclude&gt;</span>',
            });
        }
        /* NOT FOR BROWSER */
        cloneNode() {
            return new OnlyincludeToken(undefined, this.getAttribute('config'));
        }
        constructor() {
            super(...arguments);
            __runInitializers(this, _instanceExtraInitializers);
        }
    };
    return OnlyincludeToken = _classThis;
})();
exports.OnlyincludeToken = OnlyincludeToken;
constants_1.classes['OnlyincludeToken'] = __filename;
