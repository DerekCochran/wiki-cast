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
exports.DoubleUnderscoreToken = void 0;
const hidden_1 = require("../../mixin/hidden");
const padded_1 = require("../../mixin/padded");
const base_1 = require("./base");
/* NOT FOR BROWSER */
const debug_1 = require("../../util/debug");
const constants_1 = require("../../util/constants");
const syntax_1 = require("../../mixin/syntax");
/* NOT FOR BROWSER END */
/**
 * behavior switch
 *
 * 状态开关
 */
let DoubleUnderscoreToken = (() => {
    let _classDecorators = [(0, syntax_1.syntax)(), (0, hidden_1.hiddenToken)(), (0, padded_1.padded)('__')];
    let _classDescriptor;
    let _classExtraInitializers = [];
    let _classThis;
    let _classSuper = base_1.NowikiBaseToken;
    var DoubleUnderscoreToken = class extends _classSuper {
        static { _classThis = this; }
        static {
            const _metadata = typeof Symbol === "function" && Symbol.metadata ? Object.create(_classSuper[Symbol.metadata] ?? null) : void 0;
            __esDecorate(null, _classDescriptor = { value: _classThis }, _classDecorators, { kind: "class", name: _classThis.name, metadata: _metadata }, null, _classExtraInitializers);
            DoubleUnderscoreToken = _classThis = _classDescriptor.value;
            if (_metadata) Object.defineProperty(_classThis, Symbol.metadata, { enumerable: true, configurable: true, writable: true, value: _metadata });
            __runInitializers(_classThis, _classExtraInitializers);
        }
        #fullWidth;
        /* NOT FOR BROWSER */
        #sensitive;
        /* NOT FOR BROWSER END */
        get type() {
            return 'double-underscore';
        }
        /**
         * @param word 状态开关名
         * @param sensitive 是否固定大小写
         * @param fullWidth 是否为全角下划线
         */
        constructor(word, sensitive, fullWidth, config, accum) {
            super(word, config, accum);
            const lc = word.toLowerCase(), [, , iAlias, sAlias] = config.doubleUnderscore;
            this.setAttribute('name', (sensitive ? sAlias?.[word]?.toLowerCase() : iAlias?.[lc]) ?? lc);
            this.#fullWidth = fullWidth;
            /* NOT FOR BROWSER */
            this.#sensitive = sensitive;
            this.setAttribute('pattern', new RegExp(`^${word}$`, sensitive ? 'u' : 'iu'));
        }
        /** @private */
        toString() {
            const underscore = this.#fullWidth ? '＿＿' : '__';
            return underscore + this.innerText + underscore;
        }
        /** @private */
        print() {
            PRINT: {
                const underscore = this.#fullWidth ? '＿＿' : '__';
                return super.print({ pre: underscore, post: underscore });
            }
        }
        /* NOT FOR BROWSER */
        cloneNode() {
            // @ts-expect-error abstract class
            return debug_1.Shadow.run(() => new DoubleUnderscoreToken(this.innerText, this.#sensitive, this.#fullWidth, this.getAttribute('config')));
        }
    };
    return DoubleUnderscoreToken = _classThis;
})();
exports.DoubleUnderscoreToken = DoubleUnderscoreToken;
constants_1.classes['DoubleUnderscoreToken'] = __filename;
