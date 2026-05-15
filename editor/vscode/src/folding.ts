export type FoldRange = [startLine: number, endLine: number];

function isIdentStart(c: string): boolean {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c === '_';
}

function isIdentChar(c: string): boolean {
    return isIdentStart(c) || (c >= '0' && c <= '9');
}

function skipGroup(s: string, i: number): number {
    i++;
    while (i < s.length) {
        const c = s[i];
        if (c === ')') return i + 1;
        if (c === '(') { i = skipGroup(s, i); continue; }
        if (c === '{') { i = skipRawBlock(s, i); continue; }
        if (c === '"') { i = skipString(s, i); continue; }
        if (c === '\\') { i += 2; continue; }
        i++;
    }
    return i;
}

function skipRawBlock(s: string, i: number): number {
    i++;
    while (i < s.length) {
        const c = s[i];
        if (c === '}') return i + 1;
        if (c === '{') { i = skipRawBlock(s, i); continue; }
        if (c === '\\') { i += 2; continue; }
        i++;
    }
    return i;
}

function skipString(s: string, i: number): number {
    i++;
    while (i < s.length) {
        if (s[i] === '"') return i + 1;
        if (s[i] === '\\') i++;
        i++;
    }
    return i;
}

export function computeFoldingRanges(raw_text: string): FoldRange[] {
    const text = raw_text.replace(/\r\n?/g, '\n');
    const ranges: FoldRange[] = [];
    const stack: Array<[line: number, isDirective: boolean]> = [];

    let i = 0;
    let line = 0;

    while (i < text.length) {
        const c = text[i];

        if (c === '\n') {
            line++;
            i++;
            continue;
        }

        if (c === '\\') {
            i++;
            if (i >= text.length) break;
            const next = text[i];

            if (next === ':') {
                i++;
                while (i < text.length && text[i] !== '\n') i++;
            } else if (next === '*') {
                i++;
                while (i < text.length) {
                    if (text[i] === '*' && i + 1 < text.length && text[i + 1] === '\\') {
                        i += 2;
                        break;
                    }
                    if (text[i] === '\n') line++;
                    i++;
                }
            } else if (next === '(') {
                i = skipGroup(text, i);
            } else if (isIdentStart(next)) {
                i++;
                while (i < text.length && isIdentChar(text[i])) i++;
                if (i < text.length && text[i] === '(') {
                    i = skipGroup(text, i);
                }
                if (i < text.length && text[i] === '{') {
                    stack.push([line, true]);
                    i++;
                }
            } else {
                if (next === '\n') line++;
                i++;
            }
        } else if (c === '{') {
            stack.push([line, false]);
            i++;
        } else if (c === '}') {
            const entry = stack.pop();
            if (entry) {
                const [startLine, isDirective] = entry;
                if (isDirective && line > startLine + 1) {
                    ranges.push([startLine, line - 1]);
                }
            }
            i++;
        } else {
            i++;
        }
    }

    return ranges;
}
