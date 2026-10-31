#!/usr/bin/env python3
"""Validate full random commutators, flow observables and valence operators."""
import array
import math
from pathlib import Path
import re
import sys

root = Path(sys.argv[1])
failed = False
rank_counts = tuple(map(int, sys.argv[2:])) or (1, 2, 4)
for ref in sorted(root.glob('original_*.bin')):
    a = array.array('d')
    a.frombytes(ref.read_bytes())
    for ranks in rank_counts:
        test = root / ref.name.replace('original_', f'mpi{ranks}_')
        b = array.array('d')
        b.frombytes(test.read_bytes())
        assert len(a) == len(b), test
        error = max(abs(x-y) for x, y in zip(a, b))
        scale = max(1., max(map(abs, a)))
        ok = all(math.isfinite(x) for x in b) and error <= 1e-11*scale
        failed |= not ok
        print(test.name, f'elements={len(a)} max_error={error:.4e}', 'PASS' if ok else 'FAIL')

for refdir in sorted(root.glob('*_original')):
    if not refdir.is_dir():
        continue
    required = ['resultRp2.snt']
    if refdir.name.startswith('O18_'):
        required += ['result.snt', 'resultE2_2b.snt', 'resultM1_2b.snt']
    for mode in ('original', *(f'mpi{n}' for n in rank_counts)):
        directory = root / refdir.name.replace('_original', f'_{mode}')
        for filename in required:
            assert (directory / filename).is_file(), (directory, filename)
        log = (directory / 'console.log').read_text().replace('\x00', '')
        assert re.search(r'^real\s+\S+', log, re.M), ('incomplete run', directory)
    # Physical columns only; elapsed time, memory and operation counts differ.
    def flow(path):
        return [[float(v) for v in [line.split()[i] for i in (0,1,2,3,4,5,6,7,8,9,10,12)]] for line in path.read_text().replace('\x00', '').splitlines()
                if re.match(r'^\s*\d+\s+[-+\d.]', line)]
    for ranks in rank_counts:
        testdir = root / refdir.name.replace('_original', f'_mpi{ranks}')
        a, b = flow(refdir/'flow.dat'), flow(testdir/'flow.dat')
        # A different number of steps is a failure, but still compare all
        # exported operators so a failed flow does not hide other diagnostics.
        same_steps = bool(a) and len(a) == len(b)
        error = max((abs(x-y) for ra, rb in zip(a, b) for x, y in zip(ra, rb)), default=math.inf)
        ok = same_steps and error <= 2e-8 and all(math.isfinite(v) for row in b for v in row)
        failed |= not ok
        print(testdir.name, f'flow_rows={len(a)}/{len(b)} max_error={error:.4e}', 'PASS' if ok else 'FAIL')
        for ref in sorted(refdir.glob('result*')):
            if not ref.is_file(): continue
            test = testdir / ref.name
            def numbers(path):
                lines = [l for l in path.read_text().replace('\x00', '').splitlines() if l.strip() and not l.lstrip().startswith(('#','!'))]
                return [float(t) for l in lines for t in l.split()
                        if re.fullmatch(r'[-+]?(?:\d+\.?\d*|\.\d+)(?:[eE][-+]?\d+)?',t)]
            a, b = numbers(ref), numbers(test)
            assert len(a) == len(b), test
            error = max((abs(x-y) for x,y in zip(a,b)), default=0.)
            ok = all(math.isfinite(x) for x in b) and error <= 2e-9
            def zero_body(path):
                match = re.search(r'Zero body term:\s*([-+0-9.eE]+)', path.read_text().replace('\x00', ''))
                return float(match.group(1)) if match else 0.
            zero_error = abs(zero_body(ref)-zero_body(test))
            ok &= math.isfinite(zero_error) and zero_error <= 2e-7
            failed |= not ok
            print(test.parent.name, test.name, f'max_error={error:.4e} zero_error={zero_error:.4e}', 'PASS' if ok else 'FAIL')
    for name in ('original', *(f'mpi{n}' for n in rank_counts)):
        text = (root/refdir.name.replace('_original',f'_{name}')/'console.log').read_text().replace('\x00', '')
        print(refdir.name, name, 'wall_seconds=', re.findall(r'^real\s+(\S+)',text,re.M))
raise SystemExit(int(failed))
