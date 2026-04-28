"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.parseTable = void 0;
const index_1 = require("../src/index");
const index_2 = require("../src/table/index");
const tr_1 = require("../src/table/tr");
const td_1 = require("../src/table/td");
const dd_1 = require("../src/nowiki/dd");
/* NOT FOR BROWSER */
const constants_1 = require("../util/constants");
/* NOT FOR BROWSER END */
/**
 * 判断是否为表格行或表格
 * @param token 表格节点
 */
const isTr = (token) => token.lastChild.constructor !== index_1.Token;
/**
 * 取出最近的表格行
 * @param top 当前解析的表格或表格行
 * @param stack 表格栈
 */
const pop = (top, stack) => top.is('td') ? stack.pop() : top;
/**
 * 解析表格，注意`tr`和`td`包含开头的换行
 * @param {Token & {firstChild: AstText}} root 根节点
 * @param config
 * @param accum
 */
const parseTable = ({ firstChild: { data }, type, name }, config, accum) => {
    const stack = [], lines = data.split('\n');
    let out = type === 'root' || type === 'parameter-value' || type === 'ext-inner' && name === 'poem'
        ? ''
        : `\n${lines.shift()}`, top;
    /**
     * 向表格中插入纯文本
     * @param str 待插入的文本
     * @param topToken 当前解析的表格或表格行
     */
    const push = (str, topToken) => {
        if (!topToken) {
            out += str;
            return;
        }
        const { lastChild } = topToken;
        if (isTr(topToken)) {
            const token = new index_1.Token(str, config, accum);
            token.type = 'table-inter';
            token.setAttribute('stage', 3);
            topToken.insertAt(token);
        }
        else {
            lastChild.setText(lastChild.toString() + str);
        }
    };
    for (const outLine of lines) {
        top = stack.pop();
        const [spaces] = /^(?:\s|\0\d+[cno]\x7F)*/u.exec(outLine), line = outLine.slice(spaces.length), matchesStart = /^(:*)((?:\s|\0\d+[cn]\x7F)*)(\{\||\{(?:\0\d+[cn]\x7F)*\0\d+!\x7F|\0\d+\{\x7F)(.*)$/u
            .exec(line);
        if (matchesStart) {
            while (top && !top.is('td')) {
                top = stack.pop();
            }
            const [, indent, moreSpaces, tableSyntax, attr] = matchesStart;
            if (indent) {
                // @ts-expect-error abstract class
                new dd_1.DdToken(indent, config, accum);
            }
            push(`\n${spaces}${indent && `\0${accum.length - 1}d\x7F`}${moreSpaces}\0${accum.length}b\x7F`, top);
            // @ts-expect-error abstract class
            stack.push(...top ? [top] : [], new index_2.TableToken(tableSyntax, attr, config, accum));
            continue;
        }
        else if (!top) {
            out += `\n${outLine}`;
            continue;
        }
        const matches = /^(?:(\|\}|\0\d+!\x7F\}|\0\d+\}\x7F)|(\|-+|\0\d+!\x7F-+|\0\d+-\x7F-*)(?!-)|(!|(?:\||\0\d+!\x7F)\+?))(.*)$/u
            .exec(line);
        if (!matches) {
            push(`\n${outLine}`, top);
            stack.push(top);
            continue;
        }
        const [, closing, row, cell, attr] = matches;
        if (closing) {
            while (!top.is('table')) {
                top = stack.pop();
            }
            top.close(`\n${spaces}${closing}`, true);
            push(attr, stack[stack.length - 1]);
        }
        else if (row) {
            top = pop(top, stack);
            if (top.is('tr')) {
                top = stack.pop();
            }
            // @ts-expect-error abstract class
            const tr = new tr_1.TrToken(`\n${spaces}${row}`, attr, config, accum);
            stack.push(top, tr);
            top.insertAt(tr);
        }
        else {
            top = pop(top, stack);
            const regex = cell === '!' ? /!!|(?:\||\0\d+!\x7F){2}|\0\d+\+\x7F/gu : /(?:\||\0\d+!\x7F){2}|\0\d+\+\x7F/gu;
            let mt = regex.exec(attr), lastIndex = 0, lastSyntax = `\n${spaces}${cell}`;
            /**
             * 创建表格单元格
             * @param tr 当前表格行
             */
            const newTd = (tr) => {
                // @ts-expect-error abstract class
                const td = new td_1.TdToken(lastSyntax, attr.slice(lastIndex, mt?.index), config, accum);
                tr.insertAt(td);
                return td;
            };
            while (mt) {
                newTd(top);
                ({ lastIndex } = regex);
                [lastSyntax] = mt;
                mt = regex.exec(attr);
            }
            stack.push(top, newTd(top));
        }
    }
    return out.slice(1);
};
exports.parseTable = parseTable;
constants_1.parsers['parseTable'] = __filename;
