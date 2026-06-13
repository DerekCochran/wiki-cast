"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.parseHrAndDoubleUnderscore = void 0;
const cm_util_1 = require("@bhsd/cm-util");
const hr_1 = require("../src/nowiki/hr");
const doubleUnderscore_1 = require("../src/nowiki/doubleUnderscore");
const heading_1 = require("../src/heading");
/* NOT FOR BROWSER */
const constants_1 = require("../util/constants");
/* NOT FOR BROWSER END */
/**
 * 解析`<hr>`和状态开关
 * @param {Token} root 根节点
 * @param config
 * @param accum
 */
const parseHrAndDoubleUnderscore = ({ firstChild: { data }, type, name }, config, accum) => {
    const [insensitive, sensitive, aliases] = config.doubleUnderscore, all = [...insensitive, ...sensitive];
    config.insensitiveDoubleUnderscore ??= new Set(insensitive.filter(cm_util_1.isUnderscore));
    config.sensitiveDoubleUnderscore ??= new Set(sensitive.filter(cm_util_1.isUnderscore));
    // eslint-disable-next-line @typescript-eslint/no-unused-expressions
    /^((?:\0\d+[cno]\x7F)*)(-{4,})|__(toc|notoc)__|＿{2}(目次)＿{2}/gimu;
    config.regexHrAndDoubleUnderscore ??= new RegExp(String.raw `^((?:\0\d+[cno]\x7F)*)(-{4,})|__(${all.filter(cm_util_1.isUnderscore).join('|')})__|＿{2}(${all.filter(s => !(0, cm_util_1.isUnderscore)(s)).map(s => s.slice(2, -2)).join('|')})＿{2}`, 'gimu');
    if (type !== 'root' && (type !== 'ext-inner' || name !== 'poem')) {
        data = `\0${data}`;
    }
    data = data.replace(config.regexHrAndDoubleUnderscore, (m, p1, p2, p3, p4) => {
        if (p2) {
            // @ts-expect-error abstract class
            new hr_1.HrToken(p2, config, accum);
            return `${p1}\0${accum.length - 1}r\x7F`;
        }
        const key = p3 ?? p4, caseSensitive = config.sensitiveDoubleUnderscore.has(key), lc = key.toLowerCase(), caseInsensitive = config.insensitiveDoubleUnderscore.has(lc);
        if (caseSensitive || caseInsensitive) {
            // @ts-expect-error abstract class
            new doubleUnderscore_1.DoubleUnderscoreToken(key, caseSensitive, Boolean(p4), config, accum);
            return `\0${accum.length - 1}${caseInsensitive && (aliases?.[lc] ?? /* c8 ignore next */ lc) === 'toc' ? 'u' : 'n'}\x7F`;
        }
        return m;
    });
    if (!config.excludes.includes('heading')) {
        data = data.replace(/^((?:\0\d+[cn]\x7F)*)(={1,6})(.+)\2((?:\s|\0\d+[cn]\x7F)*)$/gmu, (_, lead, equals, heading, trail) => {
            const text = `${lead}\0${accum.length}h\x7F`;
            // @ts-expect-error abstract class
            new heading_1.HeadingToken(equals.length, [heading, trail], config, accum);
            return text;
        });
    }
    return type === 'root' || type === 'ext-inner' && name === 'poem' ? data : data.slice(1);
};
exports.parseHrAndDoubleUnderscore = parseHrAndDoubleUnderscore;
constants_1.parsers['parseHrAndDoubleUnderscore'] = __filename;
