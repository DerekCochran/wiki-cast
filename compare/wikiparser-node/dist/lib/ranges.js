"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.Ranges = exports.Range = void 0;
const constants_1 = require("../util/constants");
const diff_1 = require("../util/diff");
/** 模拟Python的Range对象。除`step`至少为`1`外，允许负数、小数或`end < start`的情形。 */
class Range {
    // @ts-expect-error lazy initialization
    start;
    // @ts-expect-error lazy initialization
    end;
    // @ts-expect-error lazy initialization
    step;
    /**
     * @param str 表达式
     * @throws `RangeError` 起点、终点和步长均应为整数
     * @throws `RangeError` n的系数不能为0
     * @throws `RangeError` 应使用CSS选择器或Python切片的格式
     */
    constructor(str) {
        str = str.trim();
        if (str === 'odd') {
            Object.assign(this, { start: 1, end: Infinity, step: 2 });
        }
        else if (str === 'even') {
            Object.assign(this, { start: 0, end: Infinity, step: 2 });
        }
        else if (str.includes(':')) {
            const [start, end, step = '1'] = str
                .split(':', 3);
            this.start = Number(start);
            this.end = Number(end?.trim() || Infinity);
            this.step = Math.max(Number(step), 1);
            /* c8 ignore next 9 */
            if (!Number.isInteger(this.start)) {
                throw new RangeError(`The start of a range, \`${start}\`, should be an integer!`);
            }
            if (this.end !== Infinity && !Number.isInteger(this.end)) {
                throw new RangeError(`The end of a range, \`${end}\`, should be an integer!`);
            }
            if (!Number.isInteger(this.step)) {
                throw new RangeError(`The step of a range, \`${step}\`, should be an integer!`);
            }
        }
        else {
            const mt = /^([+-])?(\d+)?n(?:\s*([+-])\s*(\d+))?$/u
                .exec(str);
            /* c8 ignore next 3 */
            if (!mt) {
                throw new RangeError(`The argument \`${str}\` should be either in the form of "an+b" as in CSS selectors or Python slices!`);
            }
            const [, sgnA = '+', a = 1, sgnB = '+'] = mt, b = Number(mt[4] ?? 0);
            this.step = Number(a);
            /* c8 ignore next 3 */
            if (this.step === 0) {
                throw new RangeError(`In the argument \`${str}\`, the coefficient of "n" must not be 0!`);
            }
            else if (sgnA === '+') { // `an+b` or `an-b`
                this.start = sgnB === '+' || b === 0 ? b : this.step - 1 - (b - 1) % this.step;
                this.end = Infinity;
            }
            else if (sgnB === '-') { // `-an-b`
                this.start = 0;
                this.end = b > 0 ? 0 : this.step;
            }
            else { // `-an+b`
                this.start = b % this.step;
                this.end = this.step + b;
            }
        }
    }
    /** @private */
    has(i, length) {
        let { start, end } = this;
        start += start < 0 ? length : 0;
        end += end < 0 ? length : 0;
        return i >= start && i < end && (i - Math.max(start, 0)) % this.step === 0;
    }
}
exports.Range = Range;
/** @extends {Array<number|Range>} */
class Ranges extends Array {
    /** @param a 表达式数组 */
    constructor(a) {
        super();
        if (a === undefined) {
            return;
        }
        for (const ele of (Array.isArray(a) ? a : [a])) {
            if (ele instanceof Range) {
                this.push(ele);
                continue;
            }
            else if (typeof ele === 'string' && !ele.trim()) {
                continue;
            }
            const number = Number(ele);
            if (Number.isInteger(number)) {
                this.push(number);
            }
            else if (typeof ele === 'string' && Number.isNaN(number)) {
                try {
                    this.push(new Range(ele));
                }
                catch (e) /* c8 ignore start */ {
                    if (e instanceof RangeError) {
                        (0, diff_1.error)(e.message);
                    }
                }
                /* c8 ignore stop */
            }
        }
    }
    /**
     * 是否包含指定的索引
     * @param i 指定的索引
     * @param length 序列的长度
     */
    has(i, length) {
        return i >= 0 && i < length
            && this.some(ele => typeof ele === 'number' ? ele === i || ele + length === i : ele.has(i, length));
    }
}
exports.Ranges = Ranges;
constants_1.classes['Ranges'] = __filename;
