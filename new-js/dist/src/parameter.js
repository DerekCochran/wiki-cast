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
exports.ParameterToken = void 0;
const string_1 = require("../util/string");
const lint_1 = require("../util/lint");
const index_1 = __importDefault(require("../index"));
const index_2 = require("./index");
/* NOT FOR BROWSER */
const debug_1 = require("../util/debug");
const constants_1 = require("../util/constants");
const fixed_1 = require("../mixin/fixed");
/* NOT FOR BROWSER END */
// eslint-disable-next-line @typescript-eslint/no-unused-expressions
/https?:\/\/(?:\[[\da-f:.]+\]|[^[\]<>"\t\n\p{Zs}])[^[\]<>"\0\t\n\p{Zs}]*$/iu;
const linkRegex = /* #__PURE__ */ (() => new RegExp(`https?://${string_1.extUrlCharFirst}${string_1.extUrlChar}$`, 'iu'))();
/**
 * template or magic word parameter
 *
 * 模板或魔术字参数
 * @classdesc `{childNodes: [Token, Token]}`
 */
let ParameterToken = (() => {
    let _classDecorators = [fixed_1.fixedToken];
    let _classDescriptor;
    let _classExtraInitializers = [];
    let _classThis;
    let _classSuper = index_2.Token;
    var ParameterToken = class extends _classSuper {
        static { _classThis = this; }
        static {
            const _metadata = typeof Symbol === "function" && Symbol.metadata ? Object.create(_classSuper[Symbol.metadata] ?? null) : void 0;
            __esDecorate(null, _classDescriptor = { value: _classThis }, _classDecorators, { kind: "class", name: _classThis.name, metadata: _metadata }, null, _classExtraInitializers);
            ParameterToken = _classThis = _classDescriptor.value;
            if (_metadata) Object.defineProperty(_classThis, Symbol.metadata, { enumerable: true, configurable: true, writable: true, value: _metadata });
            __runInitializers(_classThis, _classExtraInitializers);
        }
        /* NOT FOR BROWSER END */
        get type() {
            return 'parameter';
        }
        /** whether to be anonymous / 是否是匿名参数 */
        get anon() {
            return this.firstChild.length === 0;
        }
        /* PRINT ONLY */
        /** whether to be a duplicated parameter / 是否是重复参数 */
        get duplicated() {
            LSP: {
                const { parentNode, name } = this;
                return Boolean(parentNode?.isTemplate())
                    && parentNode.getArgs(name, false, false).size > 1;
            }
        }
        /* PRINT ONLY END */
        /* NOT FOR BROWSER */
        set duplicated(value) {
            if (this.duplicated && !value) {
                this.parentNode.fixDuplication();
            }
        }
        set anon(value) {
            if (value) {
                throw new Error('Cannot convert named parameter to anonymous parameter!');
            }
            this.parentNode?.anonToNamed();
        }
        /** parameter value / 参数值 */
        get value() {
            return this.getValue();
        }
        set value(value) {
            this.setValue(value);
        }
        /* NOT FOR BROWSER END */
        /**
         * @param key 参数名
         * @param value 参数值
         */
        constructor(key, value, config, accum = []) {
            super(undefined, config, accum);
            const keyToken = new index_2.Token(typeof key === 'number' ? undefined : key, config, accum, {
                'Stage-11': ':', '!HeadingToken': '',
            }), token = new index_2.Token(value, config, accum);
            keyToken.type = 'parameter-key';
            keyToken.setAttribute('stage', 2);
            token.type = 'parameter-value';
            token.setAttribute('stage', 2);
            this.append(keyToken, token);
            /* NOT FOR BROWSER */
            if (typeof key === 'string') {
                this.trimName((0, string_1.removeCommentLine)(key));
            }
        }
        /** @private */
        trimName(name, set = true) {
            const trimmed = (typeof name === 'string' ? name : name.toString(true))
                .replace(/^[ \t\n\0\v]+|([^ \t\n\0\v])[ \t\n\0\v]+$/gu, '$1');
            if (set) {
                this.setAttribute('name', trimmed);
            }
            return trimmed;
        }
        /** @private */
        afterBuild() {
            if (!this.anon) {
                const { parentNode, firstChild } = this, name = this.trimName(firstChild);
                if (parentNode) {
                    parentNode.getArgs(name, false, false).add(this);
                    /* NOT FOR BROWSER */
                    parentNode.getAttribute('keys').add(name);
                }
            }
            super.afterBuild();
            /* NOT FOR BROWSER */
            const /** @implements */ parameterListener = ({ prevTarget }, data) => {
                if (!this.anon) { // 匿名参数不管怎么变动还是匿名
                    const { firstChild, name } = this;
                    if (prevTarget === firstChild) {
                        const newKey = this.trimName(firstChild);
                        data.oldKey = name;
                        data.newKey = newKey;
                    }
                }
            };
            this.addEventListener(['remove', 'insert', 'replace', 'text'], parameterListener);
        }
        /** @private */
        toString(skip) {
            return this.anon ? this.lastChild.toString(skip) : super.toString(skip, '=');
        }
        /** @private */
        text() {
            return this.anon ? this.lastChild.text() : super.text('=');
        }
        /** @private */
        getGaps() {
            return this.anon ? 0 : 1;
        }
        /** @private */
        lint(start = this.getAbsoluteIndex(), re) {
            LINT: {
                const errors = super.lint(start, re), rule = 'unescaped', { lintConfig } = index_1.default, s = lintConfig.getSeverity(rule);
                if (s) {
                    const { firstChild } = this, link = linkRegex.exec(firstChild.text())?.[0];
                    try {
                        if (link && new URL(link).search) {
                            const e = (0, lint_1.generateForChild)(firstChild, { start }, rule, 'unescaped-query', s);
                            e.startIndex = e.endIndex;
                            e.startLine = e.endLine;
                            e.startCol = e.endCol;
                            e.endIndex++;
                            e.endCol++;
                            if (lintConfig.computeEditInfo || lintConfig.fix) {
                                e.fix = (0, lint_1.fixByEscape)(e.startIndex, '{{=}}');
                            }
                            errors.push(e);
                        }
                    }
                    catch { }
                }
                return errors;
            }
        }
        /** @private */
        print() {
            PRINT: return super.print({ sep: this.anon ? '' : '=' });
        }
        /** @private */
        json(_, depth, start = this.getAbsoluteIndex()) {
            LSP: {
                const json = super.json(undefined, depth, start);
                Object.assign(json, { anon: this.anon }, this.duplicated && { duplicated: true });
                return json;
            }
        }
        /* NOT FOR BROWSER */
        cloneNode() {
            const [key, value] = this.cloneChildNodes();
            return debug_1.Shadow.run(() => {
                // @ts-expect-error abstract class
                const token = new ParameterToken(this.anon ? Number(this.name) : undefined, undefined, this.getAttribute('config'));
                token.firstChild.safeReplaceWith(key);
                token.lastChild.safeReplaceWith(value);
                if (this.anon) {
                    token.setAttribute('name', this.name);
                }
                return token;
            });
        }
        safeReplaceWith(token) {
            index_1.default.warn(`${this.constructor.name}.safeReplaceWith regress to AstNode.replaceWith.`);
            this.replaceWith(token);
        }
        /**
         * Get the parameter value
         *
         * 获取参数值
         */
        getValue() {
            const value = (0, string_1.removeCommentLine)(this.lastChild.text());
            return this.anon && this.parentNode?.isTemplate() !== false ? value : value.trim();
        }
        /**
         * Set the parameter value
         *
         * 设置参数值
         * @param value parameter value / 参数值
         */
        setValue(value) {
            const { childNodes } = index_1.default.parseWithRef(value, this);
            this.lastChild.safeReplaceChildren(childNodes);
        }
        /**
         * Rename the parameter
         *
         * 修改参数名
         * @param key new parameter name / 新参数名
         * @param force whether to rename regardless of conflicts / 是否无视冲突命名
         * @throws `Error` 仅用于模板参数
         * @throws `RangeError` 更名造成重复参数
         */
        rename(key, force) {
            const { parentNode, anon } = this;
            // 必须检测是否是TranscludeToken
            if (parentNode?.isTemplate() === false) {
                throw new Error('ParameterToken.rename method is only for template parameters!');
            }
            else if (anon) {
                parentNode?.anonToNamed();
            }
            const root = index_1.default.parseWithRef(key, this), name = this.trimName(root, false);
            if (this.name === name) {
                index_1.default.warn('The actual parameter name is not changed', name);
            }
            else if (parentNode?.hasArg(name)) {
                if (force) {
                    index_1.default.warn('Parameter renaming causes duplicated parameters', name);
                }
                else {
                    throw new RangeError(`Parameter renaming causes duplicated parameters: ${name}`);
                }
            }
            this.firstChild.safeReplaceChildren(root.childNodes);
        }
    };
    return ParameterToken = _classThis;
})();
exports.ParameterToken = ParameterToken;
constants_1.classes['ParameterToken'] = __filename;
