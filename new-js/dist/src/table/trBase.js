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
exports.TrBaseToken = void 0;
const lint_1 = require("../../util/lint");
const index_1 = __importDefault(require("../../index"));
const base_1 = require("./base");
const td_1 = require("./td");
/* NOT FOR BROWSER */
const debug_1 = require("../../util/debug");
const constants_1 = require("../../util/constants");
const html_1 = require("../../util/html");
const cached_1 = require("../../mixin/cached");
const index_2 = require("../index");
/**
 * table row or table
 *
 * 表格行或表格
 */
let TrBaseToken = (() => {
    let _classSuper = base_1.TableBaseToken;
    let _instanceExtraInitializers = [];
    let _toHtmlInternal_decorators;
    return class TrBaseToken extends _classSuper {
        static {
            const _metadata = typeof Symbol === "function" && Symbol.metadata ? Object.create(_classSuper[Symbol.metadata] ?? null) : void 0;
            _toHtmlInternal_decorators = [(0, cached_1.cached)()];
            __esDecorate(this, null, _toHtmlInternal_decorators, { kind: "method", name: "toHtmlInternal", static: false, private: false, access: { has: obj => "toHtmlInternal" in obj, get: obj => obj.toHtmlInternal }, metadata: _metadata }, null, _instanceExtraInitializers);
            if (_metadata) Object.defineProperty(this, Symbol.metadata, { enumerable: true, configurable: true, writable: true, value: _metadata });
        }
        /** @private */
        lint(start = this.getAbsoluteIndex(), re) {
            LINT: {
                const errors = super.lint(start, re), inter = this.childNodes.find(({ type }) => type === 'table-inter');
                if (!inter) {
                    return errors;
                }
                const severity = (0, lint_1.isFostered)(inter), rule = 'fostered-content', s = severity && index_1.default.lintConfig.getSeverity(rule, severity === 2 ? undefined : 'transclusion');
                if (s) {
                    const error = (0, lint_1.generateForChild)(inter, { start }, rule, 'content-outside-table', s);
                    error.startIndex++;
                    error.startLine++;
                    error.startCol = 0;
                    errors.push(error);
                }
                return errors;
            }
        }
        /**
         * Get the number of rows
         *
         * 获取行数
         */
        getRowCount() {
            LINT: return Number(this.childNodes.some(child => child instanceof td_1.TdToken
                && child.isIndependent()
                && !child.firstChild.text().endsWith('+')));
        }
        getNthCol(n, insert) {
            const nCols = this.getColCount();
            n += n < 0 ? nCols : 0;
            /* c8 ignore next 3 */
            if (n < 0 || n > nCols || n === nCols && !insert) {
                throw new RangeError(`There is no cell at position ${n}!`);
            }
            let last = 0;
            for (const child of this.childNodes.slice(2)) {
                if (child instanceof td_1.TdToken) {
                    if (child.isIndependent()) {
                        last = Number(child.subtype !== 'caption');
                    }
                    n -= last;
                    if (n < 0) {
                        return child;
                    }
                }
                else if (child.is('tr') || child.is('table-syntax')) {
                    return child;
                }
            }
            return undefined;
        }
        /** 修复简单的表格语法错误 */
        #correct() {
            const [, , child] = this.childNodes;
            if (child?.constructor === index_2.Token) {
                const { firstChild } = child;
                if (firstChild?.type !== 'text') {
                    child.prepend('\n');
                }
                else if (!firstChild.data.startsWith('\n')) {
                    firstChild.insertData(0, '\n');
                }
            }
        }
        /** @private */
        text() {
            this.#correct();
            return super.text();
        }
        /** @private */
        toString(skip) {
            this.#correct();
            return super.toString(skip);
        }
        /** @private */
        removeAt(i) {
            i += i < 0 ? this.length : 0;
            const child = this.childNodes[i];
            if (child instanceof td_1.TdToken && child.isIndependent()) {
                const { nextSibling } = child;
                if (nextSibling?.is('td')) {
                    nextSibling.independence();
                }
            }
            return super.removeAt(i);
        }
        /**
         * @override
         * @param token node to be inserted / 待插入的子节点
         * @param i position to be inserted at / 插入位置
         */
        insertAt(token, i = this.length) {
            if (!debug_1.Shadow.running && !token.is('td')) {
                /* c8 ignore next 6 */
                if (this.is('tr')) {
                    this.typeError('insertAt', 'TdToken');
                }
                if (!token.is('tr')) {
                    this.typeError('insertAt', 'TrToken', 'TdToken');
                }
            }
            i += i < 0 ? this.length : 0;
            const child = this.childNodes[i];
            if (token instanceof td_1.TdToken && token.isIndependent() && child instanceof td_1.TdToken) {
                child.independence();
            }
            return super.insertAt(token, i);
        }
        /**
         * Get the number of columns
         *
         * 获取列数
         */
        getColCount() {
            let count = 0, last = 0;
            for (const child of this.childNodes) {
                if (child instanceof td_1.TdToken) {
                    last = child.isIndependent() ? Number(child.subtype !== 'caption') : last;
                    count += last;
                }
            }
            return count;
        }
        /**
         * Insert a new cell
         *
         * 插入新的单元格
         * @param inner inner wikitext of the cell / 单元格内部wikitext
         * @param {TableCoords} coord table coordinates of the cell / 单元格坐标
         * @param subtype cell type / 单元格类型
         * @param attr cell attribute / 单元格属性
         */
        insertTableCell(inner, { column }, subtype = 'td', attr = {}) {
            return this.insertBefore((0, td_1.createTd)(inner, this, subtype, attr), this.getNthCol(column, true));
        }
        /** @private */
        toHtmlInternal(opt) {
            const { childNodes, type } = this, td = childNodes.filter((0, debug_1.isToken)('td'));
            return td.some(({ subtype }) => subtype !== 'caption')
                ? `<tr${type === 'tr' ? childNodes[1].toHtmlInternal() : ''}>${(0, html_1.html)(td, '', opt)}</tr>`
                : (0, html_1.html)(td, '', opt);
        }
        constructor() {
            super(...arguments);
            __runInitializers(this, _instanceExtraInitializers);
        }
    };
})();
exports.TrBaseToken = TrBaseToken;
constants_1.classes['TrBaseToken'] = __filename;
