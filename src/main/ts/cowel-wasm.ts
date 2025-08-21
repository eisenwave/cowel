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

interface CowelWasmExports {
    memory: WebAssembly.Memory;
    __indirect_function_table: WebAssembly.Table;
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
    ): void
}

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

    private loadFile!: (path: string, baseFileId: number) => FileResult;
    private log!: (diagnostic: Diagnostic) => void;

    constructor() { }

    async init(module: BufferSource): Promise<void> {
        this.module = await WebAssembly.compile(module);

        this.instance = await WebAssembly.instantiate(this.module, {
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
            },
        });
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
