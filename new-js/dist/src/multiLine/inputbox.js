"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.InputboxToken = void 0;
const commentAndExt_1 = require("../../parser/commentAndExt");
const braces_1 = require("../../parser/braces");
const paramTag_1 = require("./paramTag");
/* NOT FOR BROWSER */
const constants_1 = require("../../util/constants");
/* NOT FOR BROWSER END */
/** `<inputbox>` */
class InputboxToken extends paramTag_1.ParamTagToken {
    /** @param name 扩展标签名 */
    constructor(name, include, wikitext, config, accum = []) {
        const placeholder = Symbol('InputboxToken'), newConfig = config.excludes.includes('heading')
            ? config
            : {
                ...config,
                excludes: [...config.excludes, 'heading'],
            }, { length } = accum;
        accum.push(placeholder);
        wikitext &&= (0, commentAndExt_1.parseCommentAndExt)(wikitext, newConfig, accum, include);
        wikitext &&= (0, braces_1.parseBraces)(wikitext, newConfig, accum);
        accum.splice(length, 1);
        super(name, include, wikitext, newConfig, accum, {
            ArgToken: ':', TranscludeToken: ':',
        });
    }
}
exports.InputboxToken = InputboxToken;
constants_1.classes['InputboxToken'] = __filename;
