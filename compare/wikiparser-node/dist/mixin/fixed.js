"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.fixedToken = void 0;
const debug_1 = require("../util/debug");
const constants_1 = require("../util/constants");
/**
 * 不可增删子节点的类
 * @ignore
 */
const fixedToken = (constructor) => {
    class FixedToken extends constructor {
        get fixed() {
            return true;
        }
        removeAt() {
            this.constructorError('cannot remove child nodes');
        }
        insertAt(token, i) {
            return debug_1.Shadow.running
                ? super.insertAt(token, i)
                : this.constructorError('cannot insert child nodes');
        }
    }
    (0, debug_1.mixin)(FixedToken, constructor);
    return FixedToken;
};
exports.fixedToken = fixedToken;
constants_1.mixins['fixedToken'] = __filename;
