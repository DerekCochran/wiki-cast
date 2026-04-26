"use strict";
var __importDefault = (this && this.__importDefault) || function (mod) {
    return (mod && mod.__esModule) ? mod : { "default": mod };
};
Object.defineProperty(exports, "__esModule", { value: true });
const binary_search_1 = __importDefault(require("binary-search"));
/**
 * 二分法查找索引
 * @param haystack 数组
 * @param needle 目标值
 * @param comparator 比较函数
 */
exports.default = (haystack, needle, comparator) => {
    const found = (0, binary_search_1.default)(haystack, needle, comparator);
    return found < 0 ? ~found : found; // eslint-disable-line no-bitwise
};
