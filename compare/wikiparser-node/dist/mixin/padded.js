"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.padded = void 0;
const debug_1 = require("../util/debug");
/* NOT FOR BROWSER */
const constants_1 = require("../util/constants");
/* NOT FOR BROWSER END */
/**
 * 给定 padding 的类
 * @param padding padding 字符串
 * @param padding.length
 */
const padded = ({ length }) => (constructor) => {
    class PaddedToken extends constructor {
        getAttribute(key) {
            return key === 'padding' ? length : super.getAttribute(key);
        }
    }
    (0, debug_1.mixin)(PaddedToken, constructor);
    return PaddedToken;
};
exports.padded = padded;
constants_1.mixins['padded'] = __filename;
