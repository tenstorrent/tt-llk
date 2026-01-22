Asserts in LLK
==============

## Purpose and scope
This document defines the mandatory rules and guidelines for using assertions in the LLK/Compute API codebase.

## Audience:
LLK developers, reviewers, and maintainers.

## Goal:
To enforce correctness, prevent invalid hardware configurations, and detect misuse as early as possible through compile-time and runtime assertions.

# What is an assert?

Asserts are **checks built into code** that verify whether a given condition holds true.

* **If the condition is true** → the program continues normally.
* **If the condition is false** → the program is terminated.

## Types of Asserts in LLK

1. **Compile-Time Assert (`static_assert`)**
   * Validates conditions during compilation.
   * Prevents invalid template instantiations or incorrect platform assumptions.
   * If a `static_assert` fails, the **build is broken** and compilation stops.
2. **Runtime Assert (`LLK_ASSERT`)**
   * Validates conditions during program execution.
   * If it fails, the TRISC that encounters it will execute an `ebreak` **instruction** and hang.
   * More details can be found here: [llk_asserts.rst#L4](https://github.com/tenstorrent/tt-metal/blob/dd2b14e643ca3a7a97f0c48a4ff76eadbd5f0a48/docs/source/tt-metalium/tools/llk_asserts.rst#L4)

# Assert role in LLK

## **Failure scenario handling:**

LLK does not support recoverable error handling (no exceptions, no error codes).

Therefore, **all invalid inputs, API misuse and violated invariants are treated as fatal programmer errors and must be guarded by asserts**. This is a deliberate design choice.

In LLK, assertions are used for:

1. Input sanitization at API boundaries
2. Enforcement of hardware and configuration invariants
3. Detection of internal logic bugs

## **Defensive Guidance**

Embedding assertions directly into the code provides **clear, enforceable guidance** on how the API is intended to be used. While comments can describe expected usage, they do not actively prevent or report violations — **assertions do**.

## **🔑** Assertion–Test Relationship

**Assertions**

* Act as guardrails within the codebase, ensuring that invalid configurations are blocked before they can execute.

**Tests**

* Validate that supported configurations behave as expected under real usage scenarios.

## **Rule for assert-test coverage**

Rule: Every parameter or operation must be protected by either an assertion or an automated test.

- **Assertions must** be used to enforce invariants at compile-time (`static_assert`) or at runtime (`LLK_ASSERT`) where appropriate.
- **Tests must** validate supported configurations and catch regressions; if an assertion is omitted for a case, a test covering that case must exist and the omission must be documented.

# Strategy for adding compile or runtime Asserts

1\. **Use Compile-Time Asserts when possible**

- Use **static\_assert** whenever the condition depends only on compile-time information (e.g., template parameters, type traits, constant expressions).
- Benefits: Faster feedback, easier debugging, prevents invalid builds.

2\. **Fallback to Runtime Asserts When Necessary**

- Use LLK\_ASSERT when conditions depend on runtime data (e.g., user input, file contents, dynamic state).
- Benefits: Covers scenarios not knowable at compile time.

## Examples: When to use `static_assert` vs `LLK_ASSERT`

### Example 1: Compile-Time Assert (Preferred when possible)

```cpp
template <uint32_t block_ct_dim>
inline void _llk_unpack_AB_reduce_block_max_row_mop_config_()
{
    // Constraint on the outerloop and innerloop dim
    static_assert(block_ct_dim < 128, "block_ct_dim must be less than 128");
    // ... rest of implementation ...
}
```

**Why:** The `block_ct_dim` is a template parameter known at instantiation time. Invalid values are rejected immediately during compilation, preventing any possibility of a bad binary being created.

### Example 2: Runtime Assert (When conditions depend on runtime values)

```cpp
template <PoolType type, ReduceDim dim>
inline void _llk_unpack_reduce_mop_config_(const std::uint32_t num_faces)
{
    LLK_ASSERT(num_faces == 1 || num_faces == 2 || num_faces == 4, "num_faces must be 1, 2, or 4");
    // ... rest of implementation ...
}
```

**Why:** The `num_faces` parameter is only known at runtime. An `LLK_ASSERT` catches invalid usage immediately at the point where it matters, with clear feedback.

# Strategy for adding compile or runtime parameters

As discussed, **compile-time asserts (`static_assert`) are preferable** because they provide immediate feedback, prevent invalid builds, and eliminate runtime overhead. However, they can only operate on **compile-time data**.

To maximize the use of `static_assert`, the logical approach is to **move as many parameters as possible into compile time**. Within the LLK/Compute API, this is often feasible and can strengthen invariants significantly.

The key question then becomes:

**Should every parameter be moved to compile time?**

## **🛠️ Compile-Time Parameters (Template Parameters)**

**Pros**

* ✅ **Early error detection**: Invalid values are caught at compile time, preventing bad builds.
* ✅ **Optimization opportunities**: Compiler can eliminate branches and generate specialized code.
* ✅ **Clearer invariants**: Constraints are enforced at the type system level.
* ✅ **No runtime overhead**: Checks happen during compilation, not execution.
* ✅ **Better debugging**: Errors surface immediately with compiler messages.

**Cons**

* ❌ **Code bloat**: Each distinct parameter value generates a separate instantiation, increasing binary size.
* ❌ **Reduced flexibility**: Only works with values known at compile time (e.g., template parameters, `constexpr`).
* ❌ **Longer compile times**: More instantiations can slow down builds.
* ❌ **Harder to generalize**: Not suitable for values that vary widely or depend on runtime input.

## **⚙️ Runtime Parameters (Function Arguments)**

**Pros**

* ✅ **Flexibility**: Can handle values determined at runtime (user input, configuration, external data).
* ✅ **Single compilation**: Function compiled once, reused for all values.
* ✅ **Simpler code reuse**: Avoids multiple template instantiations.
* ✅ **Better for dynamic scenarios**: Useful when valid values are not known until execution.

**Cons**

* ❌ **Delayed error detection**: Issues only surface when the program runs.
* ❌ **Runtime overhead**: Each check adds a small cost during execution.
* ❌ **Harder debugging**: Failures may depend on specific runtime paths or inputs.
* ❌ **Less optimization**: Compiler cannot specialize code based on parameter values.

## **📌** Strategy for compile/runtime parameters conclusion (Normative Rules):

**RULE: If a parameter influences control flow or validity and can be known at compile time, it must be a compile-time parameter unless there is a documented reason not to.**

* Use compile-time parameters when values are known during compilation and program logic branches based on them. This enables early validation and compiler optimizations.
* Document any exceptions (e.g., "We use a runtime parameter here for flexibility despite it being determinable at compile time because X").

**Use runtime parameters only when necessary**

* Choose runtime parameters when values are truly dynamic or not available until execution. This keeps the code adaptable to varying inputs.
* Document the reason if compile-time parameters would be impractical.

**Avoid raw** `int` **template parameters**

* If only a limited set of values is valid:
  * ✅ Define an `enum` to represent the allowed values, or
  * ✅ Use `static_assert` to explicitly restrict the set of acceptable values.

**Rationale for prioritizing compile-time parameters**

* Early error detection and clearer invariants at the type system level significantly improve code quality and reduce debugging time.

## **📌** Strategy for assert placement in a Call Stack

#### **1\. Low in the Stack (deep inside helper functions)**

* **Add asserts close to the point where invariants are actually required (e.g., assumptions about parameters, internal states).**
* **Ensures correctness directly at the source of the invariant.**

**Pros**

* **✅ Precise: Catches violations exactly where assumptions break.**
* **✅ Simplicity: Easier to identify the root cause.**

**Cons**

* **❌ Clutter risk: Can make internal logic noisy if overused.**

#### **2\. High in the Stack (entry points, public APIs)**

* **Add asserts at the boundaries of your system (e.g., validating inputs at API entry).**
* **Ensures external callers cannot pass invalid data deeper into the system.**

**Pros**

* **✅ Clear: Provides a clearer contract for API users.**
* **✅ Centralized: Protects the entire subsystem from invalid inputs.**

**Cons**

* **❌ Less precise: Failures may be reported far from the actual issue.**

## **📌** Strategy for assert placement in Call Stack conclusion (Rules):

### **Cover all call stacks**

* Begin by ensuring that every call stack through which the asserted variable can propagate within the LLK library is accounted for.

### **Identify the convergence point**

* Find the narrow point common to all relevant call stacks (whether higher or lower). Placing asserts at this convergence point typically provides full coverage with minimal duplication and helps ensure that future code paths remain protected automatically.
* **LLK-specific considerations:**
  * In LLK, the lowest point in the call stack often corresponds to setting a configuration or register value. This makes LLK particularly well-suited to a low-in-the-stack assertion strategy, as invariants can be enforced precisely where they matter most.
  * In cases where a single API governs specific call stacks (e.g., SFPU), it can be more effective to place asserts higher in the stack, at the API boundary.

### **Guideline - Balanced approach**

* Assertion placement should balance between low-level and high-level positions depending on the concrete case. In LLK, low-in-the-stack asserts are generally expected and favorable, but higher-level asserts may be appropriate when a single API naturally serves as the control point.

# What asserts are NOT for

**Asserts must not be used for:**

* **Recoverable errors** — Use error codes or alternative control flow if the program can continue.
* **User-facing validation** — Use dedicated validation layers and graceful error messages for user input.
* **Performance-critical hot paths (when asserts are enabled)** — Excessive asserts in tight loops can impact performance when enabled.

**Instead, asserts are for:**

* **Fatal programmer errors** — Conditions that indicate misuse of the API or internal bugs.
* **Invariant enforcement** — Preconditions, postconditions, and system invariants.
* **Input sanitization** — Since LLK has no other error handling mechanism, asserts are the **only available tool** for validating inputs and detecting invalid configurations.

**Key point:** Asserts are the sole mechanism in LLK for input sanitization. Code must not allow invalid configurations to silently execute or produce undefined behavior. Every unguarded assumption is a bug waiting to happen.

# 📌 Performance Overhead and Runtime Assert Policy

**Nature of runtime asserts**

* Runtime asserts introduce a **small performance overhead**, as they must evaluate conditions during program execution.

**Policy: Runtime asserts must always be written**

* **Developers must write `LLK_ASSERT` statements** even though they may be disabled at build time.
* **Code must not rely on undefined behavior when asserts are compiled out.** All invariants must hold logically, regardless of whether the assert fires.
* This ensures that enabling asserts on demand does not reveal latent bugs in the code.


**Current status in tt-metal**

* At present, `LLK_ASSERT`s are **disabled by default** in tt-metal release builds for performance.
* Asserts can be **enabled on demand** for development and debugging. Refer to [the tt-metal LLK debugging guide](https://github.com/tenstorrent/tt-metal/blob/main/docs/source/tt-metalium/tools/llk_asserts.rst) for details on enabling and using runtime asserts.

**When to enable asserts**

* During development and code review.
* When investigating bugs or unexpected behavior.
* In pre-release validation to catch edge cases.
* When asserts are enabled, the slight performance overhead is acceptable as a safeguard against incorrect hardware configurations.
