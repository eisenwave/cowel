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
import { readFileSync, readdirSync } from "node:fs";
import { fileURLToPath } from "node:url";
import { dirname, join } from "node:path";
import { BigIntApi, createBigIntRegExpWasmEnvObject, RegExpApi, MASK_64, } from "./cowel-wasm.js";
// ── Minimal glob implementation using only Node.js built-ins ─────────────────
// Supports `*` (any chars within a segment), `?` (single char), and `**`
// (zero or more directory levels).
function matchGlobSegment(name, pattern) {
    const regexStr = "^"
        + pattern.replace(/[.+^${}()|[\]\\]/g, "\\$&")
            .replace(/\*/g, "[^/]*")
            .replace(/\?/g, "[^/]")
        + "$";
    return new RegExp(regexStr).test(name);
}
function collectGlobMatches(baseDir, segments, index, results) {
    if (index >= segments.length) {
        return;
    }
    const segment = segments[index];
    const isLast = index === segments.length - 1;
    if (segment === "**") {
        if (!isLast) {
            // Match remaining segments at the current level ...
            collectGlobMatches(baseDir, segments, index + 1, results);
            // ... and recurse into every subdirectory.
            try {
                for (const entry of readdirSync(baseDir, { withFileTypes: true })) {
                    if (entry.isDirectory()) {
                        collectGlobMatches(join(baseDir, entry.name), segments, index, results);
                    }
                }
            }
            catch { /* ignore permission errors etc. */ }
        }
        return;
    }
    try {
        for (const entry of readdirSync(baseDir, { withFileTypes: true })) {
            if (!matchGlobSegment(entry.name, segment)) {
                continue;
            }
            const full = join(baseDir, entry.name);
            if (isLast) {
                if (entry.isFile()) {
                    results.push(full);
                }
            }
            else if (entry.isDirectory()) {
                collectGlobMatches(full, segments, index + 1, results);
            }
        }
    }
    catch { /* ignore */ }
}
function globSync(dir, pattern) {
    const results = [];
    collectGlobMatches(dir, pattern.split("/"), 0, results);
    results.sort();
    return results;
}
class LspWasmRunner {
    instance;
    heap_u8;
    heap_u32;
    heap_i64;
    bigInts;
    regExps;
    constructor() {
        // BigInt environment: reads/writes to WASM memory via heap views.
        // The closures capture `this` so heap view access is always current.
        const bigIntEnv = {
            setSmallResult: (x) => {
                const addr = this.exports.cowel_big_int_small_result;
                this.heap_i64[addr / 8 + 0] = x & MASK_64;
                this.heap_i64[addr / 8 + 1] = (x >> 64n) & MASK_64;
            },
            setBigResult: (x) => {
                const addr = this.exports.cowel_big_int_big_result;
                this.heap_u32[addr / 4] = x;
            },
            setSmallQuotient: (x) => {
                const addr = this.exports.cowel_big_int_div_result;
                this.heap_i64[addr / 8 + 0] = x & MASK_64;
                this.heap_i64[addr / 8 + 1] = (x >> 64n) & MASK_64;
            },
            setSmallRemainder: (x) => {
                const addr = this.exports.cowel_big_int_div_result + 16;
                this.heap_i64[addr / 8 + 0] = x & MASK_64;
                this.heap_i64[addr / 8 + 1] = (x >> 64n) & MASK_64;
            },
            signalDivisionByZero: () => {
                this.heap_u8[this.exports.cowel_big_int_div_result + 32] = 1;
            },
            readString: (addr, len) => this.decodeUtf8(addr, len),
            writeString: (addr, cap, str) => {
                const data = new TextEncoder().encode(str);
                if (data.length > cap) {
                    throw new Error(`String too long to write: ${data.length} > ${cap}`);
                }
                this.heap_u8.set(data, addr);
            },
        };
        this.bigInts = new BigIntApi(bigIntEnv);
        const regExpEnv = {
            readString: (addr, len) => this.decodeUtf8(addr, len),
            writeSearchResult: (addr, index, length) => {
                this.heap_u32[addr / 4 + 0] = index;
                this.heap_u32[addr / 4 + 1] = length;
            },
            allocUtf8: (str) => this.allocUtf8(str),
            writeStringView: (addr, alloc) => {
                this.heap_u32[addr / 4 + 0] = alloc.address;
                this.heap_u32[addr / 4 + 1] = alloc.size;
            },
        };
        this.regExps = new RegExpApi(regExpEnv);
    }
    async init(wasmBytes) {
        const module = await WebAssembly.compile(wasmBytes);
        const bi = this.bigInts;
        const re = this.regExps;
        const envBase = createBigIntRegExpWasmEnvObject(bi, re, () => {
            this.onMemoryGrowth();
        });
        const cowel_lsp_host_log_fatal = (type, msgPtr, msgLen, filePtr, fileLen, funcPtr, funcLen, line) => {
            const kind = type === 0 ? "assert.fail" : "assert.unreachable";
            const message = this.decodeUtf8(msgPtr, msgLen);
            const file = this.decodeUtf8(filePtr, fileLen);
            const func = this.decodeUtf8(funcPtr, funcLen);
            const output = `cowel-lsp ${kind}: ${message} (in ${func} at ${file}:${line})\n`;
            process.stderr.write(output);
        };
        const cowel_lsp_host_read_file = (pathPtr, pathLen, outPtrAddr, outLenAddr) => {
            const filePath = this.decodeUtf8(pathPtr, pathLen);
            try {
                const data = readFileSync(filePath);
                const contentPtr = this.exports.cowel_alloc(data.length, 1);
                // Refresh views in case
                // `cowel_alloc` triggered memory growth.
                // TODO: investigate if this is actually necessary.
                this.onMemoryGrowth();
                this.heap_u8.set(data, contentPtr);
                this.heap_u32[outPtrAddr / 4] = contentPtr;
                this.heap_u32[outLenAddr / 4] = data.length;
                return true;
            }
            catch {
                return false;
            }
        };
        const cowel_lsp_host_glob = (dirPtr, dirLen, patternPtr, patternLen, outPtrAddr, outLenAddr) => {
            const dir = this.decodeUtf8(dirPtr, dirLen);
            const pattern = this.decodeUtf8(patternPtr, patternLen);
            const matches = globSync(dir, pattern);
            if (matches.length === 0) {
                return 0;
            }
            // Encode as null-terminated UTF-8 strings,
            // concatenated into one block.
            const encoded = matches.map((m) => Buffer.from(`${m}\0`, "utf8"));
            const total = encoded.reduce((sum, b) => sum + b.length, 0);
            const contentPtr = this.exports.cowel_alloc(total, 1);
            // Refresh views in case cowel_alloc triggered memory growth.
            // TODO: investigate if this is actually necessary.
            this.onMemoryGrowth();
            let offset = contentPtr;
            for (const buf of encoded) {
                this.heap_u8.set(buf, offset);
                offset += buf.length;
            }
            this.heap_u32[outPtrAddr / 4] = contentPtr;
            this.heap_u32[outLenAddr / 4] = total;
            return 1;
        };
        const imports = {
            env: {
                ...envBase,
                cowel_lsp_host_log_fatal,
                cowel_lsp_host_read_file,
                cowel_lsp_host_glob,
            },
        };
        this.instance = await WebAssembly.instantiate(module, imports);
        this.onMemoryGrowth();
        this.exports._initialize();
        this.exports.cowel_lsp_register_assertion_handler();
    }
    /**
     * Sends a JSON-RPC message body (bytes only, no Content-Length header)
     * through the WASM LSP and returns the accumulated framed response bytes.
     * Returns an empty Buffer when the message produced no output.
     */
    processMessage(body) {
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
    shouldExit() {
        return this.exports.cowel_lsp_should_exit();
    }
    get exports() {
        return this.instance.exports;
    }
    onMemoryGrowth() {
        const buf = this.instance.exports.memory.buffer;
        this.heap_u8 = new Uint8Array(buf);
        this.heap_u32 = new Uint32Array(buf);
        this.heap_i64 = new BigInt64Array(buf);
    }
    decodeUtf8(address, length) {
        return new TextDecoder().decode(this.heap_u8.subarray(address, address + length));
    }
    allocUtf8(str) {
        const data = new TextEncoder().encode(str);
        const address = this.exports.cowel_alloc(data.length + 1, 1);
        this.heap_u8.set(data, address);
        this.heap_u8[address + data.length] = 0; // null terminator
        return { address, size: data.length, alignment: 1 };
    }
}
async function main() {
    const wasmPath = join(dirname(fileURLToPath(import.meta.url)), "cowel-lsp.wasm");
    const wasmBytes = readFileSync(wasmPath);
    const runner = new LspWasmRunner();
    await runner.init(wasmBytes);
    // Accumulate stdin chunks and extract Content-Length-framed messages.
    let buffer = Buffer.alloc(0);
    for await (const chunk of process.stdin) {
        buffer = Buffer.concat([buffer, chunk]);
        // Parse as many complete messages as are available in the buffer.
        while (true) {
            const headerEnd = buffer.indexOf("\r\n\r\n");
            if (headerEnd === -1)
                break;
            const header = buffer.subarray(0, headerEnd).toString("ascii");
            const match = /Content-Length: (\d+)/i.exec(header);
            if (!match)
                break; // malformed header; wait for more data
            const bodyLength = parseInt(match[1], 10);
            const bodyStart = headerEnd + 4;
            if (buffer.length < bodyStart + bodyLength)
                break;
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
main().catch((err) => {
    process.stderr.write(`cowel-lsp-wasm-runner: ${String(err)}\n`);
    process.exit(1);
});
