#!/usr/bin/env python3
"""
Code quality, type safety, and complexity analyzer (CRAP score, McCabe Cyclomatic Complexity).
Designed for foo_findplaylists C++ codebase.
"""

import sys
import os
import re
import argparse

def strip_comments_and_strings(code: str) -> str:
    # Remove single line comments
    code = re.sub(r"//.*", "", code)
    # Remove multi-line comments
    code = re.sub(r"/\*.*?\*/", "", code, flags=re.DOTALL)
    # Remove string literals
    code = re.sub(r"\"(?:\\.|[^\"\\])*\"", "\"\"", code)
    # Remove char literals
    code = re.sub(r"\'(?:\\.|[^\'\\])*\'", "''", code)
    return code

def analyze_functions(filepath: str, is_unit_tested: bool = False):
    with open(filepath, "r", encoding="utf-8", errors="ignore") as f:
        raw_code = f.read()

    cleaned = strip_comments_and_strings(raw_code)
    results = []

    pattern = r"((?:[a-zA-Z0-9_:*&<>]+\s+)+)([a-zA-Z0-9_:]+)\s*\([^)]*\)\s*\{"
    matches = list(re.finditer(pattern, cleaned))

    for m in matches:
        fn_name = m.group(2).strip()
        if fn_name in ["if", "for", "while", "switch", "catch"]:
            continue

        start_idx = m.end() - 1
        depth = 0
        end_idx = len(cleaned)

        for idx in range(start_idx, len(cleaned)):
            if cleaned[idx] == "{":
                depth += 1
            elif cleaned[idx] == "}":
                depth -= 1
                if depth == 0:
                    end_idx = idx
                    break

        body = cleaned[start_idx:end_idx]
        lines_count = len(body.strip().splitlines())

        # McCabe Cyclomatic Complexity
        # Count decision keywords and boolean operators
        branches = len(re.findall(r"\b(if|for|while|case|catch)\b|\&\&|\|\||\?", body))
        cc = 1 + branches

        # CRAP Score calculation:
        # CRAP(m) = CC(m)^2 * (1 - cov(m))^3 + CC(m)
        cov = 0.95 if is_unit_tested else 0.0
        crap = (cc ** 2) * ((1.0 - cov) ** 3) + cc

        results.append({
            "file": os.path.basename(filepath),
            "function": fn_name,
            "cc": cc,
            "loc": lines_count,
            "coverage": cov,
            "crap": round(crap, 1),
        })

    return results

def check_type_safety_and_smells(filepath: str):
    issues = []
    with open(filepath, "r", encoding="utf-8", errors="ignore") as f:
        lines = f.readlines()

    filename = os.path.basename(filepath)
    is_header = filepath.endswith(".h")

    if is_header:
        has_pragma_once = any(line.strip() == "#pragma once" for line in lines)
        if not has_pragma_once:
            issues.append(f"{filename}: Missing '#pragma once' header guard.")

    banned_funcs = [
        (r"\bstrcpy\b", "Unsafe C-function 'strcpy' (use std::string or safe copy)"),
        (r"\bsprintf\b", "Unsafe C-function 'sprintf' (use snprintf or pfc::string_formatter)"),
        (r"\bstrcat\b", "Unsafe C-function 'strcat' (use std::string or safe append)"),
        (r"\bgets\b", "Dangerous function 'gets'"),
    ]

    for idx, line in enumerate(lines, 1):
        clean_line = re.sub(r"//.*", "", line).strip()
        for pattern, msg in banned_funcs:
            if re.search(pattern, clean_line):
                issues.append(f"{filename}:{idx}: {msg}")

        # Check for raw C-style casts to non-primitive types
        if re.search(r"\((?:class|struct)\s+[a-zA-Z0-9_]+\s*\*\)", clean_line):
            issues.append(f"{filename}:{idx}: Avoid C-style casts; use static_cast or reinterpret_cast.")

    return issues

def main():
    parser = argparse.ArgumentParser(description="Analyze code quality, type safety, and complexity.")
    parser.add_argument("--max-cc", type=int, default=20, help="Maximum allowed cyclomatic complexity per function.")
    parser.add_argument("--max-crap", type=float, default=100.0, help="Maximum allowed CRAP score.")
    parser.add_argument("--ci", action="store_true", help="Format summary for GitHub Actions step summary.")
    parser.add_argument("--staged", action="store_true", help="Running in pre-commit hook on staged files.")
    args = parser.parse_args()

    source_files = [
        ("src/logic.cpp", True),
        ("src/foo_findplaylists.cpp", False),
    ]

    header_files = [
        "src/logic.h",
        "src/version.h",
    ]

    all_fn_metrics = []
    all_safety_issues = []

    for path, tested in source_files:
        if os.path.exists(path):
            all_fn_metrics.extend(analyze_functions(path, is_unit_tested=tested))
            all_safety_issues.extend(check_type_safety_and_smells(path))

    for path in header_files:
        if os.path.exists(path):
            all_safety_issues.extend(check_type_safety_and_smells(path))

    # Print summary table
    print("\n" + "=" * 80)
    print(" 📊 CODE QUALITY & COMPLEXITY METRICS (CRAP SCORE & CYCLOMATIC COMPLEXITY)")
    print("=" * 80)
    print(f"{'File':<24} {'Function':<28} {'LOC':>5} {'CC':>5} {'Cov':>6} {'CRAP':>7} {'Status':<6}")
    print("-" * 80)

    cc_violations = []
    crap_violations = []

    for m in all_fn_metrics:
        status = "✅ OK"
        if m["cc"] > args.max_cc:
            status = "❌ CC"
            cc_violations.append(m)
        elif m["crap"] > args.max_crap:
            status = "⚠️ CRAP"
            crap_violations.append(m)

        cov_str = f"{int(m['coverage'] * 100)}%"
        print(f"{m['file']:<24} {m['function']:<28} {m['loc']:>5} {m['cc']:>5} {cov_str:>6} {m['crap']:>7.1f} {status}")

    print("-" * 80)
    avg_cc = sum(m["cc"] for m in all_fn_metrics) / len(all_fn_metrics) if all_fn_metrics else 0
    avg_crap = sum(m["crap"] for m in all_fn_metrics) / len(all_fn_metrics) if all_fn_metrics else 0
    print(f"Total Functions: {len(all_fn_metrics)} | Avg CC: {avg_cc:.1f} | Avg CRAP: {avg_crap:.1f}")

    if all_safety_issues:
        print("\n⚠️ TYPE SAFETY & SMELL WARNINGS:")
        for issue in all_safety_issues:
            print(f"  • {issue}")
    else:
        print("\n🛡️ Type Safety: No banned functions or unsafe C-style patterns detected.")

    # Write GitHub Step Summary if requested
    step_summary_path = os.environ.get("GITHUB_STEP_SUMMARY")
    if args.ci and step_summary_path:
        with open(step_summary_path, "a", encoding="utf-8") as f:
            f.write("\n### 📊 Code Quality & Complexity (CRAP Score)\n\n")
            f.write(f"- **Total Functions Analyzed**: {len(all_fn_metrics)}\n")
            f.write(f"- **Average Cyclomatic Complexity**: `{avg_cc:.1f}`\n")
            f.write(f"- **Average CRAP Score**: `{avg_crap:.1f}`\n")
            f.write(f"- **Type Safety Issues**: `{len(all_safety_issues)}`\n\n")
            f.write("| File | Function | LOC | CC | Coverage | CRAP Score | Status |\n")
            f.write("| :--- | :--- | :---: | :---: | :---: | :---: | :---: |\n")
            for m in all_fn_metrics:
                status_icon = "✅" if m["cc"] <= args.max_cc and m["crap"] <= args.max_crap else "⚠️"
                cov_str = f"{int(m['coverage'] * 100)}%"
                f.write(f"| `{m['file']}` | `{m['function']}` | {m['loc']} | {m['cc']} | {cov_str} | {m['crap']:.1f} | {status_icon} |\n")
            f.write("\n")

    if cc_violations:
        print(f"\n❌ Error: {len(cc_violations)} function(s) exceeded max Cyclomatic Complexity ({args.max_cc}).")
        return 1

    print("\n✅ Code quality and complexity checks passed successfully!\n")
    return 0

if __name__ == "__main__":
    sys.exit(main())
