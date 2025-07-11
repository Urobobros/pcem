#!/usr/bin/env python3
import json, sys, difflib

def normalize(path):
    lines = []
    with open(path, 'r') as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            try:
                obj = json.loads(line)
                line = json.dumps(obj, sort_keys=True)
            except Exception:
                pass
            lines.append(line)
    return sorted(lines)

def main(a, b):
    la = normalize(a)
    lb = normalize(b)
    for l in difflib.unified_diff(la, lb, fromfile=a, tofile=b, lineterm=''):
        print(l)

if __name__ == '__main__':
    if len(sys.argv) != 3:
        print('Usage: compare_logs.py <log_a> <log_b>')
        sys.exit(1)
    main(sys.argv[1], sys.argv[2])
