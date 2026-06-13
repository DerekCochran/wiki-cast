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
exports.CategorytreeToken = void 0;
const lint_1 = require("../../util/lint");
const padded_1 = require("../../mixin/padded");
const index_1 = __importDefault(require("../../index"));
const base_1 = require("./base");
/* NOT FOR BROWSER */
const constants_1 = require("../../util/constants");
const cached_1 = require("../../mixin/cached");
const fixed_1 = require("../../mixin/fixed");
/* NOT FOR BROWSER END */
/**
 * `<categorytree>`
 * @classdesc `{childNodes: [AtomToken]}`
 */
let CategorytreeToken = (() => {
    let _classDecorators = [fixed_1.fixedToken, (0, padded_1.padded)('')];
    let _classDescriptor;
    let _classExtraInitializers = [];
    let _classThis;
    let _classSuper = base_1.LinkBaseToken;
    let _instanceExtraInitializers = [];
    let _toHtmlInternal_decorators;
    var CategorytreeToken = class extends _classSuper {
        static { _classThis = this; }
        static {
            const _metadata = typeof Symbol === "function" && Symbol.metadata ? Object.create(_classSuper[Symbol.metadata] ?? null) : void 0;
            _toHtmlInternal_decorators = [(0, cached_1.cached)()];
            __esDecorate(this, null, _toHtmlInternal_decorators, { kind: "method", name: "toHtmlInternal", static: false, private: false, access: { has: obj => "toHtmlInternal" in obj, get: obj => obj.toHtmlInternal }, metadata: _metadata }, null, _instanceExtraInitializers);
            __esDecorate(null, _classDescriptor = { value: _classThis }, _classDecorators, { kind: "class", name: _classThis.name, metadata: _metadata }, null, _classExtraInitializers);
            CategorytreeToken = _classThis = _classDescriptor.value;
            if (_metadata) Object.defineProperty(_classThis, Symbol.metadata, { enumerable: true, configurable: true, writable: true, value: _metadata });
            __runInitializers(_classThis, _classExtraInitializers);
        }
        /* NOT FOR BROWSER END */
        get type() {
            return 'ext-inner';
        }
        /** @param link 链接标题 */
        constructor(link, linkText, config, accum = []) {
            super(link, linkText, config, accum);
            __runInitializers(this, _instanceExtraInitializers);
            this.setAttribute('bracket', false);
            /* NOT FOR BROWSER */
            // @ts-expect-error abstract getter override
            this.firstChild.setAttribute('acceptable', { AstText: 0 });
        }
        /** @private */
        getTitle() {
            const target = this.firstChild.toString().trim(), opt = { halfParsed: true }, title = this.normalizeTitle(target, 14, opt);
            return title.valid && title.ns === 14
                && !title.interwiki
                ? title
                : this.normalizeTitle(`Category:${target}`, 0, opt);
        }
        /** @private */
        lint(start = this.getAbsoluteIndex()) {
            LINT: {
                const rule = 'no-ignored', s = index_1.default.lintConfig.getSeverity(rule, 'categorytree');
                if (s) {
                    const { link } = this;
                    if (!link.valid || link.ns !== 14
                        || link.interwiki) {
                        return [(0, lint_1.generateForSelf)(this, { start }, rule, 'invalid-category', s)];
                    }
                }
                return super.lint(start, false);
            }
        }
        /* NOT FOR BROWSER */
        /** @private */
        toHtmlInternal() {
            return '';
        }
        /** @private */
        safeReplaceChildren(elements) {
            if (elements.length === 0) {
                this.firstChild.replaceChildren();
            }
            else {
                super.safeReplaceChildren(elements);
            }
        }
    };
    return CategorytreeToken = _classThis;
})();
exports.CategorytreeToken = CategorytreeToken;
constants_1.classes['CategorytreeToken'] = __filename;
