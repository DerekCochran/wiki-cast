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
exports.TranscludeToken = void 0;
const string_1 = require("../util/string");
const lint_1 = require("../util/lint");
const debug_1 = require("../util/debug");
const constants_1 = require("../util/constants");
const rect_1 = require("../lib/rect");
const gapped_1 = require("../mixin/gapped");
const noEscape_1 = require("../mixin/noEscape");
const index_1 = __importDefault(require("../index"));
const index_2 = require("./index");
const parameter_1 = require("./parameter");
const atom_1 = require("./atom");
const syntax_1 = require("./syntax");
/* NOT FOR BROWSER */
const cached_1 = require("../mixin/cached");
/* NOT FOR BROWSER END */
/**
 * template or magic word
 *
 * 模板或魔术字
 * @classdesc `{childNodes: [AtomToken|SyntaxToken, ...AtomToken[], ...ParameterToken[]]}`
 */
let TranscludeToken = (() => {
    let _classDecorators = [noEscape_1.noEscape, (0, gapped_1.gapped)()];
    let _classDescriptor;
    let _classExtraInitializers = [];
    let _classThis;
    let _classSuper = index_2.Token;
    let _instanceExtraInitializers = [];
    let _toHtmlInternal_decorators;
    var TranscludeToken = class extends _classSuper {
        static { _classThis = this; }
        static {
            const _metadata = typeof Symbol === "function" && Symbol.metadata ? Object.create(_classSuper[Symbol.metadata] ?? null) : void 0;
            _toHtmlInternal_decorators = [(0, cached_1.cached)()];
            __esDecorate(this, null, _toHtmlInternal_decorators, { kind: "method", name: "toHtmlInternal", static: false, private: false, access: { has: obj => "toHtmlInternal" in obj, get: obj => obj.toHtmlInternal }, metadata: _metadata }, null, _instanceExtraInitializers);
            __esDecorate(null, _classDescriptor = { value: _classThis }, _classDecorators, { kind: "class", name: _classThis.name, metadata: _metadata }, null, _classExtraInitializers);
            TranscludeToken = _classThis = _classDescriptor.value;
            if (_metadata) Object.defineProperty(_classThis, Symbol.metadata, { enumerable: true, configurable: true, writable: true, value: _metadata });
            __runInitializers(_classThis, _classExtraInitializers);
        }
        modifier = (__runInitializers(this, _instanceExtraInitializers), '');
        #type = 'template';
        #colon = ':';
        #raw = false;
        #args = new Map();
        #anonCount = 0;
        // @ts-expect-error lazy initialization
        #title;
        /* NOT FOR BROWSER */
        #keys = new Set();
        /* NOT FOR BROWSER END */
        get type() {
            return this.#type;
        }
        /**
         * module name
         *
         * 模块名
         * @since v1.21.0
         */
        get module() {
            LSP: return this.type === 'magic-word' && this.name === 'invoke' ? this.#getTitle().title : undefined;
        }
        /**
         * function name
         *
         * 函数名
         * @since v1.21.2
         */
        get function() {
            LSP: return this.type === 'magic-word' && this.name === 'invoke'
                // eslint-disable-next-line @typescript-eslint/no-unnecessary-condition
                ? this.childNodes[2]?.text().trim()
                : undefined;
        }
        /* NOT FOR BROWSER */
        /** whether to contain duplicated parameters / 是否存在重复参数 */
        get duplication() {
            return this.isTemplate() && Boolean(this.hasDuplicatedArgs());
        }
        set duplication(duplication) {
            if (this.duplication && !duplication) {
                this.fixDuplication();
            }
        }
        /**
         * number of anonymous parameters
         *
         * 匿名参数的个数
         * @since v1.36.0
         */
        get anonCount() {
            return this.#anonCount;
        }
        /* NOT FOR BROWSER END */
        /**
         * @param title 模板标题或魔术字
         * @param parts 参数各部分
         * @throws `SyntaxError` 非法的模板名称
         */
        constructor(title, parts, config, accum = []) {
            let heading;
            const m = /^(?:\s|\0\d+[cn]\x7F)*\0(\d+)h\x7F(?:\s|\0\d+[cn]\x7F)*/u.exec(title);
            if (m) {
                heading = Number(m[1]);
                title = title.replace(`\0${heading}h\x7F`, accum[heading].toString().replace(/^\n/u, ''));
            }
            super(undefined, config, accum, {
                AtomToken: 0, SyntaxToken: 0, ParameterToken: '1:',
            });
            const { parserFunction, variable, functionHook } = config, argSubst = /^(?:\s|\0\d+[cn]\x7F)*\0\d+s\x7F/u.exec(title)?.[0];
            if (argSubst) {
                this.setAttribute('modifier', argSubst);
                title = title.slice(argSubst.length);
            }
            else if (title.includes(':')) {
                const [modifier, ...arg] = title.split(':'), [mt] = /^(?:\s|\0\d+[cn]\x7F)*/u.exec(arg[0] ?? '');
                if (this.setModifier(`${modifier}:${mt}`)) {
                    title = arg.join(':').slice(mt.length);
                }
            }
            const colon = title.search(/[:：]/u), fullWidth = title[colon] === '：', isFunction = colon !== -1;
            if (isFunction || parts.length === 0 && !this.#raw) {
                const magicWord = isFunction ? title.slice(0, colon) : title, arg = isFunction && title.slice(colon + 1), cleaned = (0, string_1.removeComment)(magicWord), name = isFunction
                    ? cleaned.slice(cleaned.search(/\S/u)) + (fullWidth ? '：' : '')
                    : cleaned.trim(), [, isSensitive, canonicalName] = (0, debug_1.getMagicWordInfo)(name, parserFunction), isFunc = !('functionHook' in config) || functionHook.includes(canonicalName), isVar = variable.includes(canonicalName);
                if (isFunction ? canonicalName && isFunc : isVar) {
                    this.setAttribute('name', canonicalName);
                    this.#type = 'magic-word';
                    if (fullWidth) {
                        this.#colon = '：';
                    }
                    /^\s*uc\s*$/iu; // eslint-disable-line @typescript-eslint/no-unused-expressions
                    const token = new syntax_1.SyntaxToken(magicWord, new RegExp(String.raw `^\s*${name}\s*$`, isSensitive ? 'u' : 'iu'), 'magic-word-name', config, accum, { 'Stage-1': ':', '!ExtToken': '' });
                    super.insertAt(token);
                    if (arg !== false) {
                        parts.unshift([arg]);
                    }
                    if (this.name === 'invoke') {
                        /* NOT FOR BROWSER */
                        this.setAttribute('acceptable', { SyntaxToken: 0, AtomToken: '1:3', ParameterToken: '3:' });
                        this.protectChildren('1:3');
                        /* NOT FOR BROWSER END */
                        for (let i = 0; i < 2; i++) {
                            const part = parts.shift();
                            if (!part) {
                                break;
                            }
                            const invoke = new atom_1.AtomToken(part.join('='), `invoke-${i ? 'function' : 'module'}`, config, accum, { 'Stage-2': ':', '!ExtToken': '', '!HeadingToken': '' });
                            super.insertAt(invoke);
                        }
                    }
                }
            }
            if (this.type === 'template') {
                const name = (0, string_1.removeComment)(title).trim();
                if (!this.normalizeTitle(name, 10, { halfParsed: true, temporary: true }).valid) {
                    accum.pop();
                    /* NOT FOR BROWSER */
                    index_1.default.debug(`Invalid template name: ${(0, string_1.noWrap)(name)}`);
                    /* NOT FOR BROWSER END */
                    throw new SyntaxError('Invalid template name');
                }
                const token = new atom_1.AtomToken(title, 'template-name', config, accum, {
                    'Stage-2': ':', '!ExtToken': '', '!HeadingToken': '',
                });
                super.insertAt(token);
            }
            if (typeof heading === 'number') {
                // @ts-expect-error sparse array
                accum[heading] = undefined;
            }
            const templateLike = this.isTemplate();
            let i = 1;
            for (let j = 0; j < parts.length; j++) {
                const part = parts[j];
                if (!(templateLike || this.name === 'switch' && j > 0 || this.name === 'tag' && j > 1)) {
                    part[0] = part.join('=');
                    part.length = 1;
                }
                if (part.length === 1) {
                    part.unshift(i);
                    i++;
                }
                // @ts-expect-error abstract class
                this.insertAt(new parameter_1.ParameterToken(...part, config, accum));
            }
            /* PRINT ONLY */
            this.seal('modifier');
            /* PRINT ONLY END */
            /* NOT FOR BROWSER */
            this.protectChildren(0);
        }
        /**
         * Set the transclusion modifier
         *
         * 设置引用修饰符
         * @param modifier transclusion modifier / 引用修饰符
         */
        setModifier(modifier) {
            const [, , raw, subst] = this.getAttribute('config').parserFunction, lcModifier = (0, string_1.removeComment)(modifier).trim();
            if (modifier && !lcModifier.endsWith(':')) {
                return false;
            }
            const magicWord = lcModifier.slice(0, -1).toLowerCase(), isRaw = raw.includes(magicWord), isSubst = subst.includes(magicWord);
            if ((this.#raw ? isRaw : isSubst || !modifier)
                || (debug_1.Shadow.running || this.length > 1) && (isRaw || isSubst || !modifier)) {
                this.setAttribute('modifier', modifier);
                this.#raw = isRaw;
                return Boolean(modifier);
            }
            return false;
        }
        /**
         * Check if it is a template or a module
         *
         * 是否是模板或模块
         */
        isTemplate() {
            return this.type === 'template' || this.name === 'invoke';
        }
        /** 获取模板或模块名 */
        #getTitle() {
            const isTemplate = this.type === 'template', title = this.normalizeTitle((isTemplate ? '' : 'Module:') + (0, string_1.removeComment)(this.childNodes[isTemplate ? 0 : 1].text()), 10, { temporary: true, ...!isTemplate && { page: '' } });
            /* NOT FOR BROWSER */
            title.fragment = undefined;
            /* NOT FOR BROWSER END */
            return title;
        }
        /** @private */
        afterBuild() {
            if (this.modifier.includes('\0')) {
                this.setAttribute('modifier', this.buildFromStr(this.modifier, constants_1.BuildMethod.String));
            }
            super.afterBuild();
            if (this.isTemplate()) {
                const isTemplate = this.type === 'template';
                if (isTemplate) {
                    this.#title = this.#getTitle();
                    this.setAttribute('name', this.#title.title);
                }
                /* NOT FOR BROWSER */
                /**
                 * 当事件bubble到`parameter`时，将`oldKey`和`newKey`保存进AstEventData。
                 * 当继续bubble到`template`时，处理并删除`oldKey`和`newKey`。
                 * @implements
                 */
                const transcludeListener = ({ prevTarget }, data) => {
                    const { oldKey, newKey } = data;
                    if (typeof oldKey === 'string') {
                        delete data.oldKey;
                        delete data.newKey;
                    }
                    if (prevTarget === this.firstChild && isTemplate) {
                        this.#title = this.#getTitle();
                        this.setAttribute('name', this.#title.title);
                    }
                    else if (oldKey !== newKey && prevTarget instanceof parameter_1.ParameterToken) {
                        const oldArgs = this.getArgs(oldKey, false, false);
                        oldArgs.delete(prevTarget);
                        this.getArgs(newKey, false, false).add(prevTarget);
                        this.#keys.add(newKey);
                        if (oldArgs.size === 0) {
                            this.#keys.delete(oldKey);
                        }
                    }
                };
                this.addEventListener(['remove', 'insert', 'replace', 'text'], transcludeListener);
            }
        }
        /** @private */
        toString(skip) {
            const { childNodes, length, firstChild, modifier, type } = this;
            return `{{${modifier}${type === 'magic-word'
                ? firstChild.toString(skip)
                    + (length === 1 ? '' : this.#colon)
                    + childNodes.slice(1).map(child => child.toString(skip)).join('|')
                : super.toString(skip, '|')}}}`;
        }
        /** @private */
        text() {
            const { childNodes, length, firstChild, modifier, type, name } = this;
            return type === 'magic-word' && name === 'vardefine'
                ? ''
                : `{{${modifier}${type === 'magic-word'
                    ? firstChild.text()
                        + (length === 1 ? '' : this.#colon)
                        + (0, string_1.text)(childNodes.slice(1), '|')
                    : super.text('|')}}}`;
        }
        /** @private */
        getAttribute(key) {
            switch (key) {
                case 'padding':
                    return this.modifier.length + 2;
                case 'title':
                    return this.#title;
                case 'colon':
                    return this.#colon;
                /* PRINT ONLY */
                case 'invalid':
                    PRINT: return (this.type === 'magic-word' && this.name === 'invoke'
                        && (this.length === 2 || !this.#getTitle().valid));
                /* PRINT ONLY END */
                /* NOT FOR BROWSER */
                case 'keys':
                    return this.#keys;
                /* NOT FOR BROWSER END */
                default:
                    return super.getAttribute(key);
            }
        }
        /** @private */
        lint(start = this.getAbsoluteIndex(), re) {
            LINT: {
                const errors = super.lint(start, re);
                if (!this.isTemplate()) {
                    return errors;
                }
                const { type, childNodes, length } = this, rect = new rect_1.BoundingRect(this, start), { lintConfig } = index_1.default, { computeEditInfo } = lintConfig, invoke = type === 'magic-word';
                let rule = 'no-ignored', s = lintConfig.getSeverity(rule, 'fragment');
                if (invoke && !this.#getTitle().valid) {
                    rule = 'invalid-invoke';
                    s = lintConfig.getSeverity(rule, 'name');
                    if (s) {
                        errors.push((0, lint_1.generateForChild)(childNodes[1], rect, rule, 'illegal-module', s));
                    }
                }
                else if (s) {
                    const child = childNodes[invoke ? 1 : 0], i = child.childNodes
                        .findIndex(c => c.type === 'text' && (0, string_1.decodeHtml)(c.data).includes('#')), textNode = child.childNodes[i];
                    if (textNode) {
                        const e = (0, lint_1.generateForChild)(child, rect, rule, 'useless-fragment', s);
                        if (computeEditInfo) {
                            e.suggestions = [
                                (0, lint_1.fixByRemove)(e, child.getRelativeIndex(i) + textNode.data.indexOf('#')),
                            ];
                        }
                        errors.push(e);
                    }
                }
                rule = 'invalid-invoke';
                s = lintConfig.getSeverity(rule, 'function');
                if (s && invoke && length === 2) {
                    errors.push((0, lint_1.generateForSelf)(this, rect, rule, 'missing-function', s));
                    return errors;
                }
                rule = 'no-duplicate';
                s = lintConfig.getSeverity(rule, 'parameter');
                if (s) {
                    const duplicatedArgs = this.getDuplicatedArgs()
                        .filter(([, parameter]) => !parameter[0].querySelector('ext')), msg = 'duplicate-parameter';
                    for (const [, args] of duplicatedArgs) {
                        Array.prototype.push.apply(errors, args.map(arg => {
                            const e = (0, lint_1.generateForChild)(arg, rect, rule, msg, s);
                            if (computeEditInfo) {
                                e.suggestions = [(0, lint_1.fixByRemove)(e, -1)];
                            }
                            return e;
                        }));
                    }
                }
                return errors;
            }
        }
        #handleAnonArgChange(addedToken, append) {
            /* NOT FOR BROWSER */
            const removing = typeof addedToken === 'number';
            /* NOT FOR BROWSER END */
            /* eslint-disable @stylistic/operator-linebreak */
            this.#anonCount +=
                removing ?
                    -1 :
                    1;
            /* eslint-enable @stylistic/operator-linebreak */
            /* NOT FOR BROWSER */
            const maxAnon = String(this.#anonCount + (removing ? 1 : 0));
            if (!removing) {
                this.#keys.add(maxAnon);
            }
            else if (!this.hasArg(maxAnon, true)) {
                this.#keys.delete(maxAnon);
            }
            let args;
            /* NOT FOR BROWSER END */
            let i;
            if (append) {
                i = this.#anonCount - 1;
                /* NOT FOR BROWSER */
            }
            else {
                args = this.getAnonArgs();
                i = removing ? addedToken - 1 : args.indexOf(addedToken);
            }
            for (; i < this.#anonCount; i++) {
                /* NOT FOR BROWSER END */
                const token = 
                /* eslint-disable @stylistic/operator-linebreak, unicorn/no-negated-condition */
                !append ?
                    args[i] :
                    addedToken, 
                /* eslint-enable @stylistic/operator-linebreak, unicorn/no-negated-condition */
                { name } = token, newName = String(i + 1);
                if (name !== newName || token === addedToken) {
                    token.setAttribute('name', newName);
                    this.getArgs(newName, false, false).add(token);
                    /* NOT FOR BROWSER */
                    if (name && token !== addedToken) {
                        this.getArgs(name, false, false).delete(token);
                    }
                }
            }
        }
        /**
         * @override
         * @param token node to be inserted / 待插入的子节点
         * @param i position to be inserted at / 插入位置
         */
        insertAt(token, i = this.length) {
            super.insertAt(token, i);
            if (token.anon) {
                this.#handleAnonArgChange(token, i === this.length - 1);
            }
            else if (token.name) {
                this.getArgs(token.name, false, false).add(token);
                /* NOT FOR BROWSER */
                this.#keys.add(token.name);
            }
            return token;
        }
        /**
         * Get parameters with the specified name
         *
         * 获取指定参数
         * @param key parameter name / 参数名
         * @param exact whether to match anonymosity / 是否匹配匿名性
         * @param copy whether to return a copy / 是否返回一个备份
         * @param init whether to initialize during parsing / 是否在解析时进行初始化
         */
        getArgs(key, exact, copy = true, init = true) {
            const keyStr = String(key)
                .replace(/^[ \t\n\0\v]+|([^ \t\n\0\v])[ \t\n\0\v]+$/gu, '$1');
            let args;
            if (this.#args.has(keyStr)) {
                args = this.#args.get(keyStr);
            }
            else {
                args = new Set(init ? [] : this.getAllArgs().filter(({ name }) => keyStr === name));
                this.#args.set(keyStr, args);
            }
            /* NOT FOR BROWSER */
            if (exact && keyStr.trim() && Number.isInteger(Number(keyStr))) {
                args = new Set([...args].filter(({ anon }) => typeof key === 'number' === anon));
            }
            else if (copy) {
                args = new Set(args);
            }
            /* NOT FOR BROWSER END */
            return args;
        }
        /**
         * Get duplicated parameters
         *
         * 获取重名参数
         * @throws `Error` 仅用于模板
         */
        getDuplicatedArgs() {
            LINT: {
                if (this.isTemplate()) {
                    return [...this.#args].filter(([, { size }]) => size > 1).map(([key, args]) => [key, [...args]]);
                }
                /* NOT FOR BROWSER */
                throw new Error('TranscludeToken.getDuplicatedArgs method is only for template!');
            }
        }
        /**
         * Get possible values of some magic words
         *
         * 对特定魔术字获取可能的取值
         * @throws `Error` 不是可接受的魔术字
         */
        getPossibleValues() {
            const { type, name, childNodes } = this;
            if (type === 'template') {
                throw new Error('TranscludeToken.getPossibleValues method is only for specific magic words!');
            }
            let start, queue;
            switch (name) {
                case 'if':
                case 'ifexist':
                case 'ifexpr':
                case 'iferror':
                    start = 2;
                    break;
                case 'ifeq':
                    start = 3;
                    break;
                case 'switch': {
                    const parameters = childNodes.slice(2), last = parameters[parameters.length - 1];
                    queue = [
                        ...parameters.filter(({ anon }) => !anon),
                        ...last?.anon ? [last] : [],
                    ].map(({ lastChild }) => lastChild);
                    break;
                }
                default:
                    throw new Error('TranscludeToken.getPossibleValues method is only for specific magic words!');
            }
            queue ??= childNodes.slice(start, start + 2).map(({ lastChild }) => lastChild);
            for (let i = 0; i < queue.length;) {
                const { length, 0: first } = queue[i].childNodes.filter(child => child.text().trim());
                if (length === 0) {
                    queue.splice(i, 1);
                }
                else if (length > 1 || !first.is('magic-word')) {
                    i++;
                }
                else {
                    try {
                        const possibleValues = first.getPossibleValues();
                        Array.prototype.splice.apply(queue, [i, 1, ...possibleValues]);
                        i += possibleValues.length;
                    }
                    catch {
                        i++;
                    }
                }
            }
            return queue;
        }
        /** @private */
        print() {
            PRINT: {
                const { childNodes, length, firstChild, modifier, type } = this;
                return `<span class="wpb-${type}${this.getAttribute('invalid') ? ' wpb-invalid' : ''}">{{${type === 'magic-word'
                    ? (0, string_1.escape)(modifier)
                        + firstChild.print()
                        + (length === 1 ? '' : this.#colon)
                        + (0, string_1.print)(childNodes.slice(1), { sep: '|' })
                    : (modifier ? `<span class="wpb-magic-word">${(0, string_1.escape)(modifier)}</span>` : '')
                        + (0, string_1.print)(childNodes, { sep: '|' })}}}</span>`;
            }
        }
        /* NOT FOR BROWSER */
        cloneNode() {
            const [first, ...cloned] = this.cloneChildNodes();
            return debug_1.Shadow.run(() => {
                // @ts-expect-error abstract class
                const token = new TranscludeToken(this.type === 'template' ? 'T' : first.text() + (cloned.length === 0 ? '' : this.#colon), [], this.getAttribute('config'));
                if (this.#raw) {
                    token.setModifier(this.modifier);
                }
                else {
                    token.setAttribute('modifier', this.modifier);
                }
                token.firstChild.safeReplaceWith(first);
                if (token.length > 1) {
                    token.removeAt(1);
                }
                token.safeAppend(cloned);
                return token;
            });
        }
        /**
         * Convert to substitution
         *
         * 替换引用
         */
        subst() {
            this.setModifier('subst:');
        }
        /**
         * Convert to safe substitution
         *
         * 安全的替换引用
         */
        safesubst() {
            this.setModifier('safesubst:');
        }
        /**
         * @override
         * @param i position of the child node / 移除位置
         */
        removeAt(i) {
            const token = super.removeAt(i);
            if (token.anon) {
                this.#handleAnonArgChange(Number(token.name));
            }
            else {
                const args = this.getArgs(token.name, false, false);
                args.delete(token);
                if (args.size === 0) {
                    this.#keys.delete(token.name);
                }
            }
            return token;
        }
        /**
         * Get all parameters
         *
         * 获取所有参数
         */
        getAllArgs() {
            const { childNodes } = this, i = childNodes.findIndex(({ type }) => type === 'parameter');
            return i === -1 ? [] : childNodes.slice(i);
        }
        /**
         * Get all anonymous parameters
         *
         * 获取所有匿名参数
         */
        getAnonArgs() {
            return this.getAllArgs().filter(({ anon }) => anon);
        }
        /**
         * Check if there is a parameter with the specified name
         *
         * 是否具有某参数
         * @param key parameter name / 参数名
         * @param exact whether to match anonymosity / 是否匹配匿名性
         */
        hasArg(key, exact) {
            return this.getArgs(key, exact, false, false).size > 0;
        }
        /**
         * Get the effective parameter with the specified name
         *
         * 获取生效的指定参数
         * @param key parameter name / 参数名
         * @param exact whether to match anonymosity / 是否匹配匿名性
         */
        getArg(key, exact) {
            return [...this.getArgs(key, exact, false)].sort((a, b) => b.compareDocumentPosition(a))[0];
        }
        /**
         * Remove parameters with the specified name
         *
         * 移除指定参数
         * @param key parameter name / 参数名
         * @param exact whether to match anonymosity / 是否匹配匿名性
         */
        removeArg(key, exact) {
            debug_1.Shadow.run(() => {
                for (const token of this.getArgs(key, exact, false)) {
                    this.removeChild(token);
                }
            });
        }
        /**
         * Get all parameter names
         *
         * 获取所有参数名
         */
        getKeys() {
            const args = this.getAllArgs();
            if (this.#keys.size === 0 && args.length > 0) {
                for (const { name } of args) {
                    this.#keys.add(name);
                }
            }
            return [...this.#keys];
        }
        /**
         * Get parameter values
         *
         * 获取参数值
         * @param key parameter name / 参数名
         */
        getValues(key) {
            return [...this.getArgs(key, false, false)].map(token => token.getValue());
        }
        getValue(key) {
            return key === undefined
                ? Object.fromEntries(this.getKeys().map(k => [k, this.getValue(k)]))
                : this.getArg(key)?.getValue();
        }
        /**
         * Insert an anonymous parameter
         *
         * 插入匿名参数
         * @param val parameter value / 参数值
         * @param newline whether to append the new parameter on a new line / 是否在添加参数时另起一行
         */
        newAnonArg(val, newline) {
            require('../addon/transclude');
            return this.newAnonArg(val, newline);
        }
        /**
         * Set the parameter value
         *
         * 设置参数值
         * @param key parameter name / 参数名
         * @param value parameter value / 参数值
         * @param newline whether to append the new parameter on a new line / 是否在添加参数时另起一行
         */
        setValue(key, value, newline) {
            require('../addon/transclude');
            this.setValue(key, value, newline);
        }
        /**
         * Convert all anonymous parameters to named ones
         *
         * 将匿名参数改写为命名参数
         * @throws `Error` 仅用于模板
         */
        anonToNamed() {
            if (!this.isTemplate()) {
                throw new Error('TranscludeToken.anonToNamed method is only for template!');
            }
            for (const token of this.getAnonArgs()) {
                token.firstChild.replaceChildren(token.name);
            }
        }
        /**
         * Replace the template name
         *
         * 替换模板名
         * @param title template name / 模板名
         */
        replaceTemplate(title) {
            require('../addon/transclude');
            this.replaceTemplate(title);
        }
        /**
         * Replace the module name
         *
         * 替换模块名
         * @param title module name / 模块名
         */
        replaceModule(title) {
            require('../addon/transclude');
            this.replaceModule(title);
        }
        /**
         * Replace the module function
         *
         * 替换模块函数
         * @param func module function name / 模块函数名
         */
        replaceFunction(func) {
            require('../addon/transclude');
            this.replaceFunction(func);
        }
        /**
         * Count duplicated parameters
         *
         * 重复参数计数
         * @throws `Error` 仅用于模板
         */
        hasDuplicatedArgs() {
            if (this.isTemplate()) {
                return this.getAllArgs().length - this.getKeys().length;
            }
            throw new Error('TranscludeToken.hasDuplicatedArgs method is only for template!');
        }
        /**
         * Fix duplicated parameters
         * @description
         * - Only empty parameters and identical parameters are removed with `aggressive = false`.
         * Anonymous parameters have a higher precedence, otherwise all anonymous parameters are converted to named ones.
         * - Additionally, consecutive numbered parameters are treated with `aggressive = true`.
         *
         * 修复重名参数
         * @description
         * - `aggressive = false`时只移除空参数和全同参数，优先保留匿名参数，否则将所有匿名参数更改为命名。
         * - `aggressive = true`时还会尝试处理连续的以数字编号的参数。
         * @param aggressive whether to use a more risky approach / 是否使用有更大风险的修复手段
         */
        fixDuplication(aggressive) {
            require('../addon/transclude');
            return this.fixDuplication(aggressive);
        }
        /**
         * Escape tables inside the template
         *
         * 转义模板内的表格
         */
        escapeTables() {
            require('../addon/transclude');
            return this.escapeTables();
        }
        /**
         * Get the module name and module function name
         *
         * 获取模块名和模块函数名
         * @since v1.16.4
         * @throws `Error` 仅用于模块
         */
        getModule() {
            /* c8 ignore next 3 */
            if (this.type !== 'magic-word' || this.name !== 'invoke') {
                throw new Error('TranscludeToken.getModule method is only for modules!');
            }
            return [this.module, this.function];
        }
        /**
         * Get the [frame object](https://www.mediawiki.org/wiki/Extension:Scribunto/Lua_reference_manual#frame-object)
         *
         * 获取 [frame 对象](https://www.mediawiki.org/wiki/Extension:Scribunto/Lua_reference_manual#frame-object)
         * @param context template calling this module / 调用该模块的模板
         * @since v1.22.0
         * @throws `Error` 仅用于模块
         */
        getFrame(context) {
            /* c8 ignore next 3 */
            if (this.type === 'template' ? context : this.name !== 'invoke') {
                throw new Error('TranscludeToken.getFrame method is only for modules!');
            }
            return {
                args: this.getValue(),
                parent: context?.getFrame(),
                title: this.#getTitle().toString(true),
            };
        }
        /** @private */
        toHtmlInternal(opt) {
            const { type, name } = this;
            if (type === 'template' && !name.startsWith('Special:')) {
                if (this.normalizeTitle(name, 0, { halfParsed: true, temporary: true }).valid) {
                    const title = name.replaceAll('_', ' ');
                    return `<a href="${this.#title.getUrl()}?action=edit&redlink=1" class="new" title="${title} (page does not exist)">${title}</a>`;
                }
                const str = this.text();
                return opt?.nowrap ? str.replaceAll('\n', ' ') : str;
            }
            return '';
        }
    };
    return TranscludeToken = _classThis;
})();
exports.TranscludeToken = TranscludeToken;
constants_1.classes['TranscludeToken'] = __filename;
