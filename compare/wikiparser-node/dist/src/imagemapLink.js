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
exports.ImagemapLinkToken = void 0;
const index_1 = require("./index");
const noinclude_1 = require("./nowiki/noinclude");
const index_2 = require("./link/index");
const extLink_1 = require("./extLink");
/* NOT FOR BROWSER */
const constants_1 = require("../util/constants");
const fixed_1 = require("../mixin/fixed");
const singleLine_1 = require("../mixin/singleLine");
/* NOT FOR BROWSER END */
/**
 * link inside the `<imagemap>`
 *
 * `<imagemap>`内的链接
 * @classdesc `{childNodes: [AstText, LinkToken|ExtLinkToken, NoincludeToken]}`
 */
let ImagemapLinkToken = (() => {
    let _classDecorators = [fixed_1.fixedToken, singleLine_1.singleLine];
    let _classDescriptor;
    let _classExtraInitializers = [];
    let _classThis;
    let _classSuper = index_1.Token;
    var ImagemapLinkToken = class extends _classSuper {
        static { _classThis = this; }
        static {
            const _metadata = typeof Symbol === "function" && Symbol.metadata ? Object.create(_classSuper[Symbol.metadata] ?? null) : void 0;
            __esDecorate(null, _classDescriptor = { value: _classThis }, _classDecorators, { kind: "class", name: _classThis.name, metadata: _metadata }, null, _classExtraInitializers);
            ImagemapLinkToken = _classThis = _classDescriptor.value;
            if (_metadata) Object.defineProperty(_classThis, Symbol.metadata, { enumerable: true, configurable: true, writable: true, value: _metadata });
            __runInitializers(_classThis, _classExtraInitializers);
        }
        /* NOT FOR BROWSER END */
        get type() {
            return 'imagemap-link';
        }
        /* NOT FOR BROWSER */
        /** internal or external link / 内外链接 */
        get link() {
            return this.childNodes[1].link;
        }
        set link(link) {
            this.childNodes[1].link = link;
        }
        /* NOT FOR BROWSER END */
        /**
         * @param pre 链接前的文本
         * @param linkStuff 内外链接
         * @param post 链接后的文本
         */
        constructor(pre, linkStuff, post, config, accum = []) {
            super(undefined, config, accum);
            this.append(pre, linkStuff.length === 2
                // @ts-expect-error abstract class
                ? new index_2.LinkToken(...linkStuff, config, accum)
                // @ts-expect-error abstract class
                : new extLink_1.ExtLinkToken(...linkStuff, config, accum), 
            // @ts-expect-error abstract class
            new noinclude_1.NoincludeToken(post, config, accum));
        }
    };
    return ImagemapLinkToken = _classThis;
})();
exports.ImagemapLinkToken = ImagemapLinkToken;
constants_1.classes['ImagemapLinkToken'] = __filename;
