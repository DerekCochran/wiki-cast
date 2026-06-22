"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.clone = void 0;
const debug_1 = require("../util/debug");
const constants_1 = require("../util/constants");
/**
 * 深拷贝节点
 * @param method 方法
 */
// eslint-disable-next-line @typescript-eslint/no-explicit-any
const clone = (method) => function () {
    const cloned = this.cloneChildNodes(), { type, name } = this;
    return debug_1.Shadow.run(() => {
        const newToken = method.call(this);
        newToken.safeAppend(cloned);
        if (type === 'ext-inner' && name) {
            newToken.setAttribute('name', name);
        }
        return newToken;
    });
};
exports.clone = clone;
constants_1.mixins['clone'] = __filename;
