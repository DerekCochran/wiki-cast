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
exports.FileToken = void 0;
const lint_1 = require("../../util/lint");
const constants_1 = require("../../util/constants");
const rect_1 = require("../../lib/rect");
const index_1 = __importDefault(require("../../index"));
const base_1 = require("./base");
const imageParameter_1 = require("../imageParameter");
/* NOT FOR BROWSER */
const string_1 = require("../../util/string");
const debug_1 = require("../../util/debug");
const title_1 = require("../../lib/title");
const cached_1 = require("../../mixin/cached");
const frame = new Map([
    ['manualthumb', 'Thumb'],
    ['frameless', 'Frameless'],
    ['framed', 'Frame'],
    ['thumbnail', 'Thumb'],
]), argTypes = new Set(['arg']), transclusion = new Set(['template', 'magic-word']), horizAlign = new Set(['left', 'right', 'center', 'none']), vertAlign = new Set(['baseline', 'sub', 'super', 'top', 'text-top', 'middle', 'bottom', 'text-bottom']);
/**
 * a more sophisticated string-explode function
 * @param str string to be exploded
 */
const explode = (str) => {
    if (str === undefined) {
        return [];
    }
    const regex = /-\{|\}-|\|/gu, exploded = [];
    let mt = regex.exec(str), depth = 0, lastIndex = 0;
    while (mt) {
        const { 0: match, index } = mt;
        if (match !== '|') {
            depth += match === '-{' ? 1 : -1;
        }
        else if (depth === 0) {
            exploded.push(str.slice(lastIndex, index));
            ({ lastIndex } = regex);
        }
        mt = regex.exec(str);
    }
    exploded.push(str.slice(lastIndex));
    return exploded;
};
/**
 * filter out the image parameters that are not of the specified type
 * @param args image parameter tokens
 * @param types token types to be filtered
 */
const filterArgs = (args, types) => args.filter(({ childNodes }) => !childNodes.some(node => node.text().trim() && types.has(node.type)));
/* NOT FOR BROWSER */
/**
 * check if a string is an integer
 * @param n string to be checked
 */
const isInteger = (n) => Boolean(n && !/\D/u.test(n));
/* NOT FOR BROWSER END */
/**
 * image
 *
 * 图片
 * @classdesc `{childNodes: [AtomToken, ...ImageParameterToken[]]}`
 */
let FileToken = (() => {
    let _classSuper = base_1.LinkBaseToken;
    let _instanceExtraInitializers = [];
    let _toHtmlInternal_decorators;
    return class FileToken extends _classSuper {
        static {
            const _metadata = typeof Symbol === "function" && Symbol.metadata ? Object.create(_classSuper[Symbol.metadata] ?? null) : void 0;
            _toHtmlInternal_decorators = [(0, cached_1.cached)()];
            __esDecorate(this, null, _toHtmlInternal_decorators, { kind: "method", name: "toHtmlInternal", static: false, private: false, access: { has: obj => "toHtmlInternal" in obj, get: obj => obj.toHtmlInternal }, metadata: _metadata }, null, _instanceExtraInitializers);
            if (_metadata) Object.defineProperty(this, Symbol.metadata, { enumerable: true, configurable: true, writable: true, value: _metadata });
        }
        /* NOT FOR BROWSER END */
        get type() {
            return 'file';
        }
        /**
         * file extension
         *
         * 扩展名
         * @since v1.5.3
         */
        get extension() {
            LSP: return this.getAttribute('title').extension;
        }
        /* NOT FOR BROWSER */
        /** image link / 图片链接 */
        get link() {
            return this.getArg('link')?.link ?? super.link;
        }
        set link(value) {
            this.setValue('link', value);
        }
        /** image size / 图片大小 */
        get size() {
            const fr = this.getFrame();
            return fr === 'framed' || fr instanceof title_1.Title ? undefined : this.getArg('width')?.size;
        }
        set size(size) {
            this.setValue('width', size && size.width + (size.height && 'x') + size.height);
        }
        /** image width / 图片宽度 */
        get width() {
            return this.is('gallery-image')
                ? this.parentNode?.widths.toString()
                : this.size?.width;
        }
        set width(width) {
            const arg = this.getArg('width');
            if (arg) {
                arg.width = width;
            }
            else {
                this.setValue('width', width);
            }
        }
        /** image height / 图片高度 */
        get height() {
            return this.is('gallery-image')
                ? this.parentNode?.heights.toString()
                : this.size?.height;
        }
        set height(height) {
            const arg = this.getArg('width');
            if (arg) {
                arg.height = height;
            }
            else {
                this.setValue('width', height && `x${height}`);
            }
        }
        /* NOT FOR BROWSER END */
        /**
         * @param link 文件名
         * @param text 图片参数
         * @param delimiter `|`
         * @param type 节点类型
         */
        constructor(link, text, config, accum = [], delimiter = '|', type) {
            super(link, undefined, config, accum, delimiter);
            __runInitializers(this, _instanceExtraInitializers);
            /* NOT FOR BROWSER */
            this.setAttribute('acceptable', { AtomToken: 0, ImageParameterToken: '1:' });
            /* NOT FOR BROWSER END */
            const { extension } = this.getTitle(true, true);
            /-\{|\}-|\|/gu; // eslint-disable-line @typescript-eslint/no-unused-expressions
            this.safeAppend(explode(text).map(
            // @ts-expect-error abstract class
            (part) => new imageParameter_1.ImageParameterToken(part, extension, type, config, accum)));
        }
        /** @private */
        lint(start = this.getAbsoluteIndex(), re) {
            LINT: {
                const errors = super.lint(start, re), args = filterArgs(this.getAllArgs(), argTypes), keys = [...new Set(args.map(({ name }) => name))], frameKeys = keys.filter(key => frame.has(key)), horizAlignKeys = keys.filter(key => horizAlign.has(key)), vertAlignKeys = keys.filter(key => vertAlign.has(key)), [fr] = frameKeys, unscaled = fr === 'framed' || fr === 'manualthumb', rect = new rect_1.BoundingRect(this, start), { lintConfig } = index_1.default, { computeEditInfo, fix } = lintConfig, { ns, extension, 
                /* NOT FOR BROWSER */
                interwiki, } = this.getAttribute('title'), { firstChild } = this;
                let rule = 'nested-link', s = lintConfig.getSeverity(rule, 'file');
                if (s
                    && constants_1.extensions.has(extension)
                    && this.isInside('ext-link-text')
                    && this.getValue('link')?.trim() !== '') {
                    const e = (0, lint_1.generateForSelf)(this, rect, rule, 'link-in-extlink', s);
                    if (computeEditInfo || fix) {
                        const link = this.getArg('link');
                        if (link) {
                            const from = start + link.getRelativeIndex();
                            e.fix = {
                                desc: index_1.default.msg('delink'),
                                range: [from, from + link.toString().length],
                                text: 'link=',
                            };
                        }
                        else {
                            e.fix = (0, lint_1.fixByInsert)(e.endIndex - 2, 'delink', '|link=');
                        }
                    }
                    errors.push(e);
                }
                rule = 'invalid-gallery';
                s = lintConfig.getSeverity(rule, 'extension');
                if (s && ns === 6 && !extension && !firstChild.querySelector('arg,magic-word,template')
                    && !interwiki) {
                    errors.push((0, lint_1.generateForSelf)(this, rect, rule, 'missing-extension', s));
                }
                s = lintConfig.getSeverity(rule, 'parameter');
                if (s && unscaled) {
                    for (const arg of args.filter(({ name }) => name === 'width')) {
                        const e = (0, lint_1.generateForChild)(arg, rect, rule, 'invalid-image-parameter', s);
                        if (computeEditInfo || fix) {
                            e.fix = (0, lint_1.fixByRemove)(e, -1);
                        }
                        errors.push(e);
                    }
                }
                rule = 'blank-alt';
                s = lintConfig.getSeverity(rule);
                if (s) {
                    const alt = this.getArg('alt');
                    if (alt?.getValue() === '') {
                        const e = (0, lint_1.generateForChild)(alt, rect, rule, 'blank-alt', s);
                        if (computeEditInfo || fix) {
                            e.fix = (0, lint_1.fixByRemove)(e, -1);
                        }
                        errors.push(e);
                    }
                }
                if (args.length === keys.length
                    && frameKeys.length < 2
                    && horizAlignKeys.length < 2
                    && vertAlignKeys.length < 2) {
                    return errors;
                }
                rule = 'no-duplicate';
                const severities = ['unknownImageParameter', 'imageParameter'].map(k => lintConfig.getSeverity(rule, k));
                /**
                 * 图片参数到语法错误的映射
                 * @param tokens 图片参数节点
                 * @param msg 消息键
                 * @param p1 替换$1
                 * @param severity 错误等级
                 */
                const generate = (tokens, msg, p1, severity = true) => tokens.map(arg => {
                    s = severities[Number(typeof severity === 'function' ? severity(arg) : severity)];
                    if (!s) {
                        return false;
                    }
                    /** `conflicting-image-parameter`或`duplicate-image-parameter` */
                    const e = (0, lint_1.generateForChild)(arg, rect, rule, index_1.default.msg(`${msg}-image-parameter`, p1), s);
                    if (computeEditInfo) {
                        e.suggestions = [(0, lint_1.fixByRemove)(e, -1)];
                    }
                    return e;
                }).filter((e) => e !== false);
                for (const key of keys) {
                    if (key === 'invalid' || key === 'width' && unscaled) {
                        continue;
                    }
                    const isCaption = key === 'caption';
                    let relevantArgs = args.filter(({ name }) => name === key);
                    if (isCaption) {
                        relevantArgs = [
                            ...relevantArgs.slice(0, -1).filter(arg => arg.text()),
                            ...relevantArgs.slice(-1),
                        ];
                    }
                    if (relevantArgs.length > 1) {
                        let severity = !isCaption || !extension || constants_1.extensions.has(extension);
                        if (isCaption && severity) {
                            const plainArgs = filterArgs(relevantArgs, transclusion);
                            severity = plainArgs.length > 1 && ((arg) => plainArgs.includes(arg));
                        }
                        Array.prototype.push.apply(errors, generate(relevantArgs, 'duplicate', key, severity));
                    }
                }
                if (frameKeys.length > 1) {
                    Array.prototype.push.apply(errors, generate(args.filter(({ name }) => frame.has(name)), 'conflicting', 'frame'));
                }
                if (horizAlignKeys.length > 1) {
                    Array.prototype.push.apply(errors, generate(args.filter(({ name }) => horizAlign.has(name)), 'conflicting', 'horizontal-alignment'));
                }
                if (vertAlignKeys.length > 1) {
                    Array.prototype.push.apply(errors, generate(args.filter(({ name }) => vertAlign.has(name)), 'conflicting', 'vertical-alignment'));
                }
                return errors;
            }
        }
        /**
         * Get all image parameter tokens
         *
         * 获取所有图片参数节点
         */
        getAllArgs() {
            LINT: return this.childNodes.slice(1);
        }
        /**
         * Get image parameters with the specified name
         *
         * 获取指定图片参数
         * @param key parameter name / 参数名
         */
        getArgs(key) {
            LINT: return this.getAllArgs().filter(({ name }) => key === name);
        }
        /**
         * Get the effective image parameter with the specified name
         *
         * 获取生效的指定图片参数
         * @param key parameter name / 参数名
         */
        getArg(key) {
            LINT: {
                const args = this.getArgs(key);
                return args[key === 'manualthumb' ? 0 : args.length - 1];
            }
        }
        /**
         * Get the effective image parameter value
         *
         * 获取生效的指定图片参数值
         * @param key parameter name / 参数名
         */
        getValue(key) {
            LINT: return this.getArg(key)?.getValue();
        }
        /** @private */
        json(_, depth, start = this.getAbsoluteIndex()) {
            LSP: {
                const json = super.json(undefined, depth, start), { extension } = this;
                if (extension) {
                    json['extension'] = extension;
                }
                return json;
            }
        }
        /* NOT FOR BROWSER */
        /**
         * 获取特定类型的图片属性参数节点
         * @param keys 接受的参数名
         * @param type 类型名
         */
        #getTypedArgs(keys, type) {
            const args = this.getAllArgs().filter(({ name }) => keys.has(name));
            if (args.length > 1) {
                index_1.default.warn(`The image ${this.name} has ${args.length} ${type} parameters. Only the last ${args[0].name} will take effect!`);
            }
            return args;
        }
        /**
         * Get image frame parameter tokens
         *
         * 获取图片框架属性参数节点
         */
        getFrameArgs() {
            return this.#getTypedArgs(frame, 'frame');
        }
        /**
         * Get image horizontal alignment parameter tokens
         *
         * 获取图片水平对齐参数节点
         */
        getHorizAlignArgs() {
            return this.#getTypedArgs(horizAlign, 'horizontal-align');
        }
        /**
         * Get image vertical alignment parameter tokens
         *
         * 获取图片垂直对齐参数节点
         */
        getVertAlignArgs() {
            return this.#getTypedArgs(vertAlign, 'vertical-align');
        }
        /**
         * Get the effective image frame paremter value
         *
         * 获取生效的图片框架属性参数
         * @since v1.11.0
         */
        getFrame() {
            const [arg] = this.getFrameArgs(), val = arg?.name;
            return val === 'manualthumb' ? arg.thumb : val;
        }
        /**
         * Get the effective image horizontal alignment parameter value
         *
         * 获取生效的图片水平对齐参数
         * @since v1.11.0
         */
        getHorizAlign() {
            return this.getHorizAlignArgs()[0]?.name;
        }
        /**
         * Get the effective image vertical alignment parameter value
         *
         * 获取生效的图片垂直对齐参数
         * @since v1.11.0
         */
        getVertAlign() {
            return this.getVertAlignArgs()[0]?.name;
        }
        /**
         * Check if the image contains the specified parameter
         *
         * 是否具有指定图片参数
         * @param key parameter name / 参数名
         */
        hasArg(key) {
            return this.getArgs(key).length > 0;
        }
        /**
         * Remove the specified image parameter
         *
         * 移除指定图片参数
         * @param key parameter name / 参数名
         */
        removeArg(key) {
            for (const token of this.getArgs(key)) {
                this.removeChild(token);
            }
        }
        /**
         * Get all image parameter names
         *
         * 获取图片参数名
         */
        getKeys() {
            return new Set(this.getAllArgs().map(({ name }) => name));
        }
        /**
         * Get the image parameter values with the specified name
         *
         * 获取指定的图片参数值
         * @param key parameter name / 参数名
         */
        getValues(key) {
            return this.getArgs(key).map(token => token.getValue());
        }
        /**
         * Set the image parameter
         *
         * 设置图片参数
         * @param key parameter name / 参数名
         * @param value parameter value / 参数值
         * @throws `RangeError` 未定义的图片参数
         */
        setValue(key, value = false) {
            if (value === false) {
                this.removeArg(key);
                return;
            }
            const token = this.getArg(key);
            if (token) {
                token.setValue(value);
                return;
            }
            const config = this.getAttribute('config'), syntax = key === 'caption' ? '$1' : Object.entries(config.img).find(([, name]) => name === key)?.[0];
            /* c8 ignore next 3 */
            if (syntax === undefined) {
                throw new RangeError(`Unknown image parameter: ${key}`);
            }
            const free = syntax.includes('$1');
            /* c8 ignore next 3 */
            if (value === true && free) {
                this.typeError('setValue', 'String');
            }
            const parameter = debug_1.Shadow.run(() => 
            // @ts-expect-error abstract class
            new imageParameter_1.ImageParameterToken(syntax.replace('$1', key === 'width' ? '1' : ''), this.extension, this.type, config));
            if (free) {
                const { childNodes } = index_1.default.parseWithRef(value, this);
                parameter.safeReplaceChildren(childNodes);
            }
            this.insertAt(parameter);
        }
        /* c8 ignore start */
        /**
         * @override
         * @throws `Error` 不适用于图片
         */
        setLinkText() {
            throw new Error('LinkBaseToken.setLinkText method is not applicable to images!');
        }
        /* c8 ignore stop */
        /** @private */
        toHtmlInternal(opt) {
            const { link, width, height, type } = this, file = this.getAttribute('title'), fr = this.getFrame(), manual = fr instanceof title_1.Title, visibleCaption = manual || fr === 'thumbnail' || fr === 'framed' || type === 'gallery-image', caption = this.getArg('caption')?.toHtmlInternal({
                ...opt,
                nowrap: true,
            }) ?? '', titleFromCaption = visibleCaption && type !== 'gallery-image' ? '' : (0, string_1.sanitizeAlt)(caption), hasLink = manual || link !== file, title = titleFromCaption || (hasLink && typeof link !== 'string' ? link.getTitleAttr() : ''), titleAttr = title && ` title="${title}"`, alt = (0, string_1.sanitizeAlt)(this.getArg('alt')?.toHtmlInternal({
                ...opt,
                nowrap: true,
            })) ?? titleFromCaption, horiz = this.getHorizAlign() ?? '', vert = this.getVertAlign() ?? '', className = `${horiz ? `mw-halign-${horiz}` : vert && `mw-valign-${vert}`}${this.getValue('border') ? ' mw-image-border' : ''} ${(0, string_1.sanitizeAlt)(this.getValue('class')) ?? ''}`.trim(), classAttr = className && ` class="${className}"`, hasWidth = isInteger(width), hasHeight = isInteger(height);
            let src;
            try {
                src = manual ? fr.getFileUrl() : file.getFileUrl(hasWidth && Number(width), hasHeight && Number(height));
            }
            catch {
                return '';
            }
            const img = `<img${alt && ` alt="${alt}"`} src="${src}" decoding="async" class="mw-file-element"${hasWidth ? ` width="${width}"` : ''}${hasHeight ? ` height="${height}"` : ''}>`;
            let href = '';
            if (link) {
                try {
                    href = typeof link === 'string' ? this.getArg('link').getUrl() : link.getUrl();
                    if (link === file) {
                        const lang = this.getValue('lang'), page = this.getValue('page');
                        if (lang) {
                            href += `?lang=${lang}`;
                        }
                        else if (page) {
                            href += `?page=${page}`;
                        }
                    }
                }
                catch { }
            }
            const a = link
                ? `<a${href && ` href="${href}"`}${hasLink ? '' : ' class="mw-file-description"'}${titleAttr}${typeof link === 'string' ? ' rel="nofollow"' : ''}>${img}</a>`
                : `<span${titleAttr}>${img}</span>`;
            if (type !== 'gallery-image') {
                return horiz || visibleCaption
                    ? `<figure${classAttr} typeof="mw:File${fr ? `/${manual ? 'Thumb' : frame.get(fr)}` : ''}">${a}<figcaption>${caption}</figcaption></figure>`
                    : `<span${classAttr}>${a}</span>`;
            }
            const parent = this.parentNode, mode = parent?.parentNode?.getAttr('mode'), nolines = typeof mode === 'string' && mode.toLowerCase() === 'nolines', padding = nolines ? 0 : 30;
            return `\t<li class="gallerybox" style="width: ${Number(width) + padding + 5}px">\n\t\t<div class="thumb" style="width: ${Number(width) + padding}px${nolines ? '' : `; height: ${Number(height) + padding}px`}"><span>${a}</span></div>\n\t\t<div class="gallerytext">${parent?.parentNode?.hasAttr('showfilename')
                ? `<a href="${file.getUrl()}" class="galleryfilename galleryfilename-truncate" title="${file.title}">${file.main}</a>\n`
                : ''}${caption}</div>\n\t</li>`;
        }
    };
})();
exports.FileToken = FileToken;
constants_1.classes['FileToken'] = __filename;
