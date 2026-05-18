export var Mode;
(function (Mode) {
    Mode[Mode["document"] = 0] = "document";
    Mode[Mode["minimal"] = 1] = "minimal";
})(Mode || (Mode = {}));
export var ProcessingStatus;
(function (ProcessingStatus) {
    ProcessingStatus[ProcessingStatus["ok"] = 0] = "ok";
    ProcessingStatus[ProcessingStatus["break"] = 1] = "break";
    ProcessingStatus[ProcessingStatus["error"] = 2] = "error";
    ProcessingStatus[ProcessingStatus["error_break"] = 3] = "error_break";
    ProcessingStatus[ProcessingStatus["fatal"] = 4] = "fatal";
})(ProcessingStatus || (ProcessingStatus = {}));
export var IoStatus;
(function (IoStatus) {
    IoStatus[IoStatus["ok"] = 0] = "ok";
    IoStatus[IoStatus["error"] = 1] = "error";
    IoStatus[IoStatus["not_found"] = 2] = "not_found";
    IoStatus[IoStatus["read_error"] = 3] = "read_error";
    IoStatus[IoStatus["permissions_error"] = 4] = "permissions_error";
})(IoStatus || (IoStatus = {}));
export var Severity;
(function (Severity) {
    Severity[Severity["min"] = 0] = "min";
    Severity[Severity["trace"] = 10] = "trace";
    Severity[Severity["debug"] = 20] = "debug";
    Severity[Severity["info"] = 30] = "info";
    Severity[Severity["soft_warning"] = 40] = "soft_warning";
    Severity[Severity["warning"] = 50] = "warning";
    Severity[Severity["error"] = 70] = "error";
    Severity[Severity["fatal"] = 90] = "fatal";
    Severity[Severity["none"] = 100] = "none";
})(Severity || (Severity = {}));
export var SyntaxHighlightPolicy;
(function (SyntaxHighlightPolicy) {
    SyntaxHighlightPolicy[SyntaxHighlightPolicy["fall_back"] = 0] = "fall_back";
    SyntaxHighlightPolicy[SyntaxHighlightPolicy["exclusive"] = 1] = "exclusive";
})(SyntaxHighlightPolicy || (SyntaxHighlightPolicy = {}));
export const MASK_64 = (1n << 64n) - 1n;
export const POW_2_127 = 1n << 127n;
export var DivRounding;
(function (DivRounding) {
    DivRounding[DivRounding["to_zero"] = 0] = "to_zero";
    DivRounding[DivRounding["to_pos_inf"] = 1] = "to_pos_inf";
    DivRounding[DivRounding["to_neg_inf"] = 2] = "to_neg_inf";
})(DivRounding || (DivRounding = {}));
export var FromStringStatus;
(function (FromStringStatus) {
    FromStringStatus[FromStringStatus["small_result"] = 0] = "small_result";
    FromStringStatus[FromStringStatus["big_result"] = 1] = "big_result";
    FromStringStatus[FromStringStatus["invalid_argument"] = 2] = "invalid_argument";
    FromStringStatus[FromStringStatus["result_out_of_range"] = 3] = "result_out_of_range";
})(FromStringStatus || (FromStringStatus = {}));
export class BigIntApi {
    repository = new Map();
    currentId = 0;
    environment;
    constructor(environment) {
        this.environment = environment;
    }
    generateId() {
        const maxId = 0xffff_ffff;
        if (this.currentId >= maxId) {
            throw new Error(`Maximum number of integer allocations reached (${maxId}).`);
        }
        return ++this.currentId;
    }
    static join_i128(lo, hi) {
        return (hi << 64n) | (lo & MASK_64);
    }
    static join_i192(btm, mid, top) {
        // No mask is applied to top since we want the result
        // to have the same sign.
        return (top << 128n)
            | ((mid & MASK_64) << 64n)
            | (btm & MASK_64);
    }
    static doCompare(x, y) {
        if (x < y)
            return -1;
        if (x > y)
            return 1;
        return 0;
    }
    get(id) {
        const value = this.repository.get(id);
        if (value === undefined) {
            throw new Error(`BigInt with id ${id} not found.`);
        }
        return value;
    }
    yieldBigResult(x) {
        const id = this.generateId();
        this.repository.set(id, BigInt(x));
        return id;
    }
    yieldResult(x) {
        if (x < POW_2_127 && x >= -POW_2_127) {
            this.environment.setSmallResult(x);
            return 0;
        }
        return this.yieldBigResult(x);
    }
    big_int_i32(value) {
        return this.yieldBigResult(BigInt(value));
    }
    big_int_i64(value) {
        return this.yieldBigResult(value);
    }
    big_int_i128(lo, hi) {
        return this.yieldBigResult(BigIntApi.join_i128(lo, hi));
    }
    big_int_i192(btm, mid, top) {
        return this.yieldBigResult(BigIntApi.join_i192(btm, mid, top));
    }
    big_int_pow2_i32(exponent) {
        const result = exponent < 0 ? 0n : 1n << BigInt(exponent);
        return this.yieldResult(result);
    }
    big_int_delete(id) {
        return this.repository.delete(id);
    }
    big_int_compare_i32(x, y) {
        return BigIntApi.doCompare(this.get(x), BigInt(y));
    }
    big_int_compare_i128(x, y_lo, y_hi) {
        return BigIntApi.doCompare(this.get(x), BigIntApi.join_i128(y_lo, y_hi));
    }
    big_int_compare(x, y) {
        return BigIntApi.doCompare(this.get(x), this.get(y));
    }
    big_int_twos_width(x) {
        const value = this.get(x);
        if (value > 0n) {
            return value.toString(2).length + 1;
        }
        if (value < 0n) {
            const abs = -value;
            const isPow2 = (abs & (abs - 1n)) === 0n;
            return abs.toString(2).length + (isPow2 ? 0 : 1);
        }
        return 1;
    }
    big_int_ones_width(x) {
        const value = this.get(x);
        if (value === 0n) {
            return 1;
        }
        const string = value.toString(2);
        return value >= 0n ? string.length + 1 : string.length;
    }
    big_int_neg(x) {
        return this.yieldResult(-this.get(x));
    }
    big_int_bit_not(x) {
        return this.yieldResult(~this.get(x));
    }
    big_int_abs(x) {
        const value = this.get(x);
        return this.yieldResult(value < 0n ? -value : value);
    }
    big_int_trunc_i128(x) {
        const value = this.get(x);
        const truncated = BigInt.asIntN(128, value);
        this.environment.setSmallResult(truncated);
        return truncated !== value;
    }
    big_int_add_i32(x, y) {
        return this.yieldResult(this.get(x) + BigInt(y));
    }
    big_int_add_i128(x, y_lo, y_hi) {
        const y = BigIntApi.join_i128(y_lo, y_hi);
        return this.yieldResult(this.get(x) + y);
    }
    big_int_add(x, y) {
        return this.yieldResult(this.get(x) + this.get(y));
    }
    big_int_sub_i128(x, y_lo, y_hi) {
        const y = BigIntApi.join_i128(y_lo, y_hi);
        return this.yieldResult(this.get(x) - y);
    }
    big_int_sub(x, y) {
        return this.yieldResult(this.get(x) - this.get(y));
    }
    big_int_mul_i128(x, y_lo, y_hi) {
        const y = BigIntApi.join_i128(y_lo, y_hi);
        return this.yieldResult(this.get(x) * y);
    }
    big_int_mul_i128_i128(x_lo, x_hi, y_lo, y_hi) {
        const x = BigIntApi.join_i128(x_lo, x_hi);
        const y = BigIntApi.join_i128(y_lo, y_hi);
        return this.yieldResult(x * y);
    }
    big_int_mul(x, y) {
        return this.yieldResult(this.get(x) * this.get(y));
    }
    big_int_div_rem(rounding, x, y) {
        const xVal = this.get(x);
        const yVal = this.get(y);
        if (yVal === 0n) {
            this.environment.signalDivisionByZero();
            this.environment.setSmallQuotient(0n);
            this.environment.setSmallRemainder(0n);
            return 0n;
        }
        let quotient;
        let remainder;
        switch (rounding) {
            case DivRounding.to_zero:
                quotient = xVal / yVal;
                remainder = xVal % yVal;
                break;
            case DivRounding.to_pos_inf:
                quotient = xVal / yVal;
                remainder = xVal % yVal;
                if (remainder !== 0n && (xVal >= 0n) === (yVal >= 0n)) {
                    quotient += 1n;
                    remainder -= yVal;
                }
                break;
            case DivRounding.to_neg_inf:
                quotient = xVal / yVal;
                remainder = xVal % yVal;
                if (remainder !== 0n && (xVal >= 0n) !== (yVal >= 0n)) {
                    quotient -= 1n;
                    remainder += yVal;
                }
                break;
            default:
                return 0n;
        }
        let quotientId = 0;
        if (quotient >= -POW_2_127 && quotient < POW_2_127) {
            this.environment.setSmallQuotient(quotient);
        }
        else {
            quotientId = this.yieldBigResult(quotient);
        }
        let remainderId = 0;
        if (remainder >= -POW_2_127 && remainder < POW_2_127) {
            this.environment.setSmallRemainder(remainder);
        }
        else {
            remainderId = this.yieldBigResult(remainder);
        }
        return (BigInt(remainderId) << 32n) | BigInt(quotientId);
    }
    big_int_div(rounding, x, y) {
        const xVal = this.get(x);
        const yVal = this.get(y);
        if (yVal === 0n) {
            this.environment.signalDivisionByZero();
            this.environment.setSmallResult(0n);
            return 0;
        }
        let quotient;
        switch (rounding) {
            case DivRounding.to_zero:
                quotient = xVal / yVal;
                break;
            case DivRounding.to_pos_inf:
                quotient = xVal / yVal;
                const rem1 = xVal % yVal;
                if (rem1 !== 0n && (xVal >= 0n) === (yVal >= 0n)) {
                    quotient += 1n;
                }
                break;
            case DivRounding.to_neg_inf:
                quotient = xVal / yVal;
                const rem2 = xVal % yVal;
                if (rem2 !== 0n && (xVal >= 0n) !== (yVal >= 0n)) {
                    quotient -= 1n;
                }
                break;
            default:
                return 0;
        }
        return this.yieldResult(quotient);
    }
    big_int_rem(rounding, x, y) {
        const xVal = this.get(x);
        const yVal = this.get(y);
        if (yVal === 0n) {
            this.environment.signalDivisionByZero();
            this.environment.setSmallResult(0n);
            return 0;
        }
        let remainder;
        switch (rounding) {
            case DivRounding.to_zero: {
                remainder = xVal % yVal;
                break;
            }
            case DivRounding.to_pos_inf: {
                remainder = xVal % yVal;
                if (remainder !== 0n && (xVal >= 0n) === (yVal >= 0n)) {
                    remainder -= yVal;
                }
                break;
            }
            case DivRounding.to_neg_inf: {
                remainder = xVal % yVal;
                if (remainder !== 0n && (xVal >= 0n) !== (yVal >= 0n)) {
                    remainder += yVal;
                }
                break;
            }
            default: {
                throw new Error(`Invalid rounding mode: ${rounding}`);
            }
        }
        return this.yieldResult(remainder);
    }
    big_int_shl_i128_i32(x_lo, x_hi, s) {
        const x = BigIntApi.join_i128(x_lo, x_hi);
        const result = s >= 0 ? x << BigInt(s) : x >> BigInt(-s);
        return this.yieldResult(result);
    }
    big_int_shl_i32(x, s) {
        const xVal = this.get(x);
        const result = s >= 0 ? xVal << BigInt(s) : xVal >> BigInt(-s);
        return this.yieldResult(result);
    }
    big_int_shr_i32(x, s) {
        const xVal = this.get(x);
        const result = s >= 0 ? xVal >> BigInt(s) : xVal << BigInt(-s);
        return this.yieldResult(result);
    }
    big_int_pow_i128_i32(x_lo, x_hi, y) {
        const x = BigIntApi.join_i128(x_lo, x_hi);
        if (y < 0 || x === 0n) {
            return this.yieldResult(0n);
        }
        return this.yieldResult(x ** BigInt(y));
    }
    big_int_pow_i32(x, y) {
        const xVal = this.get(x);
        if (y < 0 || xVal === 0n) {
            return this.yieldResult(0n);
        }
        return this.yieldResult(xVal ** BigInt(y));
    }
    big_int_bit_and_i128(x, y_lo, y_hi) {
        const y = BigIntApi.join_i128(y_lo, y_hi);
        return this.yieldResult(this.get(x) & y);
    }
    big_int_bit_and(x, y) {
        return this.yieldResult(this.get(x) & this.get(y));
    }
    big_int_bit_or_i128(x, y_lo, y_hi) {
        const y = BigIntApi.join_i128(y_lo, y_hi);
        return this.yieldResult(this.get(x) | y);
    }
    big_int_bit_or(x, y) {
        return this.yieldResult(this.get(x) | this.get(y));
    }
    big_int_bit_xor_i128(x, y_lo, y_hi) {
        const y = BigIntApi.join_i128(y_lo, y_hi);
        return this.yieldResult(this.get(x) ^ y);
    }
    big_int_bit_xor(x, y) {
        return this.yieldResult(this.get(x) ^ this.get(y));
    }
    big_int_to_string(buffer, capacity, x, base, to_upper) {
        if (base < 2 || base > 36 || capacity === 0) {
            return 0;
        }
        const xVal = this.get(x);
        let result = xVal.toString(base);
        // Using the string length as a metric here is correct
        // because all output code units are ASCII characters.
        // When encoding to UTF-8, each code unit will occupy exactly one byte.
        if (result.length > capacity) {
            return 0;
        }
        // Also, case conversions for ASCII do not affect string length.
        if (to_upper && base > 10) {
            result = result.toUpperCase();
        }
        this.environment.writeString(buffer, capacity, result);
        return result.length;
    }
    big_int_from_string(buffer, length, base) {
        if (base < 2 || base > 36 || buffer === 0 || length === 0) {
            return FromStringStatus.invalid_argument;
        }
        const string = this.environment.readString(buffer, length);
        let value;
        try {
            value = parseBigInt(string, base);
        }
        catch (_) {
            return FromStringStatus.invalid_argument;
        }
        if (value < POW_2_127 && value >= -POW_2_127) {
            this.environment.setSmallResult(value);
            return FromStringStatus.small_result;
        }
        const id = this.generateId();
        this.repository.set(id, value);
        this.environment.setBigResult(id);
        return FromStringStatus.big_result;
    }
}
;
export function parseBigInt(string, radix) {
    if (radix === 10) {
        return BigInt(string);
    }
    // Unfortunately, parseInt only supports number;
    // there is no counterpart for bigint.
    // The BigInt constructor does not permit a manually specified radix.
    // For specific bases (2, 8, 10, 16), we can still use the constructor
    // (either directly or by using a base prefix).
    // For "exotic" bases, we need to manually convert.
    const negative = string.startsWith("-");
    const digits = negative ? string.slice(1) : string;
    const value = (() => {
        switch (radix) {
            case 2: return BigInt(`0b${digits}`);
            case 8: return BigInt(`0o${digits}`);
            case 16: return BigInt(`0x${digits}`);
        }
        const radixInt = BigInt(radix);
        let result = 0n;
        for (const char of digits) {
            const digit = parseInt(char, radix);
            result *= radixInt;
            result += BigInt(digit);
        }
        return result;
    })();
    return negative ? -value : value;
}
export var RegExpStatus;
(function (RegExpStatus) {
    RegExpStatus[RegExpStatus["unmatched"] = 0] = "unmatched";
    RegExpStatus[RegExpStatus["matched"] = 1] = "matched";
    RegExpStatus[RegExpStatus["invalid"] = 2] = "invalid";
    RegExpStatus[RegExpStatus["execution_error"] = 3] = "execution_error";
})(RegExpStatus || (RegExpStatus = {}));
export var RegExpFlags;
(function (RegExpFlags) {
    /// `i`.
    RegExpFlags[RegExpFlags["ignore_case"] = 1] = "ignore_case";
    /// `m`.
    RegExpFlags[RegExpFlags["multiline"] = 2] = "multiline";
    /// `s`.
    RegExpFlags[RegExpFlags["dot_all"] = 4] = "dot_all";
    /// `v`.
    RegExpFlags[RegExpFlags["unicode_sets"] = 8] = "unicode_sets";
})(RegExpFlags || (RegExpFlags = {}));
export const RegExpFlagsString = "imsv";
export function regExpFlagsToString(flags) {
    let result = "";
    for (let i = 0; i < RegExpFlagsString.length; i++) {
        if ((flags & (1 << i)) !== 0) {
            result += RegExpFlagsString[i];
        }
    }
    return result;
}
export class RegExpApi {
    repository = new Map();
    currentId = 0;
    environment;
    constructor(environment) {
        this.environment = environment;
    }
    generateId() {
        const maxId = 0xffff_ffff;
        if (this.currentId >= maxId) {
            throw new Error(`Maximum number of regex allocations reached (${maxId}).`);
        }
        return ++this.currentId;
    }
    compile(patternAddress, patternLength, flags) {
        // "g" should always be appended because otherwise,
        // we couldn't use the RegExp for replaceAll etc.
        // "v" cannot be combined with "u",
        // so we only add "v" when "u" is not set.
        // In any case, this guarantees that the regex operates on code points.
        const flagsString = regExpFlagsToString(flags) + "g"
            + ((flags & RegExpFlags.unicode_sets) != 0 ? "" : "v");
        try {
            const pattern = this.environment.readString(patternAddress, patternLength);
            const regexp = new RegExp(pattern, flagsString);
            const id = this.generateId();
            this.repository.set(id, regexp);
            return id;
        }
        catch (_) {
            return 0;
        }
    }
    delete(id) {
        return this.repository.delete(id);
    }
    match(id, stringAddress, stringLength) {
        const regex = this.repository.get(id);
        if (!regex) {
            return RegExpStatus.invalid;
        }
        regex.lastIndex = 0;
        const string = this.environment.readString(stringAddress, stringLength);
        let match;
        try {
            match = regex.exec(string);
        }
        catch (_) {
            return RegExpStatus.execution_error;
        }
        if (match && match.index === 0 && match[0].length === string.length) {
            return RegExpStatus.matched;
        }
        return RegExpStatus.unmatched;
    }
    search(resultAddress, id, stringAddress, stringLength) {
        const regex = this.repository.get(id);
        if (!regex) {
            return RegExpStatus.invalid;
        }
        regex.lastIndex = 0;
        const string = this.environment.readString(stringAddress, stringLength);
        let match;
        try {
            match = regex.exec(string);
        }
        catch (_) {
            return RegExpStatus.execution_error;
        }
        if (!match) {
            return RegExpStatus.unmatched;
        }
        // Convert UTF-16 indices from JavaScript to UTF-8 indices for WASM.
        // JavaScript strings use UTF-16,
        // but the WASM API expects UTF-8 code unit offsets.
        const beforeMatch = string.substring(0, match.index);
        const encoder = new TextEncoder();
        const utf8Index = encoder.encode(beforeMatch).length;
        const utf8Length = encoder.encode(match[0]).length;
        this.environment.writeSearchResult(resultAddress, utf8Index, utf8Length);
        return RegExpStatus.matched;
    }
    replaceAll(resultAddress, id, stringAddress, stringLength, replacementAddress, replacementLength) {
        const regex = this.repository.get(id);
        if (!regex || regex.global === false) {
            return RegExpStatus.invalid;
        }
        regex.lastIndex = 0;
        const string = this.environment.readString(stringAddress, stringLength);
        const replacement = this.environment.readString(replacementAddress, replacementLength);
        let replaced;
        try {
            if (!regex.test(string)) {
                return RegExpStatus.unmatched;
            }
            regex.lastIndex = 0;
            replaced = string.replaceAll(regex, replacement);
        }
        catch (_) {
            return RegExpStatus.execution_error;
        }
        const allocation = this.environment.allocUtf8(replaced);
        this.environment.writeStringView(resultAddress, allocation);
        return RegExpStatus.matched;
    }
}
export function createBigIntRegExpWasmEnvObject(bigInts, regExps, onMemoryGrowth) {
    return {
        emscripten_notify_memory_growth: onMemoryGrowth,
        big_int_i32: bigInts.big_int_i32.bind(bigInts),
        big_int_i64: bigInts.big_int_i64.bind(bigInts),
        big_int_i128: bigInts.big_int_i128.bind(bigInts),
        big_int_i192: bigInts.big_int_i192.bind(bigInts),
        big_int_pow2_i32: bigInts.big_int_pow2_i32.bind(bigInts),
        big_int_delete: bigInts.big_int_delete.bind(bigInts),
        big_int_compare_i32: bigInts.big_int_compare_i32.bind(bigInts),
        big_int_compare_i128: bigInts.big_int_compare_i128.bind(bigInts),
        big_int_compare: bigInts.big_int_compare.bind(bigInts),
        big_int_twos_width: bigInts.big_int_twos_width.bind(bigInts),
        big_int_ones_width: bigInts.big_int_ones_width.bind(bigInts),
        big_int_neg: bigInts.big_int_neg.bind(bigInts),
        big_int_bit_not: bigInts.big_int_bit_not.bind(bigInts),
        big_int_abs: bigInts.big_int_abs.bind(bigInts),
        big_int_trunc_i128: bigInts.big_int_trunc_i128.bind(bigInts),
        big_int_add_i32: bigInts.big_int_add_i32.bind(bigInts),
        big_int_add_i128: bigInts.big_int_add_i128.bind(bigInts),
        big_int_add: bigInts.big_int_add.bind(bigInts),
        big_int_sub_i128: bigInts.big_int_sub_i128.bind(bigInts),
        big_int_sub: bigInts.big_int_sub.bind(bigInts),
        big_int_mul_i128: bigInts.big_int_mul_i128.bind(bigInts),
        big_int_mul_i128_i128: bigInts.big_int_mul_i128_i128.bind(bigInts),
        big_int_mul: bigInts.big_int_mul.bind(bigInts),
        big_int_div_rem: bigInts.big_int_div_rem.bind(bigInts),
        big_int_div: bigInts.big_int_div.bind(bigInts),
        big_int_rem: bigInts.big_int_rem.bind(bigInts),
        big_int_shl_i128_i32: bigInts.big_int_shl_i128_i32.bind(bigInts),
        big_int_shl_i32: bigInts.big_int_shl_i32.bind(bigInts),
        big_int_shr_i32: bigInts.big_int_shr_i32.bind(bigInts),
        big_int_pow_i128_i32: bigInts.big_int_pow_i128_i32.bind(bigInts),
        big_int_pow_i32: bigInts.big_int_pow_i32.bind(bigInts),
        big_int_bit_and_i128: bigInts.big_int_bit_and_i128.bind(bigInts),
        big_int_bit_and: bigInts.big_int_bit_and.bind(bigInts),
        big_int_bit_or_i128: bigInts.big_int_bit_or_i128.bind(bigInts),
        big_int_bit_or: bigInts.big_int_bit_or.bind(bigInts),
        big_int_bit_xor_i128: bigInts.big_int_bit_xor_i128.bind(bigInts),
        big_int_bit_xor: bigInts.big_int_bit_xor.bind(bigInts),
        big_int_to_string: bigInts.big_int_to_string.bind(bigInts),
        big_int_from_string: bigInts.big_int_from_string.bind(bigInts),
        reg_exp_compile: regExps.compile.bind(regExps),
        reg_exp_delete: regExps.delete.bind(regExps),
        reg_exp_match: regExps.match.bind(regExps),
        reg_exp_search: regExps.search.bind(regExps),
        reg_exp_replace_all: regExps.replaceAll.bind(regExps),
    };
}
/**
 * Wrapper for the WASM module that provides COWEL functionality.
 */
export class CowelWasm {
    module;
    instance;
    heap_u8;
    heap_u16;
    heap_u32;
    heap_i32;
    heap_i64;
    bigInts;
    regExps;
    preservedVariableNames = [];
    capturedVariables = [];
    loadFile;
    log;
    constructor() {
        const bigIntEnv = {
            setSmallResult: (x) => {
                const address = this.exports.cowel_big_int_small_result;
                this.heap_i64[address / 8 + 0] = x & MASK_64;
                this.heap_i64[address / 8 + 1] = (x >> 64n) & MASK_64;
            },
            setBigResult: (x) => {
                const address = this.exports.cowel_big_int_big_result;
                this.heap_u32[address / 4 + 0] = Number(x);
            },
            setSmallQuotient: (x) => {
                const address = this.exports.cowel_big_int_div_result + 0;
                this.heap_i64[address / 8 + 0] = x & MASK_64;
                this.heap_i64[address / 8 + 1] = (x >> 64n) & MASK_64;
            },
            setSmallRemainder: (x) => {
                const address = this.exports.cowel_big_int_div_result + 16;
                this.heap_i64[address / 8 + 0] = x & MASK_64;
                this.heap_i64[address / 8 + 1] = (x >> 64n) & MASK_64;
            },
            signalDivisionByZero: () => {
                const address = this.exports.cowel_big_int_div_result + 32;
                this.heap_u8[address] = 1;
            },
            readString: (address, length) => {
                return this.decodeUtf8(address, length);
            },
            writeString: (address, length, str) => {
                const data = new TextEncoder().encode(str);
                if (data.length > length) {
                    throw new Error(`String too long to write: ${data.length} > ${length}`);
                }
                this.heap_u8.set(data, address);
            },
        };
        this.bigInts = new BigIntApi(bigIntEnv);
        const regExpEnv = {
            readString: (address, length) => {
                return this.decodeUtf8(address, length);
            },
            writeSearchResult: (address, index, length) => {
                this.heap_u32[address / 4 + 0] = index;
                this.heap_u32[address / 4 + 1] = length;
            },
            allocUtf8: (str) => {
                return this.allocUtf8(str);
            },
            writeStringView: (address, allocation) => {
                this.heap_u32[address / 4 + 0] = allocation.address;
                this.heap_u32[address / 4 + 1] = allocation.size;
            },
        };
        this.regExps = new RegExpApi(regExpEnv);
    }
    async init(module) {
        this.module = await WebAssembly.compile(module);
        const imports = {
            env: {
                load_file: (resultAddress, pathAddress, pathLength, baseFileId) => {
                    const path = this.decodeUtf8(pathAddress, pathLength);
                    this.encodeFileResult(resultAddress, this.loadFile(path, baseFileId));
                },
                log: (diagnosticAddress) => {
                    const diagnostic = this.decodeDiagnostic(diagnosticAddress);
                    this.log(diagnostic);
                },
                consume_variables: (variablesAddress, length) => {
                    this.capturedVariables = [];
                    for (let i = 0; i < length; ++i) {
                        const textAddress = this.heap_u32[variablesAddress / 4 + i * 2 + 0];
                        const textLength = this.heap_u32[variablesAddress / 4 + i * 2 + 1];
                        this.capturedVariables.push(this.decodeUtf8(textAddress, textLength));
                    }
                },
                ...createBigIntRegExpWasmEnvObject(this.bigInts, this.regExps, (byteLength) => {
                    this.onMemoryGrowth(byteLength);
                }),
            },
        };
        this.instance = await WebAssembly.instantiate(this.module, imports);
        this.onMemoryGrowth(this.memory.buffer.byteLength);
        if (this.exports._initialize) {
            this.exports._initialize();
        }
        else {
            console.warn("module has no export _initialize");
        }
        this.exports.register_assertion_handler();
    }
    async generateHtml(options) {
        this.loadFile = options.loadFile;
        this.log = options.log;
        const allocations = this.makeOptions(options);
        const genResult = this.alloc2(20, 4);
        this.exports.cowel_generate_html_u8(genResult.address, allocations.options.address);
        const result = this.decodeGenResult(genResult.address);
        this.free(genResult);
        this.free(allocations.source);
        this.free(allocations.options);
        // Free preserved variables allocations.
        for (const allocation of allocations.preservedVariablesStrings) {
            this.free(allocation);
        }
        if (allocations.preservedVariablesArray !== null) {
            this.free(allocations.preservedVariablesArray);
        }
        return result;
    }
    generateCodeCitationFor(options) {
        if (options.length === 0) {
            throw new Error("Cannot generate code citation " +
                "where the target location has zero length.");
        }
        const resultAllocation = this.alloc2(8, 4);
        const stringAllocation = this.allocBytes2(options.source);
        this.exports.generate_code_citation(resultAllocation.address, stringAllocation.address, stringAllocation.size, options.line, options.column, options.begin, options.length, options.colors);
        const resultAddress = this.heap_u32[resultAllocation.address / 4 + 0];
        const resultLength = this.heap_u32[resultAllocation.address / 4 + 1];
        const result = this.decodeUtf8(resultAddress, resultLength);
        this.free(stringAllocation);
        this.free(resultAllocation);
        return result;
    }
    /**
     * Parses command-line arguments by calling into the WASM module.
     * The result struct layout (WASM32) is:
     *   offset  0: command        (uint32)
     *   offset  4: input.text     (ptr)
     *   offset  8: input.length   (uint32)
     *   offset 12: output.text    (ptr)
     *   offset 16: output.length  (uint32)
     *   offset 20: min_severity   (uint32)
     *   offset 24: no_color       (uint8)
     *   offset 25: ok             (uint8)
     *   offset 26: padding        (2 bytes)
     *   offset 28: error.text     (ptr)
     *   offset 32: error.length   (uint32)
     *   total: 36 bytes, align 4
     */
    parseCliOptions(args) {
        const encoder = new TextEncoder();
        // Allocate each arg as a null-terminated UTF-8 string in WASM memory.
        const argAllocs = args.map(arg => {
            const encoded = encoder.encode(arg);
            const alloc = this.alloc2(encoded.length + 1, 1);
            this.heap_u8.set(encoded, alloc.address);
            this.heap_u8[alloc.address + encoded.length] = 0;
            return alloc;
        });
        // Build a pointer array; use address 0 when there are no args
        // `cowel_parse_cli_options` short-circuits on `arg_count` == 0.
        let argsArray = null;
        let argsArrayAddr = 0;
        if (args.length > 0) {
            argsArray = this.alloc2(args.length * 4, 4);
            argsArrayAddr = argsArray.address;
            for (let i = 0; i < argAllocs.length; i++) {
                this.heap_u32[argsArray.address / 4 + i] = argAllocs[i].address;
            }
        }
        // Allocate the result struct.
        const resultAlloc = this.alloc2(36, 4);
        this.exports.cowel_parse_cli_options_u8(resultAlloc.address, argsArrayAddr, args.length);
        // Read fields from the result struct.
        const command = this.heap_u32[resultAlloc.address / 4 + 0];
        const inputTextAddr = this.heap_u32[resultAlloc.address / 4 + 1];
        const inputLength = this.heap_u32[resultAlloc.address / 4 + 2];
        const outputTextAddr = this.heap_u32[resultAlloc.address / 4 + 3];
        const outputLength = this.heap_u32[resultAlloc.address / 4 + 4];
        const minSeverity = this.heap_u32[resultAlloc.address / 4 + 5];
        const noColor = this.heap_u8[resultAlloc.address + 24] !== 0;
        const ok = this.heap_u8[resultAlloc.address + 25] !== 0;
        const errorTextAddr = this.heap_u32[resultAlloc.address / 4 + 7];
        const errorLength = this.heap_u32[resultAlloc.address / 4 + 8];
        // Decode heap-allocated strings before freeing.
        const input = inputTextAddr !== 0 ? this.decodeUtf8(inputTextAddr, inputLength) : "";
        const output = outputTextAddr !== 0 ? this.decodeUtf8(outputTextAddr, outputLength) : "";
        const errorMessage = errorTextAddr !== 0 ? this.decodeUtf8(errorTextAddr, errorLength) : "";
        this.exports.cowel_free_cli_options_u8(resultAlloc.address);
        this.free(resultAlloc);
        if (argsArray !== null) {
            this.free(argsArray);
        }
        for (const alloc of argAllocs) {
            this.free(alloc);
        }
        const commandStr = (command === 0 ? "none" :
            command === 1 ? "help" :
                command === 2 ? "version" :
                    "run");
        return { command: commandStr, ok, input, output, minSeverity, noColor, errorMessage };
    }
    onMemoryGrowth(_byteLength) {
        this.heap_u8 = new Uint8Array(this.memory.buffer);
        this.heap_u16 = new Uint16Array(this.memory.buffer);
        this.heap_u32 = new Uint32Array(this.memory.buffer);
        this.heap_i32 = new Int32Array(this.memory.buffer);
        this.heap_i64 = new BigInt64Array(this.memory.buffer);
    }
    /**
     * Loads a `string` from UTF-8 encoded bytes
     * residing in memory at `address`.
     */
    decodeUtf8(address, length) {
        const utf8Bytes = this.heap_u8.subarray(address, address + length);
        const decoder = new TextDecoder("utf-8", { fatal: true });
        return decoder.decode(utf8Bytes);
    }
    /**
     * Decodes a `cowel_diagnostic_u8` located at `address`.
     */
    decodeDiagnostic(address) {
        const severityNumber = this.heap_i32[address / 4 + 0];
        const idAddress = this.heap_u32[address / 4 + 1];
        const idLength = this.heap_u32[address / 4 + 2];
        const messageAddress = this.heap_u32[address / 4 + 3];
        const messageLength = this.heap_u32[address / 4 + 4];
        const stackAddress = this.heap_u32[address / 4 + 5];
        const stackSize = this.heap_u32[address / 4 + 6];
        const id = this.decodeUtf8(idAddress, idLength);
        const message = this.decodeUtf8(messageAddress, messageLength);
        const severity = severityNumber;
        const stack = [];
        // cowel_diagnostic_location_u8 =
        // cowel_string_view + file_id + begin + length + line + column.
        // In wasm32 this is 7 i32/u32 words per entry.
        const wordsPerStackLocation = 7;
        for (let i = 0; i < stackSize; ++i) {
            const locationAddress = stackAddress / 4 + i * wordsPerStackLocation;
            const stackFileNameAddress = this.heap_u32[locationAddress + 0];
            const stackFileNameLength = this.heap_u32[locationAddress + 1];
            const stackFileId = this.heap_i32[locationAddress + 2];
            const stackBegin = this.heap_u32[locationAddress + 3];
            const stackLength = this.heap_u32[locationAddress + 4];
            const stackLine = this.heap_u32[locationAddress + 5];
            const stackColumn = this.heap_u32[locationAddress + 6];
            stack.push({
                fileName: this.decodeUtf8(stackFileNameAddress, stackFileNameLength),
                fileId: stackFileId,
                begin: stackBegin,
                length: stackLength,
                line: stackLine,
                column: stackColumn,
            });
        }
        return {
            severity,
            id,
            message,
            stack,
        };
    }
    /**
     * Decodes a `cowel_gen_result_u8` located at `address`.
     */
    decodeGenResult(address) {
        const status = this.heap_i32[address / 4 + 0];
        const outputAddress = this.heap_u32[address / 4 + 1];
        const outputSize = this.heap_u32[address / 4 + 2];
        const output = this.decodeUtf8(outputAddress, outputSize);
        this.free(outputAddress, outputSize, 1);
        const variables = {};
        for (let i = 0; i < this.preservedVariableNames.length; ++i) {
            const name = this.preservedVariableNames[i];
            const value = i < this.capturedVariables.length ? this.capturedVariables[i] : "";
            variables[name] = value;
        }
        return { status, output, variables };
    }
    /**
     * Encodes a `cowel_file_result_u8` located at `address`.
     */
    encodeFileResult(address, result) {
        const dataAddress = result.data !== undefined ? this.allocBytes(result.data) : 0;
        const dataLength = result.data !== undefined ? result.data.byteLength : 0;
        this.heap_u32[address / 4 + 0] = result.status;
        this.heap_u32[address / 4 + 1] = dataAddress;
        this.heap_u32[address / 4 + 2] = dataLength;
        this.heap_u32[address / 4 + 3] = result.id;
    }
    /**
     * Allocates a `cowel_options_u8` object and a UTF-8 text buffer
     * which that object refers to as the `source` data member.
     * @param wasm The instance of the WASM helper module.
     * @param genOptions Options for generation.
     * @returns The allocated `cowel_options_u8` and source text.
     */
    makeOptions(genOptions) {
        const options = this.alloc2(92, 4);
        const source = this.allocUtf8(genOptions.source);
        const preservedVariables = genOptions.preservedVariables ?? [];
        this.preservedVariableNames = preservedVariables;
        this.capturedVariables = [];
        let preservedVariablesAddress = 0;
        const preservedVariablesStrings = [];
        let preservedVariablesArray = null;
        if (preservedVariables.length > 0) {
            // The type of the array is cowel_string_view_u8[],
            // where each entry is 8 bytes large.
            const arrayByteSize = preservedVariables.length * 8;
            preservedVariablesArray = this.alloc2(arrayByteSize, 4);
            preservedVariablesAddress = preservedVariablesArray.address;
            for (let i = 0; i < preservedVariables.length; ++i) {
                const strAlloc = this.allocUtf8(preservedVariables[i]);
                preservedVariablesStrings.push(strAlloc);
                this.heap_u32[preservedVariablesAddress / 4 + i * 2 + 0] = strAlloc.address;
                this.heap_u32[preservedVariablesAddress / 4 + i * 2 + 1] = strAlloc.size;
            }
        }
        this.exports.init_options(options.address, source.address, source.size, genOptions.mode, genOptions.minSeverity, preservedVariablesAddress, preservedVariables.length, genOptions.highlightPolicy, genOptions.enableXHighlighting);
        return {
            options,
            source,
            preservedVariablesArray,
            preservedVariablesStrings,
        };
    }
    /**
     * Copies a `string` to a newly allocated UTF-8 array in memory.
     * The returned allocation should later be freed with `_freeUtf8`.
     * @see free2
     */
    allocUtf8(str) {
        const data = new TextEncoder().encode(str);
        const address = this.allocBytes(data);
        return { address, size: data.length, alignment: 1 };
    }
    /**
     * Allocates space for `bytes.length` bytes with `align` alignment,
     * and copies the given `bytes` to the allocated memory.
     */
    allocBytes(bytes, alignment = 1) {
        const byteArray = bytes instanceof Uint8Array ? bytes : new Uint8Array(bytes);
        const address = this.alloc(byteArray.length, alignment);
        this.heap_u8.set(byteArray, address);
        return address;
    }
    allocBytes2(bytes, alignment = 1) {
        return {
            address: this.allocBytes(bytes),
            size: bytes.byteLength,
            alignment,
        };
    }
    alloc(size, alignment) {
        const result = this.exports.cowel_alloc(size, alignment);
        if (result === 0) {
            throw new Error(`Allocation failure in WASM with: size=${size}, align=${alignment}`);
        }
        return result;
    }
    alloc2(size, alignment) {
        return { address: this.alloc(size, alignment), size, alignment };
    }
    free(arg1, size, alignment) {
        if (typeof arg1 === "object") {
            this.exports.cowel_free(arg1.address, arg1.size, arg1.alignment);
        }
        else {
            this.exports.cowel_free(arg1, size, alignment);
        }
    }
    get memory() {
        return this.exports.memory;
    }
    get exports() {
        return this.instance.exports;
    }
}
export async function load(module) {
    const result = new CowelWasm();
    await result.init(module);
    return result;
}
