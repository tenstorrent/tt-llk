========================================
  LLK CodeGen — Generation Complete
========================================
Prompt:           Generate fill for Quasar
Kernel:           fill
Kernel Type:      sfpu
Target Arch:      quasar
Reference:        tt_llk_blackhole/common/inc/sfpu/ckernel_sfpu_fill.h
Generated File:   tt_llk_quasar/common/inc/sfpu/ckernel_sfpu_fill.h
Lines Generated:  79
----------------------------------------
Timing:
  Start:          2026-03-30T13:15:31Z
  End:            2026-03-30T14:35:23Z
----------------------------------------
Tokens:
  Input:          0
  Output:         0
  Cache Read:     0
  Total:          0
----------------------------------------
Quality:
  Phases:           3/3
  Compile Attempts: 4 (across all phases)
  Debug Cycles:     1
  Compilation:      PASSED
  Functional Tests: PASSED (78/78)
  Tests Source:     GENERATED
  Prettified:       DISABLED
----------------------------------------
Per Phase:
  Phase 1 (float_fill): compile_attempts=1, debug=0, test=passed
  Phase 2 (int_fill): compile_attempts=2, debug=1, test=passed
  Phase 3 (bitcast_fill): compile_attempts=1, debug=0, test=passed
----------------------------------------
Failures: 1 (1 resolved, 0 unresolved)
  [compile_error] test_phase_2 (tester): TTI_SFPSTORE requires compile-time constant for store_mode — RESOLVED
----------------------------------------
Artifacts:
  - codegen/artifacts/fill_arch_research.md
  - codegen/artifacts/fill_analysis.md
  - codegen/artifacts/fill_phase1_spec.md
  - codegen/artifacts/fill_phase2_spec.md
  - codegen/artifacts/fill_phase3_spec.md
Metrics:
  - /proj_sw/user_dev/llk_code_gen/quasar/runs.jsonl
  - /proj_sw/user_dev/llk_code_gen/quasar/2026-03-30_fill_quasar_fc62fc04/
========================================
