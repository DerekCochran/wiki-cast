"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.cached = void 0;
const lint_1 = require("../util/lint");
/* NOT FOR BROWSER */
const constants_1 = require("../util/constants");
/* NOT FOR BROWSER END */
/**
 * 缓存计算结果
 * @param force 是否强制缓存
 */
const cached = (force = true) => (method) => {
    const stores = new WeakMap();
    // eslint-disable-next-line @typescript-eslint/no-explicit-any
    return function (...args) {
        return (0, lint_1.cache)(stores.get(this), () => method.apply(this, args), value => {
            stores.set(this, value);
        }, force);
    };
};
exports.cached = cached;
constants_1.mixins['cached'] = __filename;
