"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.parseConverter = void 0;
const converter_1 = require("../src/converter");
/* NOT FOR BROWSER */
const constants_1 = require("../util/constants");
/* NOT FOR BROWSER END */
/**
 * 解析语言变体转换
 * @param text
 * @param config
 * @param accum
 */
const parseConverter = (text, config, accum) => {
    // eslint-disable-next-line @typescript-eslint/no-unused-expressions
    /;(?=(?:[^;]*?=>)?\s*zh\s*:|(?:\s|\0\d+[cn]\x7F)*$)/u;
    config.regexConverter ??= new RegExp(String.raw `;(?=(?:[^;]*?=>)?\s*(?:${config.variants.join('|')})\s*:|(?:\s|\0\d+[cn]\x7F)*$)`, 'iu');
    const regex1 = /-\{/gu, regex2 = /-\{|\}-/gu, stack = [];
    let regex = regex1, mt = regex.exec(text);
    while (mt) {
        const { 0: syntax, index } = mt;
        if (syntax === '}-') {
            const top = stack.pop(), { length } = accum, str = text.slice(top.index + 2, index), i = str.indexOf('|'), [flags, raw] = i === -1 ? [[], str] : [str.slice(0, i).split(';'), str.slice(i + 1)], temp = raw.replace(/(&[#a-z\d]+);/giu, '$1\x01'), rules = temp.split(config.regexConverter)
                .map(rule => rule.replace(/\x01/gu, ';'));
            // @ts-expect-error abstract class
            new converter_1.ConverterToken(flags, rules, config, accum);
            text = `${text.slice(0, top.index)}\0${length}v\x7F${text.slice(index + 2)}`;
            if (stack.length === 0) {
                regex = regex1;
            }
            regex.lastIndex = top.index + 3 + String(length).length;
        }
        else {
            stack.push(mt);
            regex = regex2;
            regex.lastIndex = index + 2;
        }
        mt = regex.exec(text);
    }
    return text;
};
exports.parseConverter = parseConverter;
constants_1.parsers['parseConverter'] = __filename;
