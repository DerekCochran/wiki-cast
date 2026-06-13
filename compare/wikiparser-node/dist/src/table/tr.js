"use strict";
var _a;
Object.defineProperty(exports, "__esModule", { value: true });
exports.TrToken = void 0;
const trBase_1 = require("./trBase");
/* NOT FOR BROWSER */
const constants_1 = require("../../util/constants");
/* NOT FOR BROWSER END */
/**
 * table row that contains the newline at the beginning but not at the end
 *
 * 表格行，含开头的换行，不含结尾的换行
 * @classdesc `{childNodes: [SyntaxToken, AttributesToken, ?Token, ...TdToken[]]}`
 */
class TrToken extends trBase_1.TrBaseToken {
    /* NOT FOR BROWSER END */
    get type() {
        return 'tr';
    }
    /**
     * @param syntax 表格语法
     * @param attr 表格属性
     */
    constructor(syntax, attr, config, accum) {
        super(/^\n[^\S\n]*(?:\|-+|\{\{\s*!\s*\}\}-+|\{\{\s*!-\s*\}\}-*)$/u, syntax, 'tr', attr, config, accum, { Token: 2, SyntaxToken: 0, AttributesToken: 1, TdToken: '2:' });
    }
    /* NOT FOR BROWSER */
    /** @private */
    text() {
        const str = super.text();
        return str.trim().includes('\n') ? str : '';
    }
    /**
     * 获取相邻行
     * @param subset 筛选兄弟节点的方法
     */
    #getSiblingRow(subset) {
        const { parentNode } = this;
        if (!parentNode) {
            return undefined;
        }
        const { childNodes } = parentNode, index = childNodes.indexOf(this);
        for (const child of subset(childNodes, index)) {
            if (child instanceof _a && child.getRowCount()) {
                return child;
            }
        }
        return undefined;
    }
    /**
     * Get the next row
     *
     * 获取下一行
     */
    getNextRow() {
        return this.#getSiblingRow((childNodes, index) => childNodes.slice(index + 1));
    }
    /**
     * Get the previous row
     *
     * 获取前一行
     */
    getPreviousRow() {
        return this.#getSiblingRow((childNodes, index) => childNodes.slice(0, index).reverse());
    }
}
exports.TrToken = TrToken;
_a = TrToken;
constants_1.classes['TrToken'] = __filename;
