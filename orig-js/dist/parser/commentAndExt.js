"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.parseCommentAndExt = void 0;
const common_1 = require("@bhsd/common");
const string_1 = require("../util/string");
const onlyinclude_1 = require("../src/onlyinclude");
const noinclude_1 = require("../src/nowiki/noinclude");
const include_1 = require("../src/tagPair/include");
const ext_1 = require("../src/tagPair/ext");
const comment_1 = require("../src/nowiki/comment");
/* NOT FOR BROWSER */
const constants_1 = require("../util/constants");
/* NOT FOR BROWSER END */
const onlyincludeLeft = '<onlyinclude>', onlyincludeRight = '</onlyinclude>', { length } = onlyincludeLeft, getExtRegex = [false, true].map(includeOnly => {
    const noincludeRegex = includeOnly ? 'includeonly' : '(?:no|only)include', includeRegex = includeOnly ? 'noinclude' : 'includeonly';
    // eslint-disable-next-line @typescript-eslint/no-unused-expressions
    /<!--[\s\S]*?(?:-->|$)|<foo(?:\s[^>]*)?\/?>|<\/foo\s*>|<(bar)(\s[^>]*?)?(?:\/>|>([\s\S]*?)<\/(\1\s*)>)|<(baz)(\s[^>]*?)?(?:\/>|>([\s\S]*?)(?:<\/(baz\s*)>|$))/giu;
    return (0, common_1.getRegex)(exts => new RegExp(String.raw `<!--[\s\S]*?(?:-->|$)|<${noincludeRegex}(?:\s[^>]*)?/?>|</${noincludeRegex}\s*>|<(${exts // eslint-disable-next-line unicorn/prefer-string-raw
    })(\s[^>]*?)?(?:/>|>([\s\S]*?)</(${'\\1'}\s*)>)|<(${includeRegex})(\s[^>]*?)?(?:/>|>([\s\S]*?)(?:</(${includeRegex}\s*)>|$))`, 'giu'));
});
/**
 * 更新`<onlyinclude>`和`</onlyinclude>`的位置
 * @param wikitext
 */
const update = (wikitext) => {
    const i = wikitext.indexOf(onlyincludeLeft);
    return { i, j: wikitext.indexOf(onlyincludeRight, i + length) };
};
/**
 * 解析HTML注释和扩展标签
 * @param wikitext
 * @param config
 * @param accum
 * @param includeOnly 是否嵌入
 */
const parseCommentAndExt = (wikitext, config, accum, includeOnly) => {
    if (includeOnly) {
        let { i, j } = update(wikitext);
        if (i !== -1 && j !== -1) { // `<onlyinclude>`拥有最高优先级
            let str = '';
            /**
             * 忽略未被`<onlyinclude>`和`</onlyinclude>`包裹的内容
             * @param text 未被包裹的内容
             */
            const noinclude = (text) => {
                // @ts-expect-error abstract class
                new noinclude_1.NoincludeToken(text, config, accum);
                str += `\0${accum.length - 1}n\x7F`;
            };
            while (i !== -1 && j !== -1) {
                const token = `\0${accum.length}g\x7F`;
                new onlyinclude_1.OnlyincludeToken(wikitext.slice(i + length, j), config, accum);
                if (i > 0) {
                    noinclude(wikitext.slice(0, i));
                }
                str += token;
                wikitext = wikitext.slice(j + length + 1);
                ({ i, j } = update(wikitext));
            }
            if (wikitext) {
                noinclude(wikitext);
            }
            return str;
        }
    }
    const { ext } = config;
    let newExt = ext, newConfig = config;
    if (ext.includes('translate')) {
        const { TranslateToken } = require('../src/tagPair/translate');
        const stack = [];
        newExt = ext.filter(e => e !== 'translate' && e !== 'tvar');
        newConfig = { ...config, ext: newExt };
        wikitext = wikitext.replace(/<nowiki>[\s\S]*?<\/nowiki>/giu, m => {
            stack.push(m);
            return `\0${stack.length - 1}\x7F`;
        }).replace(/<translate( nowrap)?>([\s\S]*?)<\/translate>/gu, (_, p1, p2) => {
            const l = accum.length;
            // @ts-expect-error abstract class
            new TranslateToken(p1, p2 && (0, string_1.restore)(p2, stack), newConfig, accum);
            return `\0${l}g\x7F`;
        });
        wikitext = (0, string_1.restore)(wikitext, stack);
    }
    const re = getExtRegex[includeOnly ? 1 : 0](newExt.join('|'));
    re.lastIndex = 0;
    return re.test(wikitext)
        ? wikitext.replace(re, (substr, name, attr, inner, closing, include, includeAttr, includeInner, includeClosing) => {
            const l = accum.length;
            let ch = 'n';
            if (name) {
                ch = 'e';
                // @ts-expect-error abstract class
                new ext_1.ExtToken(name, attr, inner, closing, newConfig, include, accum);
            }
            else if (substr.startsWith('<!--')) {
                ch = 'c';
                const closed = substr.endsWith('-->');
                // @ts-expect-error abstract class
                new comment_1.CommentToken((0, string_1.restore)(substr, accum, 1).slice(4, closed ? -3 : undefined), closed, config, accum);
            }
            else if (include) {
                // @ts-expect-error abstract class
                new include_1.IncludeToken(include, includeAttr && (0, string_1.restore)(includeAttr, accum, 1), includeInner && (0, string_1.restore)(includeInner, accum, 1), includeClosing, config, accum);
            }
            else {
                // @ts-expect-error abstract class
                new noinclude_1.NoincludeToken(substr, config, accum, true);
            }
            return `\0${l}${ch}\x7F`;
        })
        : wikitext;
};
exports.parseCommentAndExt = parseCommentAndExt;
constants_1.parsers['parseCommentAndExt'] = __filename;
