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
exports.GalleryImageToken = void 0;
const lint_1 = require("../../util/lint");
const constants_1 = require("../../util/constants");
const padded_1 = require("../../mixin/padded");
const index_1 = __importDefault(require("../../index"));
const index_2 = require("../index");
const file_1 = require("./file");
/* NOT FOR BROWSER */
const debug_1 = require("../../util/debug");
const singleLine_1 = require("../../mixin/singleLine");
/**
 * gallery image
 *
 * 图库图片
 */
let GalleryImageToken = (() => {
    let _classDecorators = [singleLine_1.singleLine, (0, padded_1.padded)('')];
    let _classDescriptor;
    let _classExtraInitializers = [];
    let _classThis;
    let _classSuper = file_1.FileToken;
    var GalleryImageToken = class extends _classSuper {
        static { _classThis = this; }
        static {
            const _metadata = typeof Symbol === "function" && Symbol.metadata ? Object.create(_classSuper[Symbol.metadata] ?? null) : void 0;
            __esDecorate(null, _classDescriptor = { value: _classThis }, _classDecorators, { kind: "class", name: _classThis.name, metadata: _metadata }, null, _classExtraInitializers);
            GalleryImageToken = _classThis = _classDescriptor.value;
            if (_metadata) Object.defineProperty(_classThis, Symbol.metadata, { enumerable: true, configurable: true, writable: true, value: _metadata });
            __runInitializers(_classThis, _classExtraInitializers);
        }
        /** @private */
        privateType = 'imagemap-image';
        get type() {
            return this.privateType;
        }
        /* NOT FOR BROWSER */
        /** @private */
        get link() {
            return this.type === 'imagemap-image' ? '' : super.link;
        }
        /** @private */
        set link(value) {
            if (this.type !== 'imagemap-image') {
                super.link = value;
            }
        }
        /* NOT FOR BROWSER END */
        /**
         * @param type 图片类型
         * @param link 图片文件名
         * @param text 图片参数
         */
        constructor(type, link, text, config, accum = []) {
            let token;
            if (text !== undefined) {
                const { length } = accum;
                token = new index_2.Token(text, config, accum);
                for (let n = 0; n < constants_1.MAX_STAGE - 1; n++) {
                    token.parseOnce();
                }
                accum.splice(length, 1);
            }
            const privateType = `${type}-image`;
            super(link, token?.firstChild.toString(), config, accum, undefined, privateType);
            this.setAttribute('bracket', false);
            this.privateType = privateType;
            /* PRINT ONLY */
            this.seal('privateType', true);
        }
        /** @private */
        getTitle(temporary) {
            const imagemap = this.type === 'imagemap-image';
            return this.normalizeTitle(this.firstChild.toString(), imagemap ? 0 : 6, { halfParsed: true, temporary, decode: !imagemap, page: '' });
        }
        /** 判定无效的图片 */
        #lint() {
            const title = this.getAttribute('title');
            return title.ns !== 6
                || Boolean(title.interwiki);
        }
        /** @private */
        lint(start = this.getAbsoluteIndex(), re) {
            LINT: {
                const errors = super.lint(start, re), rule = 'invalid-gallery', { lintConfig } = index_1.default, s = lintConfig.getSeverity(rule, 'image');
                if (s && this.#lint()) {
                    const e = (0, lint_1.generateForSelf)(this, { start }, rule, 'invalid-gallery', s);
                    if (lintConfig.computeEditInfo) {
                        e.suggestions = [(0, lint_1.fixByInsert)(start, 'prefix', 'File:')];
                    }
                    errors.push(e);
                }
                return errors;
            }
        }
        /* PRINT ONLY */
        /** @private */
        getAttribute(key) {
            return key === 'invalid' ? this.#lint() : super.getAttribute(key);
        }
        /* PRINT ONLY END */
        /* NOT FOR BROWSER */
        /**
         * @override
         * @param child node to be inserted / 待插入的子节点
         * @param i position to be inserted at / 插入位置
         */
        insertAt(child, i) {
            if (this.type === 'gallery-image'
                && child.is('image-parameter')
                && !constants_1.galleryParams.has(child.name)) {
                child.setAttribute('name', 'invalid');
            }
            return super.insertAt(child, i);
        }
        cloneNode() {
            const [link, ...linkText] = this.cloneChildNodes();
            return debug_1.Shadow.run(() => {
                // @ts-expect-error abstract class
                const token = new GalleryImageToken(this.type.slice(0, -6), '', undefined, this.getAttribute('config'));
                token.firstChild.safeReplaceWith(link);
                token.safeAppend(linkText);
                return token;
            });
        }
    };
    return GalleryImageToken = _classThis;
})();
exports.GalleryImageToken = GalleryImageToken;
constants_1.classes['GalleryImageToken'] = __filename;
