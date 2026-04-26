"use strict";
var __importDefault = (this && this.__importDefault) || function (mod) {
    return (mod && mod.__esModule) ? mod : { "default": mod };
};
Object.defineProperty(exports, "__esModule", { value: true });
exports.readOnly = void 0;
const debug_1 = require("../util/debug");
const constants_1 = require("../util/constants");
const index_1 = __importDefault(require("../index"));
/**
 * 只读或可写的方法
 * @param readonly 是否只读
 */
const readOnly = (readonly = false) => 
// eslint-disable-next-line @typescript-eslint/no-explicit-any
(method) => function (...args) {
    const { viewOnly } = index_1.default;
    if (!debug_1.Shadow.running) {
        index_1.default.viewOnly = readonly;
    }
    const result = method.apply(this, args);
    if (!debug_1.Shadow.running) {
        index_1.default.viewOnly = viewOnly;
    }
    return result;
};
exports.readOnly = readOnly;
constants_1.mixins['readOnly'] = __filename;
