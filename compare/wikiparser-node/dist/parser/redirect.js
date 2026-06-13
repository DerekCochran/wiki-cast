"use strict";
var __importDefault = (this && this.__importDefault) || function (mod) {
    return (mod && mod.__esModule) ? mod : { "default": mod };
};
Object.defineProperty(exports, "__esModule", { value: true });
exports.parseRedirect = void 0;
const index_1 = __importDefault(require("../index"));
const redirect_1 = require("../src/redirect");
/* NOT FOR BROWSER */
const constants_1 = require("../util/constants");
/* NOT FOR BROWSER END */
/**
 * 解析重定向
 * @param text
 * @param config
 * @param accum
 */
const parseRedirect = (text, config, accum) => {
    // eslint-disable-next-line @typescript-eslint/no-unused-expressions
    /^(\s*)((?:#redirect|#重定向)\s*(?::\s*)?)\[\[([^\n|\]]+)(\|.*?)?\]\](\s*)/iu;
    config.regexRedirect ??= new RegExp(String.raw `^(\s*)((?:${config.redirection.join('|')})\s*(?::\s*)?)\[\[([^\n|\]]+)(\|.*?)?\]\](\s*)`, 'iu');
    const mt = config.regexRedirect.exec(text);
    if (mt
        && index_1.default.normalizeTitle(mt[3], 0, false, config, { halfParsed: true, temporary: true, decode: true, page: '' }).valid) {
        text = `\0${accum.length}o\x7F${text.slice(mt[0].length)}`;
        // @ts-expect-error abstract class
        new redirect_1.RedirectToken(...mt.slice(1), config, accum);
        return text;
    }
    return false;
};
exports.parseRedirect = parseRedirect;
constants_1.parsers['parseRedirect'] = __filename;
