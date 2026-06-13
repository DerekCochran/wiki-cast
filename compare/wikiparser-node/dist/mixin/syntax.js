"use strict";
var __importDefault = (this && this.__importDefault) || function (mod) {
    return (mod && mod.__esModule) ? mod : { "default": mod };
};
Object.defineProperty(exports, "__esModule", { value: true });
exports.syntax = void 0;
const debug_1 = require("../util/debug");
const constants_1 = require("../util/constants");
const string_1 = require("../util/string");
const index_1 = __importDefault(require("../index"));
/**
 * 满足特定语法格式的Token
 * @param pattern 语法正则
 */
const syntax = (pattern) => (constructor) => {
    class SyntaxToken extends constructor {
        constructor(...args) {
            super(...args); // eslint-disable-line @typescript-eslint/no-unsafe-argument
            if (pattern) {
                this.pattern = pattern;
            }
            this.seal('pattern', true);
        }
        afterBuild() {
            super.afterBuild();
            if (!index_1.default.internal) {
                const /** @implements */ syntaxListener = (e, data) => {
                    if (!debug_1.Shadow.running && !this.pattern.test((0, string_1.text)(this.childNodes))) {
                        (0, debug_1.undo)(e, data);
                        this.constructorError('cannot modify the syntax pattern');
                    }
                };
                this.addEventListener(['remove', 'insert', 'replace', 'text'], syntaxListener);
            }
        }
        safeReplaceChildren(elements) {
            if (debug_1.Shadow.running || this.pattern.test((0, string_1.text)(elements))) {
                debug_1.Shadow.run(() => {
                    super.safeReplaceChildren(elements);
                });
            }
        }
    }
    (0, debug_1.mixin)(SyntaxToken, constructor);
    return SyntaxToken;
};
exports.syntax = syntax;
constants_1.mixins['syntax'] = __filename;
