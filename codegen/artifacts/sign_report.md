========================================
  LLK CodeGen — Generation Complete
========================================
Prompt:           Generate sign for Quasar
Kernel:           sign
Kernel Type:      sfpu
Target Arch:      quasar
Reference:        tt_llk_blackhole/common/inc/sfpu/ckernel_sfpu_sign.h
Generated File:   tt_llk_quasar/common/inc/sfpu/ckernel_sfpu_sign.h
Lines Generated:  67
----------------------------------------
Timing:
  Start:          2026-03-30T22:56:27Z
  End:            2026-03-30T23:33:50Z
----------------------------------------
Tokens:
  Input:          0
  Output:         0
  Cache Read:     0
  Total:          0
----------------------------------------
Quality:
  Phases:           1/1
  Compile Attempts: 2 (across all phases)
  Debug Cycles:     0
  Compilation:      PASSED
  Functional Tests: PASSED (78/78)
  Tests Source:     GENERATED
  Prettified:       DISABLED
----------------------------------------
Per Phase:
  Phase 1 (sign (complete kernel)): compile_attempts=2, debug=0, test=passed
----------------------------------------
Failures: 1 (1 resolved, 0 unresolved)
  [compile_error] compile_phase_1 (writer): compile checker requires template<bool APPROXIMATION_MODE> and _init_sign_() — RESOLVED
----------------------------------------
Artifacts:
  - codegen/artifacts/sign_arch_research.md
  - codegen/artifacts/sign_analysis.md
  - codegen/artifacts/sign_phase1_spec.md
Metrics:
  - /proj_sw/user_dev/llk_code_gen/quasar/runs.jsonl
  - /proj_sw/user_dev/llk_code_gen/quasar/2026-03-30_sign_quasar_912b17af/
========================================
