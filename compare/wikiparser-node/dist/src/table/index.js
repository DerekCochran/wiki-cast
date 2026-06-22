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
exports.TableToken = exports.Layout = void 0;
const lint_1 = require("../../util/lint");
const debug_1 = require("../../util/debug");
const rect_1 = require("../../lib/rect");
const cached_1 = require("../../mixin/cached");
const index_1 = __importDefault(require("../../index"));
const trBase_1 = require("./trBase");
const syntax_1 = require("../syntax");
/* NOT FOR BROWSER */
const html_1 = require("../../util/html");
const constants_1 = require("../../util/constants");
const string_1 = require("../../util/string");
const closingPattern = /^\n[^\S\n]*(?:\|\}|\{\{\s*!\s*\}\}\}|\{\{\s*!\)\s*\}\})$/u;
/** @extends {Array<TableCoords[]>} */
class Layout extends Array {
    /* NOT FOR BROWSER */
    /**
     * Print the table layout
     *
     * 打印表格布局
     */
    print() {
        require('../../addon/table');
        this.print();
    }
}
exports.Layout = Layout;
/**
 * table
 *
 * 表格
 * @classdesc `{childNodes: [SyntaxToken, AttributesToken, ?Token, ...TdToken[], ...TrToken[], ?SyntaxToken]}`
 */
let TableToken = (() => {
    let _classSuper = trBase_1.TrBaseToken;
    let _instanceExtraInitializers = [];
    let _getLayout_decorators;
    let _toHtmlInternal_decorators;
    return class TableToken extends _classSuper {
        static {
            const _metadata = typeof Symbol === "function" && Symbol.metadata ? Object.create(_classSuper[Symbol.metadata] ?? null) : void 0;
            _getLayout_decorators = [(0, cached_1.cached)(false)];
            _toHtmlInternal_decorators = [(0, cached_1.cached)()];
            __esDecorate(this, null, _getLayout_decorators, { kind: "method", name: "getLayout", static: false, private: false, access: { has: obj => "getLayout" in obj, get: obj => obj.getLayout }, metadata: _metadata }, null, _instanceExtraInitializers);
            __esDecorate(this, null, _toHtmlInternal_decorators, { kind: "method", name: "toHtmlInternal", static: false, private: false, access: { has: obj => "toHtmlInternal" in obj, get: obj => obj.toHtmlInternal }, metadata: _metadata }, null, _instanceExtraInitializers);
            if (_metadata) Object.defineProperty(this, Symbol.metadata, { enumerable: true, configurable: true, writable: true, value: _metadata });
        }
        /* NOT FOR BROWSER END */
        get type() {
            return 'table';
        }
        /** whether the table is closed / 表格是否闭合 */
        get closed() {
            LINT: return this.lastChild.is('table-syntax');
        }
        /* NOT FOR BROWSER */
        set closed(closed) {
            if (closed && !this.closed) {
                this.close(this.isInside('parameter') ? '\n{{!}}}' : '\n|}');
            }
        }
        /* NOT FOR BROWSER END */
        /**
         * @param syntax 表格语法
         * @param attr 表格属性
         */
        constructor(syntax, attr, config, accum) {
            super(/^(?:\{\||\{\{\{\s*!\s*\}\}|\{\{\s*\(!\s*\}\})$/u, syntax, 'table', attr, config, accum, {
                Token: 2, SyntaxToken: [0, -1], AttributesToken: 1, TdToken: '2:', TrToken: '2:',
            });
            __runInitializers(this, _instanceExtraInitializers);
        }
        /** @private */
        lint(start = this.getAbsoluteIndex(), re) {
            LINT: {
                const errors = super.lint(start, re), rect = new rect_1.BoundingRect(this, start), rules = ['unclosed-table', 'table-layout'], s = rules.map(rule => index_1.default.lintConfig.getSeverity(rule));
                if (s[0] && !this.closed) {
                    errors.push((0, lint_1.generateForChild)(this.firstChild, rect, rules[0], 'unclosed-table', s[0]));
                }
                if (s[1]) {
                    const layout = this.getLayout(), { length } = layout;
                    if (length > 1) {
                        let low = 1, high = Infinity, j = 0;
                        for (; j < length; j++) {
                            const row = layout[j], max = row.length;
                            if (max < low) {
                                break;
                            }
                            else if (max < high) {
                                high = max;
                            }
                            const min = row.indexOf(row[max - 1]) + 1;
                            if (min > high) {
                                break;
                            }
                            else if (min > low) {
                                low = min;
                            }
                        }
                        if (j < length) {
                            const row = this.getNthRow(j), e = (0, lint_1.generateForChild)(row, rect, rules[1], 'inconsistent-table', s[1]);
                            e.startIndex++;
                            e.startLine++;
                            e.startCol = 0;
                            errors.push(e);
                        }
                    }
                }
                return errors;
            }
        }
        // eslint-disable-next-line jsdoc/require-param
        /**
         * Close the table syntax
         *
         * 闭合表格语法
         * @param syntax syntax of the table end / 表格结尾语法
         */
        close(syntax = '\n|}', halfParsed) {
            if (!this.lastChild.is('table-syntax')) {
                debug_1.Shadow.run(() => {
                    const token = new syntax_1.SyntaxToken(halfParsed ? syntax : undefined, closingPattern, 'table-syntax', this.getAttribute('config'), this.getAttribute('accum'), { 'Stage-1': ':', '!ExtToken': '', TranscludeToken: ':' });
                    super.insertAt(token);
                    /* NOT FOR BROWSER */
                    if (!halfParsed) {
                        token.afterBuild();
                    }
                });
            }
            /* NOT FOR BROWSER */
            if (!halfParsed) {
                const { childNodes } = index_1.default.parseWithRef(syntax, this, 2);
                this.lastChild.safeReplaceChildren(childNodes);
            }
        }
        /**
         * Get the table layout
         *
         * 获取表格布局
         * @param stop stop condition / 中止条件
         * @param stop.row stop at the row / 中止行
         * @param stop.column stop at the column / 中止列
         * @param stop.x stop at the row / 中止行
         * @param stop.y stop at the column / 中止列
         */
        getLayout(stop) {
            LINT: {
                const rows = this.getAllRows(), { length } = rows, layout = Layout.from((0, debug_1.emptyArray)(length, () => []));
                for (let i = 0; i < layout.length; i++) {
                    const rowLayout = layout[i];
                    /* NOT FOR BROWSER */
                    if (i > (stop?.row ?? stop?.y ?? NaN)) {
                        break;
                    }
                    /* NOT FOR BROWSER END */
                    let j = 0, k = 0, last;
                    for (const cell of rows[i].childNodes.slice(2)) {
                        if (cell.is('td')) {
                            if (cell.isIndependent()) {
                                last = cell.subtype !== 'caption';
                            }
                            if (last) {
                                const coords = { row: i, column: j }, { rowspan, colspan } = cell;
                                j++;
                                while (rowLayout[k]) {
                                    k++;
                                }
                                /* NOT FOR BROWSER */
                                if (i === stop?.row && j > stop.column) {
                                    rowLayout[k] = coords;
                                    return layout;
                                }
                                /* NOT FOR BROWSER END */
                                for (let y = i; y < Math.min(i + rowspan, length); y++) {
                                    for (let x = k; x < k + colspan; x++) {
                                        layout[y][x] = coords;
                                    }
                                }
                                k += colspan;
                                /* NOT FOR BROWSER */
                                if (i === stop?.y && k > (stop.x ?? NaN)) {
                                    return layout;
                                }
                            }
                        }
                        else if ((0, debug_1.isRowEnd)(cell)) {
                            break;
                        }
                    }
                }
                return layout;
            }
        }
        /**
         * Get all rows
         *
         * 获取所有行
         */
        getAllRows() {
            LINT: return [
                ...super.getRowCount() ? [this] : [],
                ...this.childNodes.slice(1)
                    .filter((child) => child.is('tr') && child.getRowCount() > 0),
            ];
        }
        getNthRow(n, force, insert) {
            LINT: {
                const isRow = super.getRowCount();
                /* NOT FOR BROWSER */
                const nRows = this.getRowCount();
                n += n < 0 ? nRows : 0;
                /* NOT FOR BROWSER END */
                if (n === 0
                    && (isRow
                        || force && nRows === 0)) {
                    return this;
                }
                /* NOT FOR BROWSER */
                /* c8 ignore next 3 */
                if (n < 0 || n > nRows || n === nRows && !insert) {
                    throw new RangeError(`The table does not have row ${n}!`);
                }
                /* NOT FOR BROWSER END */
                if (isRow) {
                    n--;
                }
                for (const child of this.childNodes.slice(2)) {
                    const { type } = child;
                    if (type === 'tr' && child.getRowCount()) {
                        n--;
                        if (n < 0) {
                            return child;
                        }
                        /* NOT FOR BROWSER */
                    }
                    else if (type === 'table-syntax') {
                        return child;
                        /* NOT FOR BROWSER END */
                    }
                }
                return undefined;
            }
        }
        /** @private */
        json(_, depth, start = this.getAbsoluteIndex()) {
            LSP: {
                const json = super.json(undefined, depth, start);
                json['closed'] = this.closed;
                return json;
            }
        }
        /* NOT FOR BROWSER */
        /**
         * @override
         * @param token node to be inserted / 待插入的子节点
         * @param i position to be inserted at / 插入位置
         * @throws `SyntaxError` 表格的闭合部分非法
         */
        insertAt(token, i = this.length) {
            i += i < 0 ? this.length : 0;
            const previous = this.childNodes[i - 1];
            if (typeof token !== 'string' && token.is('td') && previous?.is('tr')) {
                index_1.default.warn('The table cell is inserted into the current row instead.');
                return previous.insertAt(token);
            }
            /* c8 ignore next 3 */
            if (i > 0 && token instanceof syntax_1.SyntaxToken && token.pattern !== closingPattern) {
                throw new SyntaxError(`The closing part of the table is invalid: ${(0, string_1.noWrap)(token.toString())}`);
            }
            return super.insertAt(token, i);
        }
        /** @private */
        getRowCount() {
            return super.getRowCount()
                + this.childNodes.filter(child => child.is('tr') && child.getRowCount()).length;
        }
        /**
         * Get the next row
         *
         * 获取下一行
         */
        getNextRow() {
            return this.getNthRow(super.getRowCount() ? 1 : 0, false, false);
        }
        /**
         * Get the cell with the specified coordinates
         *
         * 获取指定坐标的单元格
         * @param coords table coordinates / 表格坐标
         */
        getNthCell(coords) {
            require('../../addon/table');
            return this.getNthCell(coords);
        }
        /**
         * Print the table layout
         *
         * 打印表格布局
         */
        printLayout() {
            require('../../addon/table');
            this.printLayout();
        }
        /**
         * Convert to table coordinates after rendering
         *
         * 转换为渲染后的表格坐标
         * @param {TableCoords} coord table coordinates in wikitext / wikitext中的表格坐标
         */
        toRenderedCoords(coord) {
            require('../../addon/table');
            return this.toRenderedCoords(coord);
        }
        /**
         * Convert to table coordinates in wikitext
         *
         * 转换为wikitext中的表格坐标
         * @param {TableRenderedCoords} coord table coordinates after rendering / 渲染后的表格坐标
         */
        toRawCoords(coord) {
            require('../../addon/table');
            return this.toRawCoords(coord);
        }
        /**
         * Get the full row
         *
         * 获取完整行
         * @param y row number / 行号
         */
        getFullRow(y) {
            require('../../addon/table');
            return this.getFullRow(y);
        }
        /**
         * Get the full column
         *
         * 获取完整列
         * @param x column number / 列号
         */
        getFullCol(x) {
            require('../../addon/table');
            return this.getFullCol(x);
        }
        /**
         * Format the row
         *
         * 设置行格式
         * @param y row number / 行号
         * @param attr table attribute / 表格属性
         * @param multiRow whether to format multi-row cells / 是否对所有单元格设置，或是仅对行首单元格设置
         */
        formatTableRow(y, attr, multiRow) {
            require('../../addon/table');
            this.formatTableRow(y, attr, multiRow);
        }
        /**
         * Format the column
         *
         * 设置列格式
         * @param x column number / 列号
         * @param attr table attribute / 表格属性
         * @param multiCol whether to format multi-column cells / 是否对所有单元格设置，或是仅对行首单元格设置
         */
        formatTableCol(x, attr, multiCol) {
            require('../../addon/table');
            this.formatTableCol(x, attr, multiCol);
        }
        /**
         * Fill the table row
         *
         * 填补表格行
         * @param y row number / 行号
         * @param inner content to fill / 填充内容
         * @param subtype type of the cell / 单元格类型
         * @param attr table attribute / 表格属性
         */
        fillTableRow(y, inner, subtype, attr) {
            require('../../addon/table');
            this.fillTableRow(y, inner, subtype, attr);
        }
        /**
         * Fill the table
         *
         * 填补表格
         * @param inner content to fill / 填充内容
         * @param subtype type of the cell / 单元格类型
         * @param attr table attribute / 表格属性
         */
        fillTable(inner, subtype, attr) {
            require('../../addon/table');
            this.fillTable(inner, subtype, attr);
        }
        insertTableCell(inner, coords, subtype, attr) {
            require('../../addon/table');
            return this.insertTableCell(inner, coords, subtype, attr);
        }
        /**
         * Insert a table row
         *
         * 插入表格行
         * @param y row number / 行号
         * @param attr table row attribute / 表格行属性
         * @param inner inner wikitext / 内部wikitext
         * @param subtype type of the cell / 单元格类型
         * @param innerAttr cell attribute / 单元格属性
         */
        insertTableRow(y, attr, inner, subtype, innerAttr) {
            require('../../addon/table');
            return this.insertTableRow(y, attr, inner, subtype, innerAttr);
        }
        /**
         * Insert a table column
         *
         * 插入表格列
         * @param x column number / 列号
         * @param inner inner wikitext / 内部wikitext
         * @param subtype type of the cell / 单元格类型
         * @param attr cell attribute / 单元格属性
         */
        insertTableCol(x, inner, subtype, attr) {
            require('../../addon/table');
            this.insertTableCol(x, inner, subtype, attr);
        }
        /**
         * Remove a table row
         *
         * 移除表格行
         * @param y row number / 行号
         */
        removeTableRow(y) {
            require('../../addon/table');
            return this.removeTableRow(y);
        }
        /**
         * Remove a table column
         *
         * 移除表格列
         * @param x column number / 列号
         */
        removeTableCol(x) {
            require('../../addon/table');
            this.removeTableCol(x);
        }
        /**
         * Marge cells
         *
         * 合并单元格
         * @param xlim column range / 列范围
         * @param ylim row range / 行范围
         */
        mergeCells(xlim, ylim) {
            require('../../addon/table');
            return this.mergeCells(xlim, ylim);
        }
        /**
         * Split a cell into rows
         *
         * 分裂成多行
         * @param coords coordinates of the cell / 单元格坐标
         */
        splitIntoRows(coords) {
            require('../../addon/table');
            this.splitIntoRows(coords);
        }
        /**
         * Split a cell into columns
         *
         * 分裂成多列
         * @param coords coordinates of the cell / 单元格坐标
         */
        splitIntoCols(coords) {
            require('../../addon/table');
            this.splitIntoCols(coords);
        }
        /**
         * Split a cell into cells
         *
         * 分裂成单元格
         * @param coords coordinates of the cell / 单元格坐标
         */
        splitIntoCells(coords) {
            require('../../addon/table');
            this.splitIntoCells(coords);
        }
        /**
         * Replicate a row and insert the result before the row
         *
         * 复制一行并插入该行之前
         * @param row row number / 行号
         */
        replicateTableRow(row) {
            require('../../addon/table');
            return this.replicateTableRow(row);
        }
        /**
         * Replicate a column and insert the result before the column
         *
         * 复制一列并插入该列之前
         * @param x column number / 列号
         */
        replicateTableCol(x) {
            require('../../addon/table');
            return this.replicateTableCol(x);
        }
        /**
         * Move a table row
         *
         * 移动表格行
         * @param y row number / 行号
         * @param before new position / 新位置
         */
        moveTableRowBefore(y, before) {
            require('../../addon/table');
            return this.moveTableRowBefore(y, before);
        }
        /**
         * Move a table row
         *
         * 移动表格行
         * @param y row number / 行号
         * @param after new position / 新位置
         */
        moveTableRowAfter(y, after) {
            require('../../addon/table');
            return this.moveTableRowAfter(y, after);
        }
        /**
         * Move a table column
         *
         * 移动表格列
         * @param x column number / 列号
         * @param before new position / 新位置
         */
        moveTableColBefore(x, before) {
            require('../../addon/table');
            this.moveTableColBefore(x, before);
        }
        /**
         * Move a table column
         *
         * 移动表格列
         * @param x column number / 列号
         * @param after new position / 新位置
         */
        moveTableColAfter(x, after) {
            require('../../addon/table');
            this.moveTableColAfter(x, after);
        }
        /** @private */
        toHtmlInternal(opt) {
            /**
             * 过滤需要移出表格的节点
             * @param token 表格或表格行
             */
            const filter = (token) => token.childNodes.filter((0, debug_1.isToken)('table-inter'));
            const { childNodes } = this, tr = childNodes.filter((0, debug_1.isToken)('tr')), newOpt = {
                ...opt,
                nowrap: true,
            }, firstRow = super.toHtmlInternal(opt), newline = opt?.nowrap ? ' ' : '\n';
            return `${[this, ...tr].flatMap(filter).map(token => token.toHtmlInternal(newOpt).trim()).join(' ')}<table${childNodes[1].toHtmlInternal()}>${newline}<tbody>${firstRow + (tr.length > 0 && firstRow.endsWith('</tr>') ? newline : '')}${(0, html_1.html)(tr, newline, opt)}${tr.length === 0 && !firstRow ? '<tr><td></td></tr>' : ''}</tbody></table>`;
        }
    };
})();
exports.TableToken = TableToken;
constants_1.classes['TableToken'] = __filename;
