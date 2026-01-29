export enum Mode {
    document,
    minimal,
}

export enum ProcessingStatus {
    ok,
    break,
    error,
    error_break,
    fatal,
}

export enum IoStatus {
    ok,
    error,
    not_found,
    read_error,
    permissions_error,
}

export enum Severity {
    min = 0,
    trace = 10,
    debug = 20,
    info = 30,
    soft_warning = 40,
    warning = 50,
    error = 70,
    fatal = 90,
    none = 100,
}

export type GenOptions = {
    source: string;
    mode: Mode;
    minSeverity: Severity;
    loadFile(path: string, baseFileId: number): FileResult;
    log(diagnostic: Diagnostic): void;
};

export type GenResult = {
    status: ProcessingStatus;
    output: string;
};

export type FileResult = {
    status: IoStatus,
    data?: Buffer,
    id: number
};

export type Diagnostic = {
    /** The severity of the diagnostic. */
    severity: Severity;
    /**
     * An ID which identifies the diagnostic.
     * This is usually a dot-separated sequence of identifiers.
     */
    id: string;
    /** A diagnostic message. */
    message: string;
    /**
     * A possibly empty string.
     * If provided, instead of the `fileId`,
     * this names the file in which the error has occurred.
     */
    fileName: string;
    /** The ID of the file, or `-1`. */
    fileId: number;
    /** Index of the code unit where the cited code span begins. */
    begin: number;
    /** Length of the cited code span, in code units. */
    length: number;
    /** Zero-based line number of the start of the code span. */
    line: number;
    /**
     * The offset from the start of the line of the code span, in code units.
     */
    column: number;
};

export type CodeCiteOptions = {
    source: Buffer;
    line: number;
    column: number;
    begin: number;
    length: number;
    colors: boolean;
};

type Address = number;

type Allocation = {
    address: Address,
    size: number,
    alignment: number,
};

interface CowelWasmEnvObject {
    load_file: (
        resultAddress: Address,
        pathAddress: Address,
        pathLength: number,
        baseFileId: number,
    ) => void;
    log: (diagnosticAddress: Address) => void;
    emscripten_notify_memory_growth: (byteLength: number) => void;

    big_int_i32: (x: number) => BigIntId;
    big_int_i64: (x: bigint) => BigIntId;
    big_int_i128: (x_lo: bigint, y_lo: bigint) => BigIntId;
    big_int_i192: (btm: bigint, mid: bigint, top: bigint) => BigIntId;
    big_int_pow2_i32: (exponent: number) => BigIntId;
    big_int_delete: (id: BigIntId) => boolean;
    big_int_compare_i32: (x: BigIntId, y: number) => -1 | 0 | 1;
    big_int_compare_i128: (x: BigIntId, y_lo: bigint, y_hi: bigint) => -1 | 0 | 1;
    big_int_compare: (x: BigIntId, y: BigIntId) => -1 | 0 | 1;
    big_int_twos_width: (x: BigIntId) => number;
    big_int_ones_width: (x: BigIntId) => number;
    big_int_neg: (x: BigIntId) => BigIntId;
    big_int_bit_not: (x: BigIntId) => BigIntId;
    big_int_abs: (x: BigIntId) => BigIntId;
    big_int_trunc_i128: (x: BigIntId) => boolean;
    big_int_add_i32: (x: BigIntId, y: number) => BigIntId;
    big_int_add_i128: (x: BigIntId, y_lo: bigint, y_hi: bigint) => BigIntId;
    big_int_add: (x: BigIntId, y: BigIntId) => BigIntId;
    big_int_sub_i128: (x: BigIntId, y_lo: bigint, y_hi: bigint) => BigIntId;
    big_int_sub: (x: BigIntId, y: BigIntId) => BigIntId;
    big_int_mul_i128: (x: BigIntId, y_lo: bigint, y_hi: bigint) => BigIntId;
    big_int_mul_i128_i128: (x_lo: bigint, x_hi: bigint, y_lo: bigint, y_hi: bigint) => BigIntId;
    big_int_mul: (x: BigIntId, y: BigIntId) => BigIntId;
    big_int_div_rem: (rounding: DivRounding, x: BigIntId, y: BigIntId) => bigint;
    big_int_div: (rounding: DivRounding, x: BigIntId, y: BigIntId) => BigIntId;
    big_int_rem: (rounding: DivRounding, x: BigIntId, y: BigIntId) => BigIntId;
    big_int_shl_i128_i32: (x_lo: bigint, x_hi: bigint, s: number) => BigIntId;
    big_int_shl_i32: (x: BigIntId, s: number) => BigIntId;
    big_int_shr_i32: (x: BigIntId, s: number) => BigIntId;
    big_int_pow_i128_i32: (x_lo: bigint, x_hi: bigint, y: number) => BigIntId;
    big_int_pow_i32: (x: BigIntId, y: number) => BigIntId;
    big_int_bit_and_i128: (x: BigIntId, y_lo: bigint, y_hi: bigint) => BigIntId;
    big_int_bit_and: (x: BigIntId, y: BigIntId) => BigIntId;
    big_int_bit_or_i128: (x: BigIntId, y_lo: bigint, y_hi: bigint) => BigIntId;
    big_int_bit_or: (x: BigIntId, y: BigIntId) => BigIntId;
    big_int_bit_xor_i128: (x: BigIntId, y_lo: bigint, y_hi: bigint) => BigIntId;
    big_int_bit_xor: (x: BigIntId, y: BigIntId) => BigIntId;
    big_int_to_string:
    (buffer: Address, capacity: number, x: BigIntId, base: number, to_upper: boolean) => number;
    big_int_from_string: (buffer: Address, length: number, base: number) => FromStringStatus;
}

type CowelWasmImports = WebAssembly.Imports & {
    readonly env: CowelWasmEnvObject;
};

type CowelWasmExports = WebAssembly.Exports & {
    readonly memory: WebAssembly.Memory;
    readonly __indirect_function_table: WebAssembly.Table;
    _initialize(): void;
    cowel_alloc(size: number, alignment: number): Address;
    cowel_free(address: Address, size: number, alignment: number): void;
    cowel_generate_html_u8(result: Address, options: Address): void;

    init_options(
        resultAddress: Address,
        sourceAddress: Address,
        sourceLength: number,
        mode: number,
        min_log_severity: number
    ): void;
    register_assertion_handler(): void;
    generate_code_citation(
        resultAddress: Address,
        sourceAddress: Address,
        sourceLength: number,
        line: number,
        column: number,
        begin: number,
        length: number,
        colors: boolean
    ): void;

    readonly cowel_big_int_small_result: Address;
    readonly cowel_big_int_big_result: Address;
    readonly cowel_big_int_div_result: Address;
};

type BigIntId = number;

const MASK_64 = (1n << 64n) - 1n;
const POW_2_127 = 1n << 127n;

enum DivRounding {
    to_zero = 0,
    to_pos_inf = 1,
    to_neg_inf = 2,
}

enum FromStringStatus {
    small_result,
    big_result,
    invalid_argument,
    result_out_of_range,
}

interface BigIntEnvironment {
    setSmallResult(x: bigint): void;
    setBigResult(x: BigIntId): void;

    setSmallQuotient(x: bigint): void;
    setSmallRemainder(x: bigint): void;
    signalDivisionByZero(): void;

    readString(address: Address, length: number): string;
    writeString(address: Address, length: number, str: string): void;
}

class BigIntApi {
    private readonly repository: Map<BigIntId, bigint> = new Map();
    private currentId = 0;
    private readonly environment;

    constructor(environment: BigIntEnvironment) {
        this.environment = environment;
    }

    private generateId(): number {
        const maxId = (1 << 32) - 1;
        if (this.currentId >= maxId) {
            throw new Error(`Maximum number of integer allocations reached (${maxId}).`);
        }
        return ++this.currentId;
    }

    private static join_i128(lo: bigint, hi: bigint): bigint {
        return (hi << 64n) | (lo & MASK_64);
    }

    private static join_i192(btm: bigint, mid: bigint, top: bigint): bigint {
        // No mask is applied to top since we want the result
        // to have the same sign.
        return (top << 128n)
            | ((mid & MASK_64) << 64n)
            | (btm & MASK_64);
    }

    private static doCompare(x: bigint, y: bigint): -1 | 0 | 1 {
        if (x < y) return -1;
        if (x > y) return 1;
        return 0;
    }

    private get(id: BigIntId): bigint {
        const value = this.repository.get(id);
        if (value === undefined) {
            throw new Error(`BigInt with id ${id} not found.`);
        }
        return value;
    }

    private yieldBigResult(x: bigint): BigIntId {
        const id = this.generateId();
        this.repository.set(id, BigInt(x));
        return id;
    }

    private yieldResult(x: bigint): BigIntId {
        if (x < POW_2_127 && x >= -POW_2_127) {
            this.environment.setSmallResult(x);
            return 0;
        }
        return this.yieldBigResult(x);
    }

    big_int_i32(value: number): BigIntId {
        return this.yieldBigResult(BigInt(value));
    }

    big_int_i64(value: bigint): BigIntId {
        return this.yieldBigResult(value);
    }

    big_int_i128(lo: bigint, hi: bigint): BigIntId {
        return this.yieldBigResult(BigIntApi.join_i128(lo, hi));
    }

    big_int_i192(btm: bigint, mid: bigint, top: bigint): BigIntId {
        return this.yieldBigResult(BigIntApi.join_i192(btm, mid, top));
    }

    big_int_pow2_i32(exponent: number): BigIntId {
        const result = exponent < 0 ? 0n : 1n << BigInt(exponent);
        return this.yieldResult(result);
    }

    big_int_delete(id: BigIntId): boolean {
        return this.repository.delete(id);
    }

    big_int_compare_i32(x: BigIntId, y: number): -1 | 0 | 1 {
        return BigIntApi.doCompare(this.get(x), BigInt(y));
    }

    big_int_compare_i128(x: BigIntId, y_lo: bigint, y_hi: bigint): -1 | 0 | 1 {
        return BigIntApi.doCompare(this.get(x), BigIntApi.join_i128(y_lo, y_hi));
    }

    big_int_compare(x: BigIntId, y: BigIntId): -1 | 0 | 1 {
        return BigIntApi.doCompare(this.get(x), this.get(y));
    }

    big_int_twos_width(x: BigIntId): number {
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

    big_int_ones_width(x: BigIntId): number {
        const value = this.get(x);
        if (value === 0n) {
            return 1;
        }
        const string = value.toString(2);
        return value >= 0n ? string.length + 1 : string.length;
    }

    big_int_neg(x: BigIntId): BigIntId {
        return this.yieldResult(-this.get(x));
    }

    big_int_bit_not(x: BigIntId): BigIntId {
        return this.yieldResult(~this.get(x));
    }

    big_int_abs(x: BigIntId): BigIntId {
        const value = this.get(x);
        return this.yieldResult(value < 0n ? -value : value);
    }

    big_int_trunc_i128(x: BigIntId): boolean {
        const value = this.get(x);
        const MASK_128 = (1n << 128n) - 1n;
        const truncated = value & MASK_128;
        this.environment.setSmallResult(truncated);
        return truncated !== value;
    }

    big_int_add_i32(x: BigIntId, y: number): BigIntId {
        return this.yieldResult(this.get(x) + BigInt(y));
    }

    big_int_add_i128(x: BigIntId, y_lo: bigint, y_hi: bigint): BigIntId {
        const y = BigIntApi.join_i128(y_lo, y_hi);
        return this.yieldResult(this.get(x) + y);
    }

    big_int_add(x: BigIntId, y: BigIntId): BigIntId {
        return this.yieldResult(this.get(x) + this.get(y));
    }

    big_int_sub_i128(x: BigIntId, y_lo: bigint, y_hi: bigint): BigIntId {
        const y = BigIntApi.join_i128(y_lo, y_hi);
        return this.yieldResult(this.get(x) - y);
    }

    big_int_sub(x: BigIntId, y: BigIntId): BigIntId {
        return this.yieldResult(this.get(x) - this.get(y));
    }

    big_int_mul_i128(x: BigIntId, y_lo: bigint, y_hi: bigint): BigIntId {
        const y = BigIntApi.join_i128(y_lo, y_hi);
        return this.yieldResult(this.get(x) * y);
    }

    big_int_mul_i128_i128(x_lo: bigint, x_hi: bigint, y_lo: bigint, y_hi: bigint): BigIntId {
        const x = BigIntApi.join_i128(x_lo, x_hi);
        const y = BigIntApi.join_i128(y_lo, y_hi);
        return this.yieldResult(x * y);
    }

    big_int_mul(x: BigIntId, y: BigIntId): BigIntId {
        return this.yieldResult(this.get(x) * this.get(y));
    }

    big_int_div_rem(rounding: DivRounding, x: BigIntId, y: BigIntId): bigint {
        const xVal = this.get(x);
        const yVal = this.get(y);
        if (yVal === 0n) {
            this.environment.signalDivisionByZero();
            this.environment.setSmallQuotient(0n);
            this.environment.setSmallRemainder(0n);
            return 0n;
        }

        let quotient: bigint;
        let remainder: bigint;

        switch (rounding) {
            case DivRounding.to_zero:
                quotient = xVal / yVal;
                remainder = xVal % yVal;
                break;
            case DivRounding.to_pos_inf:
                quotient = xVal / yVal;
                remainder = xVal % yVal;
                if (remainder !== 0n && (remainder > 0n) !== (yVal > 0n)) {
                    quotient += 1n;
                    remainder -= yVal;
                }
                break;
            case DivRounding.to_neg_inf:
                quotient = xVal / yVal;
                remainder = xVal % yVal;
                if (remainder !== 0n && (remainder > 0n) === (yVal > 0n)) {
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
        } else {
            quotientId = this.yieldBigResult(quotient);
        }

        let remainderId = 0;
        if (remainder >= -POW_2_127 && remainder < POW_2_127) {
            this.environment.setSmallRemainder(remainder);
        } else {
            remainderId = this.yieldBigResult(remainder);
        }

        return (BigInt(remainderId) << 32n) | BigInt(quotientId);
    }

    big_int_div(rounding: DivRounding, x: BigIntId, y: BigIntId): BigIntId {
        const xVal = this.get(x);
        const yVal = this.get(y);
        if (yVal === 0n) {
            this.environment.signalDivisionByZero();
            this.environment.setSmallResult(0n);
            return 0;
        }

        let quotient: bigint;

        switch (rounding) {
            case DivRounding.to_zero:
                quotient = xVal / yVal;
                break;
            case DivRounding.to_pos_inf:
                quotient = xVal / yVal;
                const rem1 = xVal % yVal;
                if (rem1 !== 0n && (rem1 > 0n) !== (yVal > 0n)) {
                    quotient += 1n;
                }
                break;
            case DivRounding.to_neg_inf:
                quotient = xVal / yVal;
                const rem2 = xVal % yVal;
                if (rem2 !== 0n && (rem2 > 0n) === (yVal > 0n)) {
                    quotient -= 1n;
                }
                break;
            default:
                return 0;
        }

        return this.yieldResult(quotient);
    }

    big_int_rem(rounding: DivRounding, x: BigIntId, y: BigIntId): BigIntId {
        const xVal = this.get(x);
        const yVal = this.get(y);
        if (yVal === 0n) {
            this.environment.signalDivisionByZero();
            this.environment.setSmallResult(0n);
            return 0;
        }

        let remainder: bigint;
        switch (rounding) {
            case DivRounding.to_zero: {
                remainder = xVal % yVal;
                break;
            }
            case DivRounding.to_pos_inf: {
                remainder = xVal % yVal;
                if (remainder !== 0n && (remainder > 0n) !== (yVal > 0n)) {
                    remainder -= yVal;
                }
                break;
            }
            case DivRounding.to_neg_inf: {
                remainder = xVal % yVal;
                if (remainder !== 0n && (remainder > 0n) === (yVal > 0n)) {
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

    big_int_shl_i128_i32(x_lo: bigint, x_hi: bigint, s: number): BigIntId {
        const x = BigIntApi.join_i128(x_lo, x_hi);
        const result = s >= 0 ? x << BigInt(s) : x >> BigInt(-s);
        return this.yieldResult(result);
    }

    big_int_shl_i32(x: BigIntId, s: number): BigIntId {
        const xVal = this.get(x);
        const result = s >= 0 ? xVal << BigInt(s) : xVal >> BigInt(-s);
        return this.yieldResult(result);
    }

    big_int_shr_i32(x: BigIntId, s: number): BigIntId {
        const xVal = this.get(x);
        const result = s >= 0 ? xVal >> BigInt(s) : xVal << BigInt(-s);
        return this.yieldResult(result);
    }

    big_int_pow_i128_i32(x_lo: bigint, x_hi: bigint, y: number): BigIntId {
        const x = BigIntApi.join_i128(x_lo, x_hi);
        if (y < 0 || x === 0n) {
            return this.yieldResult(0n);
        }
        return this.yieldResult(x ** BigInt(y));
    }

    big_int_pow_i32(x: BigIntId, y: number): BigIntId {
        const xVal = this.get(x);
        if (y < 0 || xVal === 0n) {
            return this.yieldResult(0n);
        }
        return this.yieldResult(xVal ** BigInt(y));
    }

    big_int_bit_and_i128(x: BigIntId, y_lo: bigint, y_hi: bigint): BigIntId {
        const y = BigIntApi.join_i128(y_lo, y_hi);
        return this.yieldResult(this.get(x) & y);
    }

    big_int_bit_and(x: BigIntId, y: BigIntId): BigIntId {
        return this.yieldResult(this.get(x) & this.get(y));
    }

    big_int_bit_or_i128(x: BigIntId, y_lo: bigint, y_hi: bigint): BigIntId {
        const y = BigIntApi.join_i128(y_lo, y_hi);
        return this.yieldResult(this.get(x) | y);
    }

    big_int_bit_or(x: BigIntId, y: BigIntId): BigIntId {
        return this.yieldResult(this.get(x) | this.get(y));
    }

    big_int_bit_xor_i128(x: BigIntId, y_lo: bigint, y_hi: bigint): BigIntId {
        const y = BigIntApi.join_i128(y_lo, y_hi);
        return this.yieldResult(this.get(x) ^ y);
    }

    big_int_bit_xor(x: BigIntId, y: BigIntId): BigIntId {
        return this.yieldResult(this.get(x) ^ this.get(y));
    }

    big_int_to_string(
        buffer: Address,
        capacity: number,
        x: BigIntId,
        base: number,
        to_upper: boolean,
    ): number {
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

    big_int_from_string(buffer: Address, length: number, base: number): FromStringStatus {
        if (base < 2 || base > 36 || buffer === 0 || length === 0) {
            return FromStringStatus.invalid_argument;
        }
        const string = this.environment.readString(buffer, length);
        const value = BigInt(parseInt(string, base));
        if (value < POW_2_127 && value >= -POW_2_127) {
            this.environment.setSmallResult(value);
            return FromStringStatus.small_result;
        }
        const id = this.generateId();
        this.repository.set(id, value);
        this.environment.setBigResult(id);
        return FromStringStatus.big_result;
    }
};

/**
 * Wrapper for the WASM module that provides COWEL functionality.
 */
export class CowelWasm {
    private module!: WebAssembly.Module;
    private instance!: WebAssembly.Instance;
    private heap_u8!: Uint8Array;
    private heap_u16!: Uint16Array;
    private heap_u32!: Uint32Array;
    private heap_i32!: Int32Array;
    private heap_i64!: BigInt64Array;
    private bigInts: BigIntApi;

    private loadFile!: (path: string, baseFileId: number) => FileResult;
    private log!: (diagnostic: Diagnostic) => void;

    constructor() {
        const bigIntEnv: BigIntEnvironment = {
            setSmallResult: (x: bigint) => {
                const address = this.exports.cowel_big_int_small_result;
                this.heap_i64[address / 8 + 0] = x & MASK_64;
                this.heap_i64[address / 8 + 1] = (x >> 64n) & MASK_64;
            },
            setBigResult: (x: BigIntId) => {
                const address = this.exports.cowel_big_int_big_result;
                this.heap_u32[address / 4 + 0] = Number(x);
            },
            setSmallQuotient: (x: bigint) => {
                const address = this.exports.cowel_big_int_div_result + 0;
                this.heap_i64[address / 8 + 0] = x & MASK_64;
                this.heap_i64[address / 8 + 1] = (x >> 64n) & MASK_64;
            },
            setSmallRemainder: (x: bigint) => {
                const address = this.exports.cowel_big_int_div_result + 16;
                this.heap_i64[address / 8 + 0] = x & MASK_64;
                this.heap_i64[address / 8 + 1] = (x >> 64n) & MASK_64;
            },
            signalDivisionByZero: () => {
                const address = this.exports.cowel_big_int_div_result + 32;
                this.heap_u8[address] = 1;
            },
            readString: (address: Address, length: number): string => {
                return this.decodeUtf8(address, length);
            },
            writeString: (address: Address, length: number, str: string): void => {
                const data = new TextEncoder().encode(str);
                if (data.length > length) {
                    throw new Error(`String too long to write: ${data.length} > ${length}`);
                }
                this.heap_u8.set(data, address);
            },
        };
        this.bigInts = new BigIntApi(bigIntEnv);
    }

    async init(module: BufferSource): Promise<void> {
        this.module = await WebAssembly.compile(module);
        const imports: CowelWasmImports = {
            env: {
                load_file: (
                    resultAddress: Address,
                    pathAddress: Address,
                    pathLength: number,
                    baseFileId: number,
                ) => {
                    const path = this.decodeUtf8(pathAddress, pathLength);
                    this.encodeFileResult(resultAddress, this.loadFile(path, baseFileId));
                },
                log: (diagnosticAddress: Address) => {
                    const diagnostic = this.decodeDiagnostic(diagnosticAddress);
                    this.log(diagnostic);
                },
                emscripten_notify_memory_growth: (byteLength: number) => {
                    this.onMemoryGrowth(byteLength);
                },
                big_int_i32: this.bigInts.big_int_i32.bind(this.bigInts),
                big_int_i64: this.bigInts.big_int_i64.bind(this.bigInts),
                big_int_i128: this.bigInts.big_int_i128.bind(this.bigInts),
                big_int_i192: this.bigInts.big_int_i192.bind(this.bigInts),
                big_int_pow2_i32: this.bigInts.big_int_pow2_i32.bind(this.bigInts),
                big_int_delete: this.bigInts.big_int_delete.bind(this.bigInts),
                big_int_compare_i32: this.bigInts.big_int_compare_i32.bind(this.bigInts),
                big_int_compare_i128: this.bigInts.big_int_compare_i128.bind(this.bigInts),
                big_int_compare: this.bigInts.big_int_compare.bind(this.bigInts),
                big_int_twos_width: this.bigInts.big_int_twos_width.bind(this.bigInts),
                big_int_ones_width: this.bigInts.big_int_ones_width.bind(this.bigInts),
                big_int_neg: this.bigInts.big_int_neg.bind(this.bigInts),
                big_int_bit_not: this.bigInts.big_int_bit_not.bind(this.bigInts),
                big_int_abs: this.bigInts.big_int_abs.bind(this.bigInts),
                big_int_trunc_i128: this.bigInts.big_int_trunc_i128.bind(this.bigInts),
                big_int_add_i32: this.bigInts.big_int_add_i32.bind(this.bigInts),
                big_int_add_i128: this.bigInts.big_int_add_i128.bind(this.bigInts),
                big_int_add: this.bigInts.big_int_add.bind(this.bigInts),
                big_int_sub_i128: this.bigInts.big_int_sub_i128.bind(this.bigInts),
                big_int_sub: this.bigInts.big_int_sub.bind(this.bigInts),
                big_int_mul_i128: this.bigInts.big_int_mul_i128.bind(this.bigInts),
                big_int_mul_i128_i128: this.bigInts.big_int_mul_i128_i128.bind(this.bigInts),
                big_int_mul: this.bigInts.big_int_mul.bind(this.bigInts),
                big_int_div_rem: this.bigInts.big_int_div_rem.bind(this.bigInts),
                big_int_div: this.bigInts.big_int_div.bind(this.bigInts),
                big_int_rem: this.bigInts.big_int_rem.bind(this.bigInts),
                big_int_shl_i128_i32: this.bigInts.big_int_shl_i128_i32.bind(this.bigInts),
                big_int_shl_i32: this.bigInts.big_int_shl_i32.bind(this.bigInts),
                big_int_shr_i32: this.bigInts.big_int_shr_i32.bind(this.bigInts),
                big_int_pow_i128_i32: this.bigInts.big_int_pow_i128_i32.bind(this.bigInts),
                big_int_pow_i32: this.bigInts.big_int_pow_i32.bind(this.bigInts),
                big_int_bit_and_i128: this.bigInts.big_int_bit_and_i128.bind(this.bigInts),
                big_int_bit_and: this.bigInts.big_int_bit_and.bind(this.bigInts),
                big_int_bit_or_i128: this.bigInts.big_int_bit_or_i128.bind(this.bigInts),
                big_int_bit_or: this.bigInts.big_int_bit_or.bind(this.bigInts),
                big_int_bit_xor_i128: this.bigInts.big_int_bit_xor_i128.bind(this.bigInts),
                big_int_bit_xor: this.bigInts.big_int_bit_xor.bind(this.bigInts),
                big_int_to_string: this.bigInts.big_int_to_string.bind(this.bigInts),
                big_int_from_string: this.bigInts.big_int_from_string.bind(this.bigInts),
            },
        };

        this.instance = await WebAssembly.instantiate(this.module, imports);
        this.onMemoryGrowth(this.memory.buffer.byteLength);

        if (this.exports._initialize) {
            this.exports._initialize();
        } else {
            console.warn("module has no export _initialize");
        }
        this.exports.register_assertion_handler();
    }

    async generateHtml(options: GenOptions): Promise<GenResult> {
        this.loadFile = options.loadFile;
        this.log = options.log;

        const allocations = this.makeOptions(options);
        const genResult = this.alloc2(12, 4);

        this.exports.cowel_generate_html_u8(genResult.address, allocations.options.address);
        const result = this.decodeGenResult(genResult.address);

        this.free(genResult);
        this.free(allocations.source);
        this.free(allocations.options);

        return result;
    }

    generateCodeCitationFor(options: CodeCiteOptions): string {
        if (options.length === 0) {
            throw new Error("Cannot generate code citation " +
                "where the target location has zero length.");
        }

        const resultAllocation = this.alloc2(8, 4);
        const stringAllocation = this.allocBytes2(options.source);

        this.exports.generate_code_citation(
            resultAllocation.address,
            stringAllocation.address,
            stringAllocation.size,
            options.line,
            options.column,
            options.begin,
            options.length,
            options.colors,
        );

        const resultAddress = this.heap_u32[resultAllocation.address / 4 + 0];
        const resultLength = this.heap_u32[resultAllocation.address / 4 + 1];
        const result = this.decodeUtf8(resultAddress, resultLength);

        this.free(stringAllocation);
        this.free(resultAllocation);

        return result;
    }

    private onMemoryGrowth(_byteLength: number): void {
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
    private decodeUtf8(address: Address, length: number): string {
        const utf8Bytes = this.heap_u8.subarray(address, address + length);
        const decoder = new TextDecoder("utf-8", { fatal: true });
        return decoder.decode(utf8Bytes);
    }

    /**
     * Decodes a `cowel_diagnostic_u8` located at `address`.
     */
    private decodeDiagnostic(address: Address): Diagnostic {
        const severityNumber = this.heap_i32[address / 4 + 0];
        const idAddress = this.heap_u32[address / 4 + 1];
        const idLength = this.heap_u32[address / 4 + 2];
        const messageAddress = this.heap_u32[address / 4 + 3];
        const messageLength = this.heap_u32[address / 4 + 4];
        const fileNameAddress = this.heap_u32[address / 4 + 5];
        const fileNameLength = this.heap_u32[address / 4 + 6];
        const fileId = this.heap_i32[address / 4 + 7];
        const begin = this.heap_u32[address / 4 + 8];
        const length = this.heap_u32[address / 4 + 9];
        const line = this.heap_u32[address / 4 + 10];
        const column = this.heap_u32[address / 4 + 11];

        const id = this.decodeUtf8(idAddress, idLength);
        const message = this.decodeUtf8(messageAddress, messageLength);
        const fileName = this.decodeUtf8(fileNameAddress, fileNameLength);
        const severity: Severity = severityNumber;

        return {
            severity,
            id,
            message,
            fileName,
            fileId,
            begin, length, line, column,
        };
    }

    /**
     * Decodes a `cowel_gen_result_u8` located at `address`.
     */
    private decodeGenResult(address: Address): GenResult {
        const status: ProcessingStatus = this.heap_i32[address / 4 + 0];
        const outputAddress = this.heap_u32[address / 4 + 1];
        const outputSize = this.heap_u32[address / 4 + 2];
        const output = this.decodeUtf8(outputAddress, outputSize);

        return { status, output };
    }

    /**
     * Encodes a `cowel_file_result_u8` located at `address`.
     */
    private encodeFileResult(address: Address, result: FileResult): void {
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
    private makeOptions(genOptions: GenOptions): { options: Allocation, source: Allocation } {
        const options = this.alloc2(88, 4);
        const source = this.allocUtf8(genOptions.source);

        this.exports.init_options(
            options.address,
            source.address,
            source.size,
            genOptions.mode,
            genOptions.minSeverity,
        );

        return { options, source };
    }

    /**
     * Copies a `string` to a newly allocated UTF-8 array in memory.
     * The returned allocation should later be freed with `_freeUtf8`.
     * @see free2
     */
    private allocUtf8(str: string): Allocation {
        const data = new TextEncoder().encode(str);
        const address = this.allocBytes(data);
        return { address, size: data.length, alignment: 1 };
    }

    /**
     * Allocates space for `bytes.length` bytes with `align` alignment,
     * and copies the given `bytes` to the allocated memory.
     */
    private allocBytes(bytes: Uint8Array | ArrayBuffer, alignment = 1): Address {
        const byteArray = bytes instanceof Uint8Array ? bytes : new Uint8Array(bytes);

        const address = this.alloc(byteArray.length, alignment);
        this.heap_u8.set(byteArray, address);
        return address;
    }

    private allocBytes2(bytes: Uint8Array | ArrayBuffer, alignment = 1): Allocation {
        return {
            address: this.allocBytes(bytes),
            size: bytes.byteLength,
            alignment,
        };
    }

    private alloc(size: number, alignment: number): Address {
        const result = this.exports.cowel_alloc(size, alignment);
        if (result === 0) {
            throw new Error(`Allocation failure in WASM with: size=${size}, align=${alignment}`);
        }
        return result;
    }

    private alloc2(size: number, alignment: number): Allocation {
        return { address: this.alloc(size, alignment), size, alignment };
    }

    private free(address: Address, size: number, alignment: number): void;
    private free(allocation: Allocation): void;
    private free(arg1: Address | Allocation, size?: number, alignment?: number): void {
        if (typeof arg1 === "object") {
            this.exports.cowel_free(arg1.address, arg1.size, arg1.alignment);
        } else {
            this.exports.cowel_free(arg1, size!, alignment!);
        }
    }

    private get memory(): WebAssembly.Memory {
        return this.exports.memory;
    }

    private get exports(): CowelWasmExports {
        return this.instance.exports as unknown as CowelWasmExports;
    }

}

export async function load(module: BufferSource): Promise<CowelWasm> {
    const result = new CowelWasm();
    await result.init(module);
    return result;
}
