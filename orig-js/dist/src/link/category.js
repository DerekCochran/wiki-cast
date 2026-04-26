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
exports.CategoryToken = void 0;
const base_1 = require("./base");
/* PRINT ONLY */
const string_1 = require("../../util/string");
/* PRINT ONLY END */
/* NOT FOR BROWSER */
const constants_1 = require("../../util/constants");
const cached_1 = require("../../mixin/cached");
/* NOT FOR BROWSER END */
/**
 * category
 *
 * 分类
 * @classdesc `{childNodes: [AtomToken, ?Token]}`
 */
let CategoryToken = (() => {
    let _classSuper = base_1.LinkBaseToken;
    let _instanceExtraInitializers = [];
    let _toHtmlInternal_decorators;
    return class CategoryToken extends _classSuper {
        static {
            const _metadata = typeof Symbol === "function" && Symbol.metadata ? Object.create(_classSuper[Symbol.metadata] ?? null) : void 0;
            _toHtmlInternal_decorators = [(0, cached_1.cached)()];
            __esDecorate(this, null, _toHtmlInternal_decorators, { kind: "method", name: "toHtmlInternal", static: false, private: false, access: { has: obj => "toHtmlInternal" in obj, get: obj => obj.toHtmlInternal }, metadata: _metadata }, null, _instanceExtraInitializers);
            if (_metadata) Object.defineProperty(this, Symbol.metadata, { enumerable: true, configurable: true, writable: true, value: _metadata });
        }
        /* NOT FOR BROWSER END */
        get type() {
            return 'category';
        }
        /* PRINT ONLY */
        /** sort key / 分类排序关键字 */
        get sortkey() {
            LSP: {
                const [, child] = this.childNodes;
                return child && (0, string_1.decodeHtml)(child.text());
            }
        }
        /* PRINT ONLY END */
        /* NOT FOR BROWSER */
        set sortkey(text) {
            this.setSortkey(text);
        }
        /**
         * link text
         *
         * 链接显示文字
         * @since v1.32.0
         */
        get innerText() {
            return this.link.main;
        }
        /* NOT FOR BROWSER END */
        /** @private */
        json(_, depth, start = this.getAbsoluteIndex()) {
            LSP: {
                const json = super.json(undefined, depth, start), { sortkey } = this;
                if (sortkey) {
                    json['sortkey'] = sortkey;
                }
                return json;
            }
        }
        /* NOT FOR BROWSER */
        /**
         * Set the sort key
         *
         * 设置排序关键字
         * @param text sort key / 排序关键字
         */
        setSortkey(text) {
            this.setLinkText(text);
        }
        /** @private */
        toHtmlInternal() {
            constants_1.states.get(this.getRootNode())?.categories.add(super.toHtmlInternal());
            return '';
        }
        constructor() {
            super(...arguments);
            __runInitializers(this, _instanceExtraInitializers);
        }
    };
})();
exports.CategoryToken = CategoryToken;
constants_1.classes['CategoryToken'] = __filename;
