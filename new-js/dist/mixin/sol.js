"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.sol = void 0;
const debug_1 = require("../util/debug");
const constants_1 = require("../util/constants");
/**
 * 只能位于行首的类
 * @param self 是否允许同类节点相邻
 */
const sol = (self) => (constructor) => {
    class SolToken extends constructor {
        #prependNewLine() {
            const { previousVisibleSibling, parentNode, type } = this;
            if (previousVisibleSibling) {
                return self && previousVisibleSibling.type === type
                    || previousVisibleSibling.toString().endsWith('\n')
                    ? ''
                    : '\n';
            }
            return parentNode?.type === 'root'
                || type === 'list' && parentNode?.is('list-range')
                || parentNode?.type === 'ext-inner' && parentNode.name === 'poem'
                ? ''
                : '\n';
        }
        toString(skip) {
            return this.#prependNewLine() + super.toString(skip);
        }
        getAttribute(key) {
            return key === 'padding'
                ? this.#prependNewLine().length + super.getAttribute('padding')
                : super.getAttribute(key);
        }
        text() {
            return this.#prependNewLine() + super.text();
        }
    }
    (0, debug_1.mixin)(SolToken, constructor);
    return SolToken;
};
exports.sol = sol;
constants_1.mixins['sol'] = __filename;
