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
exports.GalleryToken = void 0;
const lint_1 = require("../../util/lint");
const index_1 = __importDefault(require("../../index"));
const index_2 = require("./index");
const galleryImage_1 = require("../link/galleryImage");
const commentLine_1 = require("../nowiki/commentLine");
/* NOT FOR BROWSER */
const debug_1 = require("../../util/debug");
const constants_1 = require("../../util/constants");
const html_1 = require("../../util/html");
const cached_1 = require("../../mixin/cached");
/* NOT FOR BROWSER END */
/**
 * `<gallery>`
 * @classdesc `{childNodes: (GalleryImageToken|CommentLineToken|AstText)[]}`
 */
let GalleryToken = (() => {
    let _classSuper = index_2.MultiLineToken;
    let _instanceExtraInitializers = [];
    let _toHtmlInternal_decorators;
    return class GalleryToken extends _classSuper {
        static {
            const _metadata = typeof Symbol === "function" && Symbol.metadata ? Object.create(_classSuper[Symbol.metadata] ?? null) : void 0;
            _toHtmlInternal_decorators = [(0, cached_1.cached)()];
            __esDecorate(this, null, _toHtmlInternal_decorators, { kind: "method", name: "toHtmlInternal", static: false, private: false, access: { has: obj => "toHtmlInternal" in obj, get: obj => obj.toHtmlInternal }, metadata: _metadata }, null, _instanceExtraInitializers);
            if (_metadata) Object.defineProperty(this, Symbol.metadata, { enumerable: true, configurable: true, writable: true, value: _metadata });
        }
        /** all images / 所有图片 */
        get images() {
            return this.childNodes.filter((0, debug_1.isToken)('gallery-image'));
        }
        /* NOT FOR BROWSER END */
        /* PRINT ONLY */
        /**
         * image widths
         *
         * 图片宽度
         * @since v1.12.5
         */
        get widths() {
            LSP: return this.#getSize('widths');
        }
        /**
         * image heights
         *
         * 图片高度
         * @since v1.12.5
         */
        get heights() {
            LSP: return this.#getSize('heights');
        }
        /* PRINT ONLY END */
        /** @param inner 标签内部wikitext */
        constructor(inner, config, accum = []) {
            super(undefined, config, accum, {
                AstText: ':', GalleryImageToken: ':', CommentLineToken: ':',
            });
            __runInitializers(this, _instanceExtraInitializers);
            for (const line of inner?.split('\n') ?? []) {
                const matches = /^([^|]+)(?:\|(.*))?/u.exec(line);
                if (!matches) {
                    // @ts-expect-error abstract class
                    super.insertAt((line.trim() ? new commentLine_1.CommentLineToken(line, config, accum) : line));
                    continue;
                }
                const [, file, alt] = matches;
                if (this.#checkFile(file)) {
                    // @ts-expect-error abstract class
                    super.insertAt(new galleryImage_1.GalleryImageToken('gallery', file, alt, config, accum));
                }
                else {
                    // @ts-expect-error abstract class
                    super.insertAt(new commentLine_1.CommentLineToken(line, config, accum));
                }
            }
        }
        /**
         * 检查文件名是否有效
         * @param file 文件名
         */
        #checkFile(file) {
            return this.normalizeTitle(file, 6, { halfParsed: true, temporary: true, decode: true, page: '' }).valid;
        }
        /** @private */
        lint(start = this.getAbsoluteIndex(), re) {
            LINT: {
                const { top, left } = this.getRootNode().posFromIndex(start), errors = [], rule = 'no-ignored', { lintConfig } = index_1.default, s = ['Image', 'NoImage', 'Comment'].map(k => lintConfig.getSeverity(rule, `gallery${k}`));
                for (let i = 0; i < this.length; i++) {
                    const child = this.childNodes[i], str = child.toString(), { length } = str, trimmed = str.trim(), { type } = child, startLine = top + i, startCol = i ? 0 : left;
                    child.setAttribute('aIndex', start);
                    if (type === 'noinclude' && trimmed && !/^<!--.*-->$/u.test(trimmed)) {
                        let [severity] = s;
                        if (trimmed.startsWith('|')) {
                            [, severity] = s;
                        }
                        else if (trimmed.startsWith('<!--') || trimmed.endsWith('-->')) {
                            [, , severity] = s;
                        }
                        if (severity) {
                            const endIndex = start + length, e = {
                                rule,
                                message: index_1.default.msg('invalid-content', 'gallery'),
                                severity,
                                startIndex: start,
                                endIndex,
                                startLine,
                                endLine: startLine,
                                startCol,
                                endCol: startCol + length,
                            };
                            if (lintConfig.computeEditInfo) {
                                e.suggestions = [
                                    (0, lint_1.fixByRemove)(e),
                                    (0, lint_1.fixByComment)(e, str),
                                ];
                            }
                            errors.push(e);
                        }
                    }
                    else if (type !== 'noinclude' && type !== 'text') {
                        const childErrors = child.lint(start, re);
                        if (childErrors.length > 0) {
                            Array.prototype.push.apply(errors, childErrors);
                        }
                    }
                    start += length + 1;
                }
                return errors;
            }
        }
        /* PRINT ONLY */
        /**
         * 获取图片的宽度或高度
         * @param key `widths` 或 `heights`
         */
        #getSize(key) {
            LSP: {
                const widths = this.parentNode?.getAttr(key), mt = typeof widths === 'string' && /^(\d+)\s*(?:px)?$/u.exec(widths)?.[1];
                return mt && Number(mt) || 120;
            }
        }
        /** @private */
        json(_, depth, start = this.getAbsoluteIndex()) {
            LSP: {
                const json = super.json(undefined, depth, start);
                Object.assign(json, { widths: this.widths, heights: this.heights });
                return json;
            }
        }
        /* PRINT ONLY END */
        /* NOT FOR BROWSER */
        /**
         * Insert an image
         *
         * 插入图片
         * @param file image file name / 图片文件名
         * @param i position to be inserted at / 插入位置
         * @throws `SyntaxError` 非法的文件名
         */
        insertImage(file, i) {
            if (this.#checkFile(file)) {
                const token = debug_1.Shadow.run(() => 
                // @ts-expect-error abstract class
                new galleryImage_1.GalleryImageToken('gallery', file, undefined, this.getAttribute('config')));
                return this.insertAt(token, i);
            }
            throw new SyntaxError(`Invalid file name: ${file}`);
        }
        insertAt(token, i = this.length) {
            if (!debug_1.Shadow.running
                && (typeof token === 'string' ? token.trim() : !token.is('gallery-image'))) {
                throw new RangeError('Please do not insert invisible content into <gallery>!');
            }
            return super.insertAt(token, i);
        }
        /** @private */
        toHtmlInternal() {
            return (0, html_1.html)(this.childNodes.filter((0, debug_1.isToken)('gallery-image')), '\n');
        }
    };
})();
exports.GalleryToken = GalleryToken;
constants_1.classes['GalleryToken'] = __filename;
