#!/usr/bin/env python3
"""Identical observational phase markers for current and staged original main."""
from pathlib import Path
import sys
p = Path(sys.argv[1])
s = p.read_text()
if '#include "PerformanceProfile.hh"' in s:
    raise SystemExit('Already instrumented: ' + str(p))
s = s.replace('#include "IMSRG.hh"', '#include "IMSRG.hh"\n#include "PerformanceProfile.hh"', 1)
def before(anchor, label):
    global s
    assert s.count(anchor) == 1, (anchor, s.count(anchor))
    s = s.replace(anchor, f'  imsrg_profile::checkpoint("{label}");\n' + anchor)
before('  Parameters parameters(argc,argv);', 'start')
before('  std::cout << "Reading interactions..."', 'modelspace_setup')
before('  // Read in the 3-body file', 'read_nn')
before('  if (store_3bme_pn)', 'read_3n')
before('  std::cout << "Creating HF"', 'hamiltonian_setup')
before('  if ( method == "HF" )', 'hf')
before('  if (perturbative_triples)\n', 'normal_order_h')
before('  IMSRGSolver imsrgsolver(HNO);', 'operator_setup_and_mbpt')
anchor='  imsrgsolver.Solve();\n\n  if (IMSRG3)'
assert s.count(anchor)==1
s=s.replace(anchor, '  imsrg_profile::checkpoint("flow_setup");\n  imsrgsolver.Solve();\n  imsrg_profile::checkpoint("flow_core");\n\n  if (IMSRG3)')
anchor='    imsrgsolver.SetSmax(smax);\n    imsrgsolver.Solve();\n  }'
assert s.count(anchor)==1
s=s.replace(anchor, '    imsrgsolver.SetSmax(smax);\n    imsrg_profile::checkpoint("valence_setup");\n    imsrgsolver.Solve();\n    imsrg_profile::checkpoint("flow_valence");\n  }')
anchor='      op = imsrgsolver.Transform(op);'
assert s.count(anchor)==1
s=s.replace(anchor, '      imsrg_profile::checkpoint("prepare_" + opnames[i]);\n'+anchor+'\n      imsrg_profile::checkpoint("evolve_" + opnames[i]);')
anchor='  Hbare.PrintTimes();\n\n  return 0;'
assert s.count(anchor)==1
s=s.replace(anchor, '  imsrg_profile::checkpoint("final_output");\n'+anchor)
p.write_text(s)
