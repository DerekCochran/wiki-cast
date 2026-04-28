"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.RedirectMap = void 0;
const constants_1 = require("../util/constants");
/**
 * 快速规范化页面标题
 * @param title 标题
 */
const normalizeTitle = (title) => {
    const Parser = require('../index');
    return String(Parser.normalizeTitle(title, 0, false, undefined, { temporary: true, page: '' }));
};
/** 重定向列表 */
class RedirectMap extends Map {
    #redirect;
    /** @ignore */
    constructor(entries, redirect = true) {
        super();
        this.#redirect = redirect;
        if (entries) {
            for (const [k, v] of Symbol.iterator in entries ? entries : Object.entries(entries)) {
                this.set(k, v);
            }
        }
    }
    set(key, value) {
        return super.set(normalizeTitle(key), this.#redirect ? normalizeTitle(value) : value);
    }
}
exports.RedirectMap = RedirectMap;
constants_1.classes['RedirectMap'] = __filename;
