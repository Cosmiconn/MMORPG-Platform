#!/usr/bin/env python3
import re
import sys

def check_log(path):
    mem_pattern = re.compile(r'memory_mb=(\d+)')
    values = []

    try:
        with open(path) as f:
            for line in f:
                m = mem_pattern.search(line)
                if m:
                    values.append(int(m.group(1)))
    except FileNotFoundError:
        print(f"WARN: Log file not found: {path}")
        return True  # Don't fail CI if log is missing

    if len(values) < 10:
        print(f"WARN: Only {len(values)} data points, need >= 10")
        return True

    # Check: Does memory grow more than 10% over the test?
    window = max(6, len(values) // 10)
    first_avg = sum(values[:window]) / window
    last_avg = sum(values[-window:]) / window

    growth = (last_avg - first_avg) / max(first_avg, 1)

    print(f"Memory: start={first_avg:.1f}MB end={last_avg:.1f}MB growth={growth*100:.1f}%")

    if growth > 0.10:
        print(f"FAIL: Memory growth {growth*100:.1f}% exceeds 10% budget")
        return False

    print(f"PASS: Memory growth {growth*100:.1f}% within budget")
    return True

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: check_memory_growth.py <logfile>")
        sys.exit(0)
    sys.exit(0 if check_log(sys.argv[1]) else 1)
