/**
 * @file lsp-wasm-runner.ts
 * @brief Standalone Node.js bridge that runs cowel-lsp.wasm as an LSP server.
 *
 * Loads cowel-lsp.wasm from the same directory as this script,
 * wires up the big_int_* and reg_exp_* WASM imports by reusing BigIntApi and
 * RegExpApi from cowel-wasm.ts,
 * then forwards Content-Length-framed JSON-RPC messages from process.stdin
 * through the WASM and writes any output to process.stdout.
 */

import { readFileSync } from "node:fs";
import { fileURLToPath } from "node:url";
import { dirname, join } from "node:path";
import {
    type Address,
    type Allocation,
    type BigIntId,
    BigIntApi,
    BigIntEnvironment,
    createBigIntRegExpWasmEnvObject,
    RegExpApi,
    RegExpEnvironment,
    MASK_64,
} from "./cowel-wasm.js";

// ── WASM exports type ────────────────────────────────────────────────────────

type LspWasmExports = WebAssembly.Exports & {
    readonly memory: WebAssembly.Memory;
    _initialize(): void;
    cowel_alloc(size: number, alignment: number): Address;
    cowel_free(address: Address, size: number, alignment: number): void;
    readonly cowel_big_int_small_result: Address;
    readonly cowel_big_int_big_result: Address;
    readonly cowel_big_int_div_result: Address;
    cowel_lsp_process_message(ptr: Address, len: number): void;
    cowel_lsp_get_output_ptr(): Address;
    cowel_lsp_get_output_length(): number;
    cowel_lsp_should_exit(): boolean;
};

// ── WASM runner ─────────────────────────────────────────────────────────────

class LspWasmRunner {
    private instance!: WebAssembly.Instance;
    private heap_u8!: Uint8Array;
    private heap_u32!: Uint32Array;
    private heap_i64!: BigInt64Array;
    private readonly bigInts: BigIntApi;
    private readonly regExps: RegExpApi;

    constructor() {
        // BigInt environment: reads/writes to WASM memory via heap views.
        // The closures capture `this` so heap view access is always current.
        const bigIntEnv: BigIntEnvironment = {
            setSmallResult: (x: bigint) => {
                const addr = this.exports.cowel_big_int_small_result;
                this.heap_i64[addr / 8 + 0] = x & MASK_64;
                this.heap_i64[addr / 8 + 1] = (x >> 64n) & MASK_64;
            },
            setBigResult: (x: BigIntId) => {
                const addr = this.exports.cowel_big_int_big_result;
                this.heap_u32[addr / 4] = x;
            },
            setSmallQuotient: (x: bigint) => {
                const addr = this.exports.cowel_big_int_div_result;
                this.heap_i64[addr / 8 + 0] = x & MASK_64;
                this.heap_i64[addr / 8 + 1] = (x >> 64n) & MASK_64;
            },
            setSmallRemainder: (x: bigint) => {
                const addr = this.exports.cowel_big_int_div_result + 16;
                this.heap_i64[addr / 8 + 0] = x & MASK_64;
                this.heap_i64[addr / 8 + 1] = (x >> 64n) & MASK_64;
            },
            signalDivisionByZero: () => {
                this.heap_u8[this.exports.cowel_big_int_div_result + 32] = 1;
            },
            readString: (addr: Address, len: number) =>
                this.decodeUtf8(addr, len),
            writeString: (addr: Address, cap: number, str: string) => {
                const data = new TextEncoder().encode(str);
                if (data.length > cap) {
                    throw new Error(`String too long to write: ${data.length} > ${cap}`);
                }
                this.heap_u8.set(data, addr);
            },
        };
        this.bigInts = new BigIntApi(bigIntEnv);

        const regExpEnv: RegExpEnvironment = {
            readString: (addr: Address, len: number) =>
                this.decodeUtf8(addr, len),
            writeSearchResult: (
                addr: Address,
                index: number,
                length: number,
            ) => {
                this.heap_u32[addr / 4 + 0] = index;
                this.heap_u32[addr / 4 + 1] = length;
            },
            allocUtf8: (str: string) => this.allocUtf8(str),
            writeStringView: (addr: Address, alloc: Allocation) => {
                this.heap_u32[addr / 4 + 0] = alloc.address;
                this.heap_u32[addr / 4 + 1] = alloc.size;
            },
        };
        this.regExps = new RegExpApi(regExpEnv);
    }

    async init(wasmBytes: Uint8Array): Promise<void> {
        const module = await WebAssembly.compile(wasmBytes);
        const bi = this.bigInts;
        const re = this.regExps;
        const imports = {
            env: createBigIntRegExpWasmEnvObject(
                bi,
                re,
                () => {
                    this.onMemoryGrowth();
                },
            ),
        };

        this.instance = await WebAssembly.instantiate(module, imports);
        this.onMemoryGrowth();
        this.exports._initialize();
    }

    /**
     * Sends a JSON-RPC message body (bytes only, no Content-Length header)
     * through the WASM LSP and returns the accumulated framed response bytes.
     * Returns an empty Buffer when the message produced no output.
     */
    processMessage(body: Buffer): Buffer {
        const len = body.length;
        const ptr = this.exports.cowel_alloc(len, 1);
        this.heap_u8.set(body, ptr);
        this.exports.cowel_lsp_process_message(ptr, len);
        this.exports.cowel_free(ptr, len, 1);

        const outPtr = this.exports.cowel_lsp_get_output_ptr();
        const outLen = this.exports.cowel_lsp_get_output_length();
        return Buffer.from(this.heap_u8.subarray(outPtr, outPtr + outLen));
    }

    /** Returns true if the LSP client sent an "exit" notification. */
    shouldExit(): boolean {
        return this.exports.cowel_lsp_should_exit();
    }

    private get exports(): LspWasmExports {
        return this.instance.exports as LspWasmExports;
    }

    private onMemoryGrowth(): void {
        const buf = (this.instance.exports as LspWasmExports).memory.buffer;
        this.heap_u8 = new Uint8Array(buf);
        this.heap_u32 = new Uint32Array(buf);
        this.heap_i64 = new BigInt64Array(buf);
    }

    private decodeUtf8(address: Address, length: number): string {
        return new TextDecoder().decode(this.heap_u8.subarray(address, address + length));
    }

    private allocUtf8(str: string): Allocation {
        const data = new TextEncoder().encode(str);
        const address = this.exports.cowel_alloc(data.length + 1, 1);
        this.heap_u8.set(data, address);
        this.heap_u8[address + data.length] = 0; // null terminator
        return { address, size: data.length, alignment: 1 };
    }
}

// ── stdin/stdout bridge ─────────────────────────────────────────────────────

async function main(): Promise<void> {
    const wasmPath = join(
        dirname(fileURLToPath(import.meta.url)),
        "cowel-lsp.wasm",
    );
    const wasmBytes = readFileSync(wasmPath);

    const runner = new LspWasmRunner();
    await runner.init(wasmBytes);

    // Accumulate stdin chunks and extract Content-Length-framed messages.
    let buffer = Buffer.alloc(0);

    for await (const chunk of process.stdin) {
        buffer = Buffer.concat([buffer, chunk as Buffer]);

        // Parse as many complete messages as are available in the buffer.
        while (true) {
            const headerEnd = buffer.indexOf("\r\n\r\n");
            if (headerEnd === -1) break;

            const header = buffer.subarray(0, headerEnd).toString("ascii");
            const match = /Content-Length: (\d+)/i.exec(header);
            if (!match) break; // malformed header; wait for more data

            const bodyLength = parseInt(match[1], 10);
            const bodyStart = headerEnd + 4;
            if (buffer.length < bodyStart + bodyLength) break;

            const body = buffer.subarray(bodyStart, bodyStart + bodyLength);
            buffer = buffer.subarray(bodyStart + bodyLength);

            const output = runner.processMessage(body);
            if (output.length > 0) {
                process.stdout.write(output);
            }

            if (runner.shouldExit()) {
                process.exit(0);
            }
        }
    }
}

main().catch((err: unknown) => {
    process.stderr.write(`cowel-lsp-wasm-runner: ${String(err)}\n`);
    process.exit(1);
});
