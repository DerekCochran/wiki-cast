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
exports.ImageParameterToken = void 0;
const common_1 = require("@bhsd/common");
const string_1 = require("../util/string");
const lint_1 = require("../util/lint");
const constants_1 = require("../util/constants");
const debug_1 = require("../util/debug");
const noEscape_1 = require("../mixin/noEscape");
const index_1 = __importDefault(require("../index"));
const index_2 = require("./index");
/^(?:ftp:\/\/|\/\/|\0\d+m\x7F)/iu; // eslint-disable-line @typescript-eslint/no-unused-expressions
const getUrlLikeRegex = (0, common_1.getRegex)(protocol => new RegExp(String.raw `^(?:${protocol}|//|\0\d+m\x7F)`, 'iu'));
// eslint-disable-next-line @typescript-eslint/no-unused-expressions
/^(?:(?:ftp:\/\/|\/\/)(?:\[[\da-f:.]+\]|[^[\]<>"\t\n\p{Zs}])|\0\d+m\x7F)[^[\]<>"\0\t\n\p{Zs}]*$/iu;
const getUrlRegex = (0, common_1.getRegex)(protocol => new RegExp(String.raw `^(?:(?:${protocol}|//)${string_1.extUrlCharFirst}|\0\d+m\x7F)${string_1.extUrlChar}$`, 'iu'));
/* eslint-disable @typescript-eslint/no-unused-expressions */
/^(\s*)link=(.*)(?=$|\n)(\s*)$/u;
/^(\s*(?!\s))(.*)px(\s*)$/u;
/* eslint-enable @typescript-eslint/no-unused-expressions */
const getSyntaxRegex = (0, common_1.getRegex)(syntax => new RegExp(String.raw `^(\s*(?!\s))${syntax.replace('$1', '(.*)')}${syntax.endsWith('$1') ? '(?=$|\n)' : ''}(\s*)$`, 'u'));
/**
 * 获取网址
 * @param link 外链
 */
const getUrl = (link) => {
    if (!link) {
        return link;
    }
    else if (link.startsWith('//')) {
        link = `https:${link}`;
    }
    return new URL(link).href;
};
function validate(key, val, config, extOrType, halfParsed) {
    val = (0, string_1.removeComment)(val).trim();
    let value = val.replace(key === 'link' ? /\0\d+[tq]\x7F/gu : /\0\d+t\x7F/gu, '').trim();
    switch (key) {
        case 'width':
            return !value && Boolean(val) || /^(?:\d+x?|\d*x\d+)(?:\s*px)?$/u.test(value);
        case 'link': {
            const isGalleryImage = extOrType === 'gallery-image';
            if (!value) {
                return val;
            }
            else if (getUrlLikeRegex(config.protocol).test(value)) {
                return getUrlRegex(config.protocol).test(value) ? val : isGalleryImage;
            }
            else if (value.startsWith('[[') && value.endsWith(']]')) {
                value = value.slice(2, -2);
            }
            const title = index_1.default.normalizeTitle(value, 0, false, config, { halfParsed, decode: true, selfLink: true, page: '' });
            return title.valid ? title : isGalleryImage;
        }
        case 'lang':
            return (extOrType === 'svg' || extOrType === 'svgz') && !/[^a-z\d-]/u.test(value);
        case 'alt':
        case 'class':
        case 'manualthumb':
            return true;
        case 'page':
            return (extOrType === 'djvu' || extOrType === 'djv' || extOrType === 'pdf') && Number(value) > 0;
        default:
            return Boolean(value) && !isNaN(value);
    }
}
/**
 * image parameter
 *
 * 图片参数
 */
let ImageParameterToken = (() => {
    let _classDecorators = [noEscape_1.noEscape];
    let _classDescriptor;
    let _classExtraInitializers = [];
    let _classThis;
    let _classSuper = index_2.Token;
    var ImageParameterToken = class extends _classSuper {
        static { _classThis = this; }
        static {
            const _metadata = typeof Symbol === "function" && Symbol.metadata ? Object.create(_classSuper[Symbol.metadata] ?? null) : void 0;
            __esDecorate(null, _classDescriptor = { value: _classThis }, _classDecorators, { kind: "class", name: _classThis.name, metadata: _metadata }, null, _classExtraInitializers);
            ImageParameterToken = _classThis = _classDescriptor.value;
            if (_metadata) Object.defineProperty(_classThis, Symbol.metadata, { enumerable: true, configurable: true, writable: true, value: _metadata });
            __runInitializers(_classThis, _classExtraInitializers);
        }
        #syntax = '';
        /* NOT FOR BROWSER */
        #extension;
        /* NOT FOR BROWSER END */
        get type() {
            return 'image-parameter';
        }
        /**
         * thumbnail
         *
         * 缩略图
         * @since v1.29.0
         */
        get thumb() {
            LINT: return this.name === 'manualthumb'
                ? this.normalizeTitle(`File:${super.text().trim()}`, 6, { page: '' })
                : undefined;
        }
        /** image link / 图片链接 */
        get link() {
            LINT: {
                if (this.name !== 'link') {
                    return undefined;
                }
                const value = super.text().trim();
                return debug_1.Shadow.run(() => {
                    const config = this.getAttribute('config'), token = new index_2.Token(value, config);
                    token.parseOnce(0, this.getAttribute('include')).parseOnce();
                    if (/^\0\d+m\x7F/u.test(token.firstChild.toString())) {
                        return value;
                    }
                    const link = validate('link', value, config, this.parentNode?.type);
                    return link === true ? undefined : link;
                }, index_1.default);
            }
        }
        /* NOT FOR BROWSER */
        set link(value) {
            if (this.name === 'link') {
                this.setValue(value);
            }
        }
        set thumb(value) {
            if (this.name === 'manualthumb') {
                this.setValue(value);
            }
        }
        /** parameter value / 参数值 */
        get value() {
            return this.getValue();
        }
        set value(value) {
            this.setValue(value);
        }
        /** iamge size / 图片大小 */
        get size() {
            if (this.name === 'width') {
                const size = this.getValue().trim().replace(/px$/u, '').trim();
                if (!size.includes('{{')) {
                    const [width, height = ''] = size.split('x');
                    return { width, height };
                }
                return debug_1.Shadow.internal(() => {
                    const token = index_1.default.parseWithRef(size, this, 2, false), i = token.childNodes.findIndex(({ type, data }) => type === 'text' && data.includes('x'));
                    if (i === -1) {
                        return { width: size, height: '' };
                    }
                    const str = token.childNodes[i];
                    str.splitText(str.data.indexOf('x')).splitText(1);
                    return {
                        width: (0, string_1.text)(token.childNodes.slice(0, i + 1)),
                        height: (0, string_1.text)(token.childNodes.slice(i + 2)),
                    };
                }, index_1.default);
            }
            return undefined;
        }
        set size(size) {
            if (this.name === 'width') {
                this.setValue(size && size.width + (size.height && 'x') + size.height);
            }
        }
        /** image width / 图片宽度 */
        get width() {
            return this.size?.width;
        }
        set width(width) {
            if (this.name === 'width') {
                const { height } = this;
                this.setValue((width || '') + (height && 'x') + height);
            }
        }
        /** image height / 图片高度 */
        get height() {
            return this.size?.height;
        }
        set height(height) {
            if (this.name === 'width') {
                this.setValue(this.width + (height ? `x${height}` : ''));
            }
        }
        /* NOT FOR BROWSER END */
        /**
         * @param str 图片参数
         * @param extension 文件扩展名
         * @param type 父节点类型
         */
        constructor(str, extension, type, config, accum) {
            let mt;
            const regexes = Object.entries(config.img)
                .map(([syntax, param]) => [syntax, param, getSyntaxRegex(syntax)]), param = regexes.find(([, key, regex]) => {
                mt = regex.exec(str);
                return mt
                    && (mt.length !== 4
                        || validate(key, mt[2], config, key === 'link' ? type : extension, true) !== false);
            });
            // @ts-expect-error mt already assigned
            if (param && mt) {
                if (mt.length === 3) {
                    super(undefined, config, accum);
                    this.#syntax = str;
                }
                else {
                    super(mt[2], config, accum, {
                        'Stage-2': ':', '!HeadingToken': '',
                    });
                    this.#syntax = mt[1] + param[0] + mt[3];
                }
                this.setAttribute('name', param[1]);
                if (param[1] === 'alt') {
                    this.setAttribute('stage', constants_1.MAX_STAGE - 1);
                }
                return;
            }
            super(str, config.excludes.includes('list')
                ? config
                : {
                    ...config,
                    excludes: [...config.excludes, 'list'],
                }, accum);
            this.setAttribute('name', 'caption');
            this.setAttribute('stage', 7);
            /* NOT FOR BROWSER */
            this.#extension = extension;
        }
        /** @private */
        afterBuild() {
            if (this.parentNode?.is('gallery-image') && !constants_1.galleryParams.has(this.name)) {
                this.setAttribute('name', 'invalid');
            }
            super.afterBuild();
        }
        /** @private */
        toString(skip) {
            return this.#syntax ? this.#syntax.replace('$1', super.toString(skip)) : super.toString(skip);
        }
        /** @private */
        text() {
            return this.#syntax ? this.#syntax.replace('$1', super.text()).trim() : super.text().trim();
        }
        /** @private */
        isPlain() {
            return this.name === 'caption' || this.name === 'alt';
        }
        /** @private */
        getAttribute(key) {
            /* PRINT ONLY */
            if (key === 'invalid') {
                return (this.name === 'invalid');
            }
            /** PRINT ONLY END */
            return key === 'padding'
                ? Math.max(0, this.#syntax.indexOf('$1'))
                : super.getAttribute(key);
        }
        /** @private */
        lint(start = this.getAbsoluteIndex(), re) {
            LINT: {
                const errors = super.lint(start, re), { lintConfig } = index_1.default, { computeEditInfo, fix } = lintConfig, { link, name } = this, value = name === 'width' && this.getValue();
                if (name === 'invalid' || value && value.endsWith('px')) {
                    const rule = 'invalid-gallery', s = lintConfig.getSeverity(rule, 'parameter');
                    if (s) {
                        const e = (0, lint_1.generateForSelf)(this, { start }, rule, 'invalid-image-parameter', s);
                        if (computeEditInfo || fix) {
                            e.fix = value ? (0, lint_1.fixByRemove)(e, 0, value) : (0, lint_1.fixByRemove)(e, -1);
                        }
                        errors.push(e);
                    }
                }
                else if (name === 'link') {
                    if (link === undefined) {
                        const rule = 'invalid-gallery', s = lintConfig.getSeverity(rule, 'link');
                        if (s) {
                            const e = (0, lint_1.generateForSelf)(this, { start }, rule, 'invalid-gallery-link', s);
                            if (computeEditInfo) {
                                const rawLink = super.toString(), index = rawLink.indexOf('|'), before = rawLink.slice(0, index).trim();
                                if (index !== -1
                                    && !/[<{]/u.test(before)
                                    && typeof validate('link', before, this.getAttribute('config'), this.parentNode?.type) === 'string') {
                                    e.suggestions = [(0, lint_1.fixBySpace)(start + this.getRelativeIndex(0) + index)];
                                }
                            }
                            errors.push(e);
                        }
                    }
                    else if (typeof link === 'string') {
                        const rule = 'invalid-url', s = lintConfig.getSeverity(rule);
                        if (s && !this.querySelector('magic-word')) {
                            try {
                                getUrl(link);
                            }
                            catch {
                                errors.push((0, lint_1.generateForSelf)(this, { start }, rule, 'invalid-url', s));
                            }
                        }
                    }
                    else if (link.encoded) {
                        const rule = 'url-encoding', s = lintConfig.getSeverity(rule, 'file');
                        if (s) {
                            const e = (0, lint_1.generateForSelf)(this, { start }, rule, 'unnecessary-encoding', s);
                            if (computeEditInfo || fix) {
                                e.fix = (0, lint_1.fixByDecode)(e, this);
                            }
                            errors.push(e);
                        }
                    }
                }
                else if (name === 'manualthumb') {
                    const rule = 'invalid-gallery', s = lintConfig.getSeverity(rule, 'thumb');
                    if (s && !this.querySelector('arg,magic-word,template')) {
                        const { valid, ns, extension, 
                        /* NOT FOR BROWSER */
                        interwiki, } = this.thumb;
                        if (!valid || ns !== 6 || !constants_1.extensions.has(extension)
                            || interwiki) {
                            errors.push((0, lint_1.generateForSelf)(this, { start }, rule, 'invalid-thumb', s));
                        }
                    }
                }
                return errors;
            }
        }
        /** 是否是不可变参数 */
        #isVoid() {
            LINT: return this.#syntax && !this.#syntax.includes('$1');
        }
        /**
         * Get the parameter value
         *
         * 获取参数值
         */
        getValue() {
            LINT: return this.name === 'invalid' ? this.text() : this.#isVoid() || super.text();
        }
        /** @private */
        print() {
            PRINT: {
                if (this.#syntax) {
                    return `<span class="wpb-image-parameter${this.name === 'invalid' ? ' wpb-invalid' : ''}">${this.#syntax.replace('$1', `<span class="wpb-image-caption">${(0, string_1.print)(this.childNodes)}</span>`)}</span>`;
                }
                return super.print({ class: 'image-caption' });
            }
        }
        /* NOT FOR BROWSER */
        cloneNode() {
            const cloned = this.cloneChildNodes();
            return debug_1.Shadow.run(() => {
                // @ts-expect-error abstract class
                const token = new ImageParameterToken(this.#syntax.replace('$1', '1'), this.#extension, this.parentNode?.type, this.getAttribute('config'));
                token.safeReplaceChildren(cloned);
                return token;
            });
        }
        insertAt(token, i) {
            if (!debug_1.Shadow.running && this.#isVoid()) {
                throw new Error(`Image parameter ${this.name} does not accept custom input!`);
            }
            return super.insertAt(token, i);
        }
        /**
         * Set the parameter value
         *
         * 设置参数值
         * @param value parameter value / 参数值
         * @throws `Error` 无效参数
         */
        setValue(value = false) {
            const { name } = this;
            if (value === false) {
                this.remove();
                return;
            }
            else if (name === 'invalid') {
                throw new Error('Invalid image parameter!');
            }
            const type = this.#isVoid() ? 'Boolean' : 'String';
            if (typeof value !== type.toLowerCase()) { // eslint-disable-line valid-typeof
                this.typeError('setValue', type);
            }
            else if (value !== true) {
                const { childNodes } = index_1.default.parseWithRef(value, this, name === 'caption' ? undefined : 5);
                this.safeReplaceChildren(childNodes);
            }
        }
        /**
         * Get the URL
         *
         * 获取网址
         * @param articlePath article path / 条目路径
         * @since v1.11.0
         */
        getUrl(articlePath) {
            const { link } = this;
            if (!link) {
                return link;
            }
            return typeof link === 'string' ? getUrl(link) : link.getUrl(articlePath);
        }
    };
    return ImageParameterToken = _classThis;
})();
exports.ImageParameterToken = ImageParameterToken;
constants_1.classes['ImageParameterToken'] = __filename;
