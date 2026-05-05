#!/usr/bin/env python3

from __future__ import annotations

import argparse
from pathlib import Path
from typing import Iterable


def excluded_lines_for_file(file_path: Path) -> set[int]:
    if not file_path.exists():
        return set()

    excluded: set[int] = set()
    in_block = False

    for index, line in enumerate(file_path.read_text(encoding="utf-8", errors="ignore").splitlines(), 1):
        if "LCOV_EXCL_START" in line:
            in_block = True
            excluded.add(index)
            continue

        if in_block:
            excluded.add(index)

        if "LCOV_EXCL_STOP" in line or "LCOV_EXCL_END" in line:
            in_block = False
            excluded.add(index)
            continue

        if "LCOV_EXCL_LINE" in line:
            excluded.add(index)
            continue

        # This is a common pattern that immediately leads to unreachable code,
        # so should treat it like the assertion it jumps into.
        if "default: break;" in line:
            excluded.add(index)

        # Treat assertions as ghost code.
        if "COWEL_ASSERT" in line or "COWEL_DEBUG_ASSERT" in line:
            excluded.add(index)

    return excluded


def parse_line_number(record_line: str) -> int | None:
    if record_line.startswith("DA:"):
        return int(record_line.split(":", 1)[1].split(",", 1)[0])
    if record_line.startswith("BRDA:"):
        return int(record_line.split(":", 1)[1].split(",", 1)[0])
    return None


def update_summary(lines: Iterable[str]) -> list[str]:
    result: list[str] = []
    line_details: list[tuple[int, int]] = []
    branch_details: list[str] = []

    for line in lines:
        if line.startswith("LF:") or line.startswith("LH:") or line.startswith("BRF:") or line.startswith("BRH:"):
            continue

        result.append(line)

        if line.startswith("DA:"):
            payload = line.split(":", 1)[1]
            line_no, hit = payload.split(",", 1)
            line_details.append((int(line_no), int(hit)))

        if line.startswith("BRDA:"):
            branch_details.append(line)

    lf = len(line_details)
    lh = sum(1 for _, hit in line_details if hit > 0)
    brf = len(branch_details)
    def branch_is_hit(branch_line: str) -> bool:
        taken = branch_line.rsplit(",", 1)[1]
        return taken != "-" and int(taken) > 0

    brh = sum(1 for line in branch_details if branch_is_hit(line))

    output: list[str] = []
    inserted_line_summary = False
    inserted_branch_summary = False

    for line in result:
        output.append(line)

        if line.startswith("FNH:") and not inserted_line_summary:
            output.append(f"LF:{lf}")
            output.append(f"LH:{lh}")
            inserted_line_summary = True

        if line.startswith("BRDA:") and not inserted_branch_summary:
            output.append(f"BRF:{brf}")
            output.append(f"BRH:{brh}")
            inserted_branch_summary = True

    if not inserted_line_summary:
        output.append(f"LF:{lf}")
        output.append(f"LH:{lh}")

    if brf > 0 and not inserted_branch_summary:
        output.append(f"BRF:{brf}")
        output.append(f"BRH:{brh}")

    return output


def filter_lcov(input_path: Path) -> str:
    text = input_path.read_text(encoding="utf-8", errors="ignore")
    records = text.split("end_of_record\n")

    filtered_records: list[str] = []

    for record in records:
        if not record.strip():
            continue

        lines = [line for line in record.splitlines() if line]
        sf_line = next((line for line in lines if line.startswith("SF:")), None)
        if sf_line is None:
            filtered_records.append("\n".join(lines) + "\nend_of_record\n")
            continue

        source_path = Path(sf_line.split(":", 1)[1])
        excluded_lines = excluded_lines_for_file(source_path)

        kept: list[str] = []
        for line in lines:
            line_no = parse_line_number(line)
            if line_no is not None and line_no in excluded_lines:
                continue
            kept.append(line)

        kept = update_summary(kept)
        filtered_records.append("\n".join(kept) + "\nend_of_record\n")

    return "".join(filtered_records)


def main() -> None:
    parser = argparse.ArgumentParser(description="Filter LCOV lines using source exclusions.")
    parser.add_argument("lcov_file", type=Path)
    args = parser.parse_args()

    filtered = filter_lcov(args.lcov_file)
    args.lcov_file.write_text(filtered, encoding="utf-8")


if __name__ == "__main__":
    main()
