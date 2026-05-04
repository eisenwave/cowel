#!/usr/bin/node

import { readFile, writeFile } from "fs/promises";
import { argv } from "process";

if (argv.length < 4) {
    console.error(`Usage: node ${argv[1]} <input.json> <output.json>`);
    process.exit(1);
}

const [inputPath, outputPath] = argv.slice(2);

const content = await readFile(inputPath, "utf-8");
const json = JSON.parse(content);

delete json.scripts;
delete json.devDependencies;

await writeFile(outputPath, JSON.stringify(json, null, 2) + "\n");
