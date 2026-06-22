"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.noEscape = void 0;
const debug_1 = require("../util/debug");
/* NOT FOR BROWSER */
const constants_1 = require("../util/constants");
/* NOT FOR BROWSER END */
/**
 * 不需要转义的类
 * @ignore
 */
const noEscape = (constructor) => {
    LSP: {
        class NoEscapeToken extends constructor {
            escape() {
                //
            }
        }
        (0, debug_1.mixin)(NoEscapeToken, constructor);
        return NoEscapeToken;
    }
};
exports.noEscape = noEscape;
constants_1.mixins['noEscape'] = __filename;
