#!/usr/bin/env python3
"""Generate every ordered cache-policy combination; default: 3**k, k=1..4."""

import argparse
from itertools import product
from pathlib import Path


KNOWN_POLICIES = ("LRU", "ARC", "2Q", "LFU", "LIRS")


def positive_int(value):
    number = int(value)
    if number <= 0:
        raise argparse.ArgumentTypeError("expected a positive integer")
    return number


def make_configs(policies, min_levels, max_levels, mode):
    configs = {}
    for level_count in range(min_levels, max_levels + 1):
        if mode == "homogeneous":
            combinations = ((policy,) * level_count for policy in policies)
        else:
            combinations = product(policies, repeat=level_count)

        for combination in combinations:
            if mode == "mixed" and len(set(combination)) == 1:
                continue
            filename = "_".join(policy.lower() for policy in combination) + ".cfg"
            configs[filename] = f"{level_count}\n{' '.join(combination)}\n"
    return configs


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--policies", nargs="+", type=str.upper,
                        choices=KNOWN_POLICIES, default=["LRU", "ARC", "2Q", "LFU", "LIRS"])
    parser.add_argument("--min-levels", type=positive_int, default=1)
    parser.add_argument("--max-levels", type=positive_int, default=4)
    parser.add_argument("--mode", choices=["all", "homogeneous", "mixed"], default="all")
    parser.add_argument("--out", type=Path, default=Path("configs/full"))
    parser.add_argument("--baseline-lru", action="store_true",
                        help="also add a one-level LRU baseline, without LRU mixtures")
    parser.add_argument("--dry-run", action="store_true", help="print counts without writing files")
    parser.add_argument("--overwrite", action="store_true",
                        help="replace conflicting files with the expected names")
    args = parser.parse_args()

    if len(args.policies) != len(set(args.policies)):
        parser.error("policy names must be unique")
    if args.min_levels > args.max_levels:
        parser.error("min-levels must not exceed max-levels")
    if args.max_levels > 64:
        parser.error("max-levels exceeds the config parser limit of 64")

    policy_count = len(args.policies)
    count = 0
    for levels in range(args.min_levels, args.max_levels + 1):
        amount = policy_count ** levels
        if args.mode == "homogeneous":
            amount = policy_count
        elif args.mode == "mixed":
            amount -= policy_count
        count += amount
        print(f"Levels {levels}: {amount} configurations")

    baseline_extra = args.baseline_lru and not (
        args.min_levels == 1 and "LRU" in args.policies and args.mode != "mixed"
    )
    count += int(baseline_extra)
    if baseline_extra:
        print("Additional one-level LRU baseline: 1")
    print(f"Total: {count}; output: {args.out}")

    if args.dry_run:
        return
    if count > 100000:
        parser.error("more than 100000 files requested; reduce the level range")

    configs = make_configs(args.policies, args.min_levels, args.max_levels, args.mode)
    if args.baseline_lru:
        configs.setdefault("lru.cfg", "1\nLRU\n")

    # Keep each experiment directory an exact set: never silently retain stale cfg files.
    if args.out.exists() and not args.out.is_dir():
        parser.error(f"output is not a directory: {args.out}")
    extra = {path.name for path in args.out.glob("*.cfg")} - set(configs)
    if extra:
        parser.error("output contains unrelated configs; choose a separate directory: "
                     + ", ".join(sorted(extra)[:5]))

    pending = []
    unchanged = 0
    for name, contents in configs.items():
        path = args.out / name
        if path.exists():
            if not path.is_file():
                parser.error(f"target is not a file: {path}")
            if path.read_text(encoding="utf-8") == contents:
                unchanged += 1
                continue
            if not args.overwrite:
                parser.error(f"different contents in {path}; use --overwrite or another directory")
        pending.append((path, contents))

    args.out.mkdir(parents=True, exist_ok=True)
    for path, contents in pending:
        path.write_text(contents, encoding="utf-8")
    print(f"Written: {len(pending)}; unchanged: {unchanged}")


if __name__ == "__main__":
    main()
