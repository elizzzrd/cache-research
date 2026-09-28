#!/usr/bin/env python3
"""Generate repeatable inputs: total_capacity request_count, then integer keys."""

import argparse
import random
from pathlib import Path


def positive_int(value):
    number = int(value)
    if number <= 0:
        raise argparse.ArgumentTypeError("value must be positive")
    return number


def build_workloads(request_count, seed):
    uniform_rng = random.Random(seed)
    hot_rng = random.Random(seed + 1)
    phase_rng = random.Random(seed + 2)

    hot_scan = []
    next_scan_key = 1000
    phases = []

    for position in range(request_count):
        # Exactly four hot accesses followed by one new scan key.
        if position % 5 < 4:
            hot_scan.append(hot_rng.randrange(8))
        else:
            hot_scan.append(next_scan_key)
            next_scan_key += 1

        # Four phases, each with a different hot set of 24 keys.
        phase = min(3, position * 4 // request_count)
        if phase_rng.random() < 0.95:
            phases.append(phase * 24 + phase_rng.randrange(24))
        else:
            phases.append(phase_rng.randrange(128))

    return {
        #"repeat": [7] * request_count,
        #"scan": list(range(request_count)),
        #"cyclic24": [position % 24 for position in range(request_count)],
        #"uniform128": [uniform_rng.randrange(128) for _ in range(request_count)],
        "hot8_scan": hot_scan,
        "phases24": phases,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--requests", type=positive_int, default=10000)
    parser.add_argument("--capacities", type=positive_int, nargs="+", default=[12, 24, 48, 96])
    parser.add_argument("--seed", type=int, default=42)
    parser.add_argument("--out", type=Path, default=Path(__file__).resolve().parent / "data")
    args = parser.parse_args()

    workloads = build_workloads(args.requests, args.seed)
    args.out.mkdir(parents=True, exist_ok=True)

    count = 0
    for capacity in dict.fromkeys(args.capacities):
        for name, requests in workloads.items():
            path = args.out / f"{name}_c{capacity}_seed{args.seed}.in"
            with path.open("w", encoding="utf-8") as stream:
                stream.write(f"{capacity} {len(requests)}\n")
                for offset in range(0, len(requests), 32):
                    stream.write(" ".join(map(str, requests[offset:offset + 32])) + "\n")
            count += 1

    print(f"Created {count} inputs in {args.out}")


if __name__ == "__main__":
    main()
