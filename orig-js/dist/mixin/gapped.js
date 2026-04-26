"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.gapped = void 0;
const debug_1 = require("../util/debug");
/* NOT FOR BROWSER */
const constants_1 = require("../util/constants");
/* NOT FOR BROWSER END */
/**
 * 给定 gap 的类
 * @param gap
 */
const gapped = (gap = 1) => (constructor) => {
    class GappedToken extends constructor {
        getGaps() {
            return gap;
        }
    }
    (0, debug_1.mixin)(GappedToken, constructor);
    return GappedToken;
};
exports.gapped = gapped;
constants_1.mixins['gapped'] = __filename;
