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
exports.MultiLineToken = void 0;
const index_1 = require("../index");
/* NOT FOR BROWSER */
const constants_1 = require("../../util/constants");
const clone_1 = require("../../mixin/clone");
/* NOT FOR BROWSER END */
/**
 * extension tag that is parsed line by line
 *
 * 逐行解析的扩展标签
 */
let MultiLineToken = (() => {
    let _classSuper = index_1.Token;
    let _instanceExtraInitializers = [];
    let _cloneNode_decorators;
    return class MultiLineToken extends _classSuper {
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
        /** @private */
        toString(skip) {
            return super.toString(skip, '\n');
        }
        /** @private */
        text() {
            return super.text('\n').replace(/\n\s*\n/gu, '\n');
        }
        /** @private */
        getGaps() {
            return 1;
        }
        /** @private */
        print() {
            PRINT: return super.print({ sep: '\n' });
        }
        /* NOT FOR BROWSER */
        cloneNode() {
            const C = this.constructor;
            return new C(undefined, this.getAttribute('config'));
        }
        constructor() {
            super(...arguments);
            __runInitializers(this, _instanceExtraInitializers);
        }
    };
})();
exports.MultiLineToken = MultiLineToken;
constants_1.classes['MultiLineToken'] = __filename;
