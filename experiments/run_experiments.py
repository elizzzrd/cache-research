#!/usr/bin/env python3
"""Run the compiled cache program over configs and input files, saving a CSV."""

import argparse
import csv
import subprocess
from pathlib import Path


def read_result(stdout):
    result = {}
    for line in stdout.splitlines():
        if "=" in line:
            key, value = line.split("=", 1)
            result[key.strip()] = value.strip()

    numeric_fields = (
        "levels", "total_capacity", "requests", "system_hits",
        "source_loads", "ideal_hits", "ideal_source_loads",
    )
    for field in numeric_fields:
        if field not in result:
            raise ValueError(f"Program output is missing {field}")
        result[field] = int(result[field])
        if result[field] < 0:
            raise ValueError(f"Negative {field}")

    count = result["requests"]
    if count == 0:
        raise ValueError("Empty experiment")
    if result["system_hits"] + result["source_loads"] != count:
        raise ValueError("System hits and loads do not add up to request count")
    if result["ideal_hits"] + result["ideal_source_loads"] != count:
        raise ValueError("Ideal hits and loads do not add up to request count")

    capacities = list(map(int, result["capacities"].split()))
    if len(capacities) != result["levels"] or any(size <= 0 for size in capacities):
        raise ValueError("Invalid level capacities")
    if sum(capacities) != result["total_capacity"]:
        raise ValueError("Level capacities do not add up to total capacity")

    result["hit_rate"] = result["system_hits"] / count
    result["ideal_hit_rate"] = result["ideal_hits"] / count
    result["excess_source_loads"] = result["source_loads"] - result["ideal_source_loads"]
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", type=Path, default=Path("build/cache"))
    parser.add_argument("--configs", type=Path, default=Path("configs"))
    parser.add_argument("--data", type=Path, default=Path("experiments/data"))
    parser.add_argument("--output", type=Path, default=Path("experiments/results.csv"))
    args = parser.parse_args()

    binary = args.binary.resolve()
    configs = sorted(args.configs.glob("*.cfg"))
    inputs = sorted(args.data.glob("*.in"))
    if not binary.is_file():
        parser.error(f"Executable not found: {binary}; build the project first")
    if not configs or not inputs:
        parser.error("Need at least one .cfg file and one .in file")

    fields = [
        "config", "input", "policies", "levels", "capacities", "total_capacity",
        "requests", "system_hits", "source_loads", "hit_rate", "ideal_hits",
        "ideal_source_loads", "ideal_hit_rate", "excess_source_loads",
    ]
    rows = []

    for data_path in inputs:
        for config_path in configs:
            with data_path.open("r", encoding="utf-8") as source:
                run = subprocess.run(
                    [str(binary), str(config_path)], stdin=source,
                    capture_output=True, text=True, check=False,
                )
            if run.returncode != 0:
                raise RuntimeError(
                    f"Failed: {config_path}, {data_path}\n{run.stderr.strip()}"
                )

            row = read_result(run.stdout)
            row["config"] = str(config_path)
            row["input"] = str(data_path)
            rows.append(row)

        print(f"Completed {len(rows)}/{len(configs) * len(inputs)} runs: {data_path.name}", flush=True)

    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=fields, extrasaction="ignore")
        writer.writeheader()
        writer.writerows(rows)

    print(f"Saved {len(rows)} rows to {args.output}")


if __name__ == "__main__":
    main()
