"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.singleLine = void 0;
const debug_1 = require("../util/debug");
const constants_1 = require("../util/constants");
/**
 * 不可包含换行符的类
 * @ignore
 */
const singleLine = (constructor) => {
    class SingleLineToken extends constructor {
        toString(skip) {
            if (this.parentNode?.name === 'inputbox') {
                return this.childNodes.map(child => {
                    const str = child.toString(skip), { type } = child;
                    return type === 'comment' || type === 'include' || type === 'ext'
                        ? str
                        : str.replaceAll('\n', ' ');
                }).join('');
            }
            return super.toString(skip).replaceAll('\n', ' ');
        }
        text() {
            return super.text().replaceAll('\n', ' ');
        }
    }
    (0, debug_1.mixin)(SingleLineToken, constructor);
    return SingleLineToken;
};
exports.singleLine = singleLine;
constants_1.mixins['singleLine'] = __filename;
