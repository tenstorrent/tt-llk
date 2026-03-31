========================================
  LLK CodeGen — Generation Complete
========================================
Prompt:           Generate activations for Quasar
Kernel:           activations
Kernel Type:      sfpu
Target Arch:      quasar
Reference:        tt_llk_blackhole/common/inc/sfpu/ckernel_sfpu_activations.h
Generated File:   tt_llk_quasar/common/inc/sfpu/ckernel_sfpu_activations.h
Lines Generated:  112
----------------------------------------
Timing:
  Start:          2026-03-30T21:14:56Z
  End:            2026-03-30T22:36:09Z
----------------------------------------
Tokens:
  Input:          0
  Output:         0
  Cache Read:     0
  Total:          0
----------------------------------------
Quality:
  Phases:           2/2
  Compile Attempts: 4 (across all phases)
  Debug Cycles:     2
  Compilation:      PASSED
  Functional Tests: PASSED (156/156)
  Tests Source:     GENERATED
  Prettified:       DISABLED
----------------------------------------
Per Phase:
  Phase 1 (CELU): compile_attempts=2, debug=1, test=passed (78/78)
  Phase 2 (Hardsigmoid): compile_attempts=2, debug=1, test=passed (78/78)
----------------------------------------
Failures: 2 (2 resolved, 0 unresolved)
  [test_failure] test_phase_1 (tester): CELU negative branch returns 0.00 — missing SFPNOP between SFPMUL and SFPNONLINEAR (pipeline hazard) — RESOLVED
  [test_failure] test_phase_2 (tester): Hardsigmoid lower clamp fails — missing SFPENCC between upper and lower clamp sequences — RESOLVED
----------------------------------------
Artifacts:
  - codegen/artifacts/activations_arch_research.md
  - codegen/artifacts/activations_analysis.md
  - codegen/artifacts/activations_phase1_spec.md
  - codegen/artifacts/activations_phase2_spec.md
Metrics:
  - /proj_sw/user_dev/llk_code_gen/quasar/runs.jsonl
  - /proj_sw/user_dev/llk_code_gen/quasar/2026-03-30_activations_quasar_6b720d80/
========================================
