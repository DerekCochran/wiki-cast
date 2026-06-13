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
exports.ConverterFlagsToken = void 0;
const lint_1 = require("../util/lint");
const rect_1 = require("../lib/rect");
const gapped_1 = require("../mixin/gapped");
const noEscape_1 = require("../mixin/noEscape");
const index_1 = __importDefault(require("../index"));
const index_2 = require("./index");
const atom_1 = require("./atom");
/* NOT FOR BROWSER */
const debug_1 = require("../util/debug");
const constants_1 = require("../util/constants");
const clone_1 = require("../mixin/clone");
/* NOT FOR BROWSER END */
const definedFlags = new Set(['A', 'T', 'R', 'D', '-', 'H', 'N']);
/**
 * flags for language conversion
 *
 * 转换flags
 * @classdesc `{childNodes: AtomToken[]}`
 */
let ConverterFlagsToken = (() => {
    let _classDecorators = [(0, gapped_1.gapped)(), noEscape_1.noEscape];
    let _classDescriptor;
    let _classExtraInitializers = [];
    let _classThis;
    let _classSuper = index_2.Token;
    let _instanceExtraInitializers = [];
    let _cloneNode_decorators;
    var ConverterFlagsToken = class extends _classSuper {
        static { _classThis = this; }
        static {
            const _metadata = typeof Symbol === "function" && Symbol.metadata ? Object.create(_classSuper[Symbol.metadata] ?? null) : void 0;
            _cloneNode_decorators = [clone_1.clone];
            __esDecorate(this, null, _cloneNode_decorators, { kind: "method", name: "cloneNode", static: false, private: false, access: { has: obj => "cloneNode" in obj, get: obj => obj.cloneNode }, metadata: _metadata }, null, _instanceExtraInitializers);
            __esDecorate(null, _classDescriptor = { value: _classThis }, _classDecorators, { kind: "class", name: _classThis.name, metadata: _metadata }, null, _classExtraInitializers);
            ConverterFlagsToken = _classThis = _classDescriptor.value;
            if (_metadata) Object.defineProperty(_classThis, Symbol.metadata, { enumerable: true, configurable: true, writable: true, value: _metadata });
            __runInitializers(_classThis, _classExtraInitializers);
        }
        #flags = __runInitializers(this, _instanceExtraInitializers);
        /* NOT FOR BROWSER END */
        get type() {
            return 'converter-flags';
        }
        /* NOT FOR BROWSER */
        /** all language conversion flags / 所有转换类型标记 */
        get flags() {
            return this.getAllFlags();
        }
        set flags(value) {
            this.replaceChildren();
            for (const flag of value) {
                this.#newFlag(flag);
            }
        }
        /* NOT FOR BROWSER END */
        /** @param flags 转换类型标记 */
        constructor(flags, config, accum = []) {
            super(undefined, config, accum, {
                AtomToken: ':',
            });
            this.safeAppend(flags.map(flag => new atom_1.AtomToken(flag, 'converter-flag', config, accum)));
        }
        /** @private */
        afterBuild() {
            this.#flags = this.childNodes.map(child => child.text().trim());
            super.afterBuild();
            /* NOT FOR BROWSER */
            const /** @implements */ converterFlagsListener = ({ prevTarget }) => {
                if (prevTarget) {
                    this.#flags[this.childNodes.indexOf(prevTarget)] = prevTarget.text().trim();
                }
            };
            this.addEventListener(['remove', 'insert', 'text', 'replace'], converterFlagsListener);
        }
        /** @private */
        toString(skip) {
            return super.toString(skip, ';');
        }
        /** @private */
        text() {
            return super.text(';');
        }
        /**
         * Get unknown language conversion flags
         *
         * 获取未知的转换类型标记
         */
        getUnknownFlags() {
            return new Set(this.#flags.filter(flag => /\{{3}[^{}]+\}{3}/u.test(flag)));
        }
        /**
         * Get language coversion flags that specify a language variant
         *
         * 获取指定语言变体的转换标记
         */
        getVariantFlags() {
            const variants = new Set(this.getAttribute('config').variants);
            return new Set(this.#flags.filter(flag => variants.has(flag)));
        }
        /** @private */
        isInvalidFlag(flag, variant, unknown) {
            /* PRINT ONLY */
            PRINT: if (typeof flag === 'object') {
                const variants = new Set(this.getAttribute('config').variants);
                flag = flag.text().trim();
                variant = this.#flags.some(f => variants.has(f)) ? variants : new Set();
                unknown = this.getUnknownFlags();
            }
            /* PRINT ONLY END */
            return Boolean(flag)
                && !variant.has(flag)
                && !unknown.has(flag)
                && (variant.size > 0 || !definedFlags.has(flag));
        }
        /** @private */
        lint(start = this.getAbsoluteIndex(), re) {
            LINT: {
                const variantFlags = this.getVariantFlags(), unknownFlags = this.getUnknownFlags(), emptyFlagCount = this.#flags.filter(flag => !flag).length, knownFlagCount = this.#flags.length - unknownFlags.size - emptyFlagCount, { lintConfig } = index_1.default, { computeEditInfo, fix } = lintConfig, errors = super.lint(start, re);
                if (variantFlags.size === knownFlagCount
                    || this.#flags.filter(flag => definedFlags.has(flag)).length === knownFlagCount) {
                    return errors;
                }
                const rule = 'no-ignored', s = lintConfig.getSeverity(rule, 'conversionFlag');
                if (s) {
                    const rect = new rect_1.BoundingRect(this, start);
                    for (let i = 0; i < this.length; i++) {
                        const child = this.childNodes[i], flag = child.text().trim();
                        if (this.isInvalidFlag(flag, variantFlags, unknownFlags)) {
                            const e = (0, lint_1.generateForChild)(child, rect, rule, 'invalid-conversion-flag', s);
                            if (computeEditInfo || fix) {
                                if (variantFlags.size === 0 && definedFlags.has(flag.toUpperCase())) {
                                    e.fix = (0, lint_1.fixByUpper)(e, flag);
                                }
                                else if (computeEditInfo) {
                                    e.suggestions = [(0, lint_1.fixByRemove)(e, i && -1)];
                                }
                            }
                            errors.push(e);
                        }
                    }
                }
                return errors;
            }
        }
        /** @private */
        print() {
            PRINT: return super.print({ sep: ';' });
        }
        /* NOT FOR BROWSER */
        cloneNode() {
            // @ts-expect-error abstract class
            return new ConverterFlagsToken([], this.getAttribute('config'));
        }
        /**
         * @override
         * @param i position of the child node / 移除位置
         */
        removeAt(i) {
            const token = super.removeAt(i);
            this.#flags?.splice(i, 1);
            return token;
        }
        /**
         * @override
         * @param token node to be inserted / 待插入的子节点
         * @param i position to be inserted at / 插入位置
         */
        insertAt(token, i = this.length) {
            super.insertAt(token, i);
            this.#flags?.splice(i, 0, token.text().trim());
            return token;
        }
        /**
         * Get all language conversion flags
         *
         * 获取所有转换类型标记
         */
        getAllFlags() {
            return new Set(this.#flags);
        }
        /**
         * Get the conversion flag token
         *
         * 获取转换类型标记节点
         * @param flag language conversion flag / 转换类型标记
         */
        getFlagTokens(flag) {
            return this.#flags.includes(flag) ? this.childNodes.filter(child => child.text().trim() === flag) : [];
        }
        /**
         * Get effective language conversion flags
         *
         * 获取有效的转换类型标记
         */
        getEffectiveFlags() {
            const variantFlags = this.getVariantFlags(), unknownFlags = this.getUnknownFlags();
            const flags = new Set([...this.#flags.filter(flag => definedFlags.has(flag)), ...unknownFlags]);
            if (flags.size === 0 && variantFlags.size === 0) {
                return new Set('S');
            }
            else if (flags.has('R')) {
                return new Set('R');
            }
            else if (flags.has('N')) {
                return new Set('N');
            }
            else if (flags.has('-')) {
                return new Set('-');
            }
            else if (flags.has('H')) {
                const hasT = flags.has('T'), hasD = flags.has('D');
                return hasT && hasD
                    ? new Set(['+', 'H', 'T', 'D'])
                    : new Set(['+', 'H', ...hasT ? ['T'] : [], ...hasD ? ['D'] : [], ...unknownFlags]);
            }
            else if (variantFlags.size > 0) {
                return new Set([...variantFlags, ...unknownFlags]);
            }
            if (flags.size === 1 && flags.has('T')) {
                flags.add('H');
            }
            if (flags.has('A')) {
                flags.add('+');
                flags.add('S');
            }
            if (flags.has('D')) {
                flags.delete('S');
            }
            return flags;
        }
        /**
         * Check if a language conversion flag is present
         *
         * 是否具有某转换类型标记
         * @param flag language conversion flag / 转换类型标记
         */
        hasFlag(flag) {
            return this.#flags.includes(flag);
        }
        /**
         * Check if an effective language conversion flag is present
         *
         * 是否具有某有效的转换类型标记
         * @param flag language conversion flag / 转换类型标记
         */
        hasEffectiveFlag(flag) {
            return this.getEffectiveFlags().has(flag);
        }
        /**
         * Remove a language conversion flag
         *
         * 移除某转换类型标记
         * @param flag language conversion flag / 转换类型标记
         */
        removeFlag(flag) {
            for (const token of this.getFlagTokens(flag)) {
                token.remove();
            }
        }
        /**
         * 添加转换类型标记
         * @param flag 转换类型标记
         */
        #newFlag(flag) {
            this.insertAt(debug_1.Shadow.run(() => new atom_1.AtomToken(flag, 'converter-flag', this.getAttribute('config'))));
        }
        /**
         * Set a language conversion flag
         *
         * 设置转换类型标记
         * @param flag language conversion flag / 转换类型标记
         */
        setFlag(flag) {
            if (!this.#flags.includes(flag)) {
                this.#newFlag(flag);
            }
        }
        /**
         * Toggle a language conversion flag
         *
         * 开关转换类型标记
         * @param flag language conversion flag / 转换类型标记
         */
        toggleFlag(flag) {
            if (this.#flags.includes(flag)) {
                this.removeFlag(flag);
            }
            else {
                this.#newFlag(flag);
            }
        }
    };
    return ConverterFlagsToken = _classThis;
})();
exports.ConverterFlagsToken = ConverterFlagsToken;
constants_1.classes['ConverterFlagsToken'] = __filename;
