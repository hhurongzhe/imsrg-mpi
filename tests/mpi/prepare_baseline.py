#!/usr/bin/env python3
"""Stage an original source copy; modify build plumbing only, never kernels."""
from pathlib import Path
import shutil
import sys

root = Path(__file__).resolve().parents[2]
source = Path(sys.argv[1] if len(sys.argv) > 1 else '/home/hrz/scratch/imsrg/src')
destination = root / 'temp/mpi_bench/original-src'
if not destination.exists():
    shutil.copytree(source, destination)
else:
    # Refuse to silently reuse a baseline with changed physics source files.
    for original in source.rglob('*'):
        if not original.is_file() or original.name == 'CMakeLists.txt':
            continue
        copied = destination / original.relative_to(source)
        if not copied.is_file() or copied.read_bytes() != original.read_bytes():
            raise RuntimeError(f'Baseline differs: {copied}')
cmake = (source/'CMakeLists.txt').read_text().replace(
    'set(CMAKE_CURRENT_BINARY_DIR "../build")', 'set(CMAKE_POSITION_INDEPENDENT_CODE ON)')
cmake += f'''
# Benchmark harness only; original commutator implementations are unchanged.
add_executable(imsrg-original-check {root}/tests/mpi/commutator.cc)
target_include_directories(imsrg-original-check PRIVATE ${{CMAKE_CURRENT_SOURCE_DIR}} {root}/src)
target_link_libraries(imsrg-original-check PRIVATE IMSRG OpenMP::OpenMP_CXX)
'''
(destination/'CMakeLists.txt').write_text(cmake)
print(destination)
