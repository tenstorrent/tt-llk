========================================
  LLK CodeGen — Generation Complete
========================================
Prompt:           Generate exp2 for Quasar
Kernel:           exp2
Kernel Type:      sfpu
Target Arch:      quasar
Reference:        tt_llk_blackhole/common/inc/sfpu/ckernel_sfpu_exp2.h
Generated File:   tt_llk_quasar/common/inc/sfpu/ckernel_sfpu_exp2.h
Lines Generated:  50
----------------------------------------
Timing:
  Start:          2026-03-30T16:25:22Z
  End:            2026-03-30T17:38:47Z
----------------------------------------
Tokens:
  Input:          0
  Output:         0
  Cache Read:     0
  Total:          0
----------------------------------------
Quality:
  Phases:           1/1
  Compile Attempts: 4 (across all phases)
  Debug Cycles:     1
  Compilation:      PASSED
  Functional Tests: PASSED (78/78)
  Tests Source:     GENERATED
  Prettified:       DISABLED
----------------------------------------
Per Phase:
  Phase 1 (exp2 core): compile_attempts=4, debug=1, test=passed
----------------------------------------
Failures: 1 (1 resolved, 0 unresolved)
  [test_failure] test_phase_1 (test_writer): All tests failed - kernel computes e^x instead of 2^x due to SFPMULI not working — RESOLVED
    Fix: Replaced SFPMULI with SFPLOADI+SFPMUL pattern, added SFPNOP for 2-cycle pipeline hazard
----------------------------------------
Artifacts:
  - codegen/artifacts/exp2_arch_research.md
  - codegen/artifacts/exp2_analysis.md
  - codegen/artifacts/exp2_phase1_spec.md
Metrics:
  - /proj_sw/user_dev/llk_code_gen/quasar/runs.jsonl
  - /proj_sw/user_dev/llk_code_gen/quasar/2026-03-30_exp2_quasar_e79aa3d1/
========================================
