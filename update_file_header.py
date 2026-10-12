import os

copyright_name = "Lucas G. (aka TheAllocator32)"
new_header_lines = [
    " * This file is part of Orka Engine - the game engine that stays out of your way",
    " *",
    f" * Copyright (c) 2024-2026 {copyright_name}",
    " *",
    " * Licensed under the Apache License, Version 2.0.",
    " * See the LICENSE file in the project root for details."
]
new_header_lines = [line + '\n' for line in new_header_lines]

def replace_orka_license_block(lines):
    start_idx, end_idx, block_end_idx = None, None, None
    for i, line in enumerate(lines):
        if "This File is part of" in line or "This file is part of" in line:
            if "Orka Engine" in line:
                start_idx = i
                break
    if start_idx is not None:
        end_idx = start_idx
        for j in range(start_idx, min(len(lines), start_idx + 20)):
            if ("See the ORKA_LICENSE" in lines[j] or
                "See the LICENSE" in lines[j] or
                lines[j].strip() == "*" or
                lines[j].strip() == "*/"):
                end_idx = j
                break
        block_end_idx = end_idx
        while block_end_idx < len(lines):
            if lines[block_end_idx].strip().endswith("*/"):
                break
            block_end_idx += 1
        before = lines[:start_idx]
        after = lines[block_end_idx+1:] if block_end_idx+1 < len(lines) else []
        result = before + new_header_lines
        result.append(" */\n")
        result += after
        while result and result[-1].strip() == "":
            result = result[:-1]
        result.append('\n')
        return result
    else:
        return lines

def replace_header_in_file(filepath):
    with open(filepath, "r", encoding="utf-8", errors="replace") as f:
        lines = f.readlines()
    new_lines = replace_orka_license_block(lines)
    if new_lines != lines:
        with open(filepath, "w", encoding="utf-8", errors="replace") as f:
            f.writelines(new_lines)
        print(f"Updated header in {filepath}")

def scan_and_replace(root="."):
    for dirpath, _, filenames in os.walk(root):
        for filename in filenames:
            if filename.endswith((".cpp", ".hpp", ".c", ".h", ".ixx")):
                replace_header_in_file(os.path.join(dirpath, filename))

if __name__ == "__main__":
    scan_and_replace(".")
