"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.getCondition = exports.basic = void 0;
/**
 * type和name选择器
 * @param selector
 */
const basic = (selector) => {
    if (selector.includes('#')) {
        const i = selector.indexOf('#'), targetType = selector.slice(0, i), targetName = selector.slice(i + 1);
        return (type, name) => (i === 0 || type === targetType) && name === targetName;
    }
    return type => type === selector;
};
exports.basic = basic;
/**
 * 将选择器转化为类型谓词
 * @param selector 选择器
 * @param scope 作用对象
 * @param has `:has()`伪选择器
 */
const getCondition = (selector, scope, has) => {
    selector = selector.trim();
    /* NOT FOR BROWSER */
    if (/[^a-z\-,#\s]|(?<![\s,])\s+(?![\s,])/u.test(selector)) {
        const { checkToken } = require('../parser/selector');
        return checkToken(selector, scope, has);
    }
    /* NOT FOR BROWSER END */
    /* c8 ignore next 3 */
    if (!selector) {
        return (() => true);
    }
    const parts = selector.split(',').map(str => str.trim()).filter(str => str !== '').map(exports.basic);
    return (({ type, name }) => parts.some(condition => condition(type, name)));
};
exports.getCondition = getCondition;
