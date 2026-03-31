========================================
  LLK CodeGen — Generation Complete
========================================
Prompt:           Generate log for Quasar
Kernel:           log
Kernel Type:      sfpu
Target Arch:      quasar
Reference:        tt_llk_blackhole/common/inc/sfpu/ckernel_sfpu_log.h
Generated File:   tt_llk_quasar/common/inc/sfpu/ckernel_sfpu_log.h
Lines Generated:  94
----------------------------------------
Timing:
  Start:          2026-03-30T17:43:32Z
  End:            2026-03-30T19:18:57Z
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
  Functional Tests: PASSED (114/114)
  Tests Source:     GENERATED
  Prettified:       DISABLED
----------------------------------------
Per Phase:
  Phase 1 (log): compile_attempts=4, debug=1, test=passed
----------------------------------------
Failures: 2 (2 resolved, 0 unresolved)
  [test_failure] test_phase_1 (tester): CC corruption: SFPIADD mod=0 overwrites CC, SFPSETSGN gated off for negative exponents — RESOLVED
  [test_failure] test_phase_1 (tester): MxFp8P input golden mismatch: quantization error amplified by log exceeds tolerance — RESOLVED
----------------------------------------
Artifacts:
  - codegen/artifacts/log_arch_research.md
  - codegen/artifacts/log_analysis.md
  - codegen/artifacts/log_phase1_spec.md
Metrics:
  - /proj_sw/user_dev/llk_code_gen/quasar/runs.jsonl
  - /proj_sw/user_dev/llk_code_gen/quasar/2026-03-30_log_quasar_d635d457/
========================================
