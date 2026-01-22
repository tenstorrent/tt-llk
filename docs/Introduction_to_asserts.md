Introduction to Asserts

# What is an assert?

Asserts are **checks built into code** that verify whether a given condition holds true.

* **If the condition is true** → the program continues normally.
* **If the condition is false** → the program is terminated.

## Types of Asserts

1. **Compile-Time Assert (`static_assert`)**
   * Validates conditions during compilation.
   * Prevents invalid template instantiations or incorrect platform assumptions.
   * If a `static_assert` fails, the **build is broken** and compilation stops.
2. **Runtime Assert (`LLK_ASSERT`)**
   * Validates conditions during program execution.
   * If it fails, the TRISC that encounters it will execute an `ebreak` **instruction** and hang.
   * More details can be found here: [https://github.com/tenstorrent/tt-metal/blob/dd2b14e643ca3a7a97f0c48a4ff76eadbd5f0a48/docs/source/tt-metalium/tools/llk_asserts.rst#L4](https://github.com/tenstorrent/tt-metal/blob/dd2b14e643ca3a7a97f0c48a4ff76eadbd5f0a48/docs/source/tt-metalium/tools/llk_asserts.rst#L4)

# When to use assert?

## **Failure scenario handling:**

In most systems, error handling and assertions serve different purposes:

* **Errors** are appropriate when an API user provides invalid input (e.g., unsupported parameters). The program must respond gracefully and continue operating where possible.
* **Assertions** are used when inputs are valid but the program enters an invalid state due to a bug in the logic or misuse of internal assumptions.

## **LLK Context**

Since LLK does not support traditional error handling, **asserts are used to catch both API misuse and internal bugs**.

## **Defensive Guidance**

Embedding assertions directly into the code provides **clear, enforceable guidance** on how the API is intended to be used. While comments can describe expected usage, they do not actively prevent or report violations — **assertions do**.

### **📌** Assertion–Test Relationship

**Assertions**

* Act as guardrails within the codebase, ensuring that invalid configurations are blocked before they can execute.

**Tests**

* Validate that supported configurations behave as expected under real usage scenarios.

### **🔑 Guiding Principle**

Every parameter or operation must be protected by **either an assertion or a test**.

* Assertions prevent misuse at **compile-time or runtime**.
* Tests detect issues through **automated validation** in realistic conditions.

### **🚀 Benefits**

By strengthening these safeguards:

* We reduce the number of reported issues.
* We minimize time spent debugging.
* We accelerate progress for both LLK developers and LLK users.

Robust assertions combined with thorough tests directly translate into **faster development cycles** and a **more reliable system**.

# Strategy for adding compile or runtime Asserts

1\. **Prefer Compile-Time Asserts First**

- Use static\_assert whenever the condition depends only on compile-time information (e.g., template parameters, type traits, constant expressions).
- Benefits: Faster feedback, easier debugging, prevents invalid builds.

2\. **Fallback to Runtime Asserts When Necessary**

- Use LLK\_ASSERT when conditions depend on runtime data (e.g., user input, file contents, dynamic state).
- Benefits: Covers scenarios not knowable at compile time.

3\. **Guideline**

- **Rule of thumb:** If it can be checked at compile time, enforce it there. Only use runtime asserts when compile-time enforcement is impossible.

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

### **📌** Strategy conclusion:

**Prefer compile-time parameters**

* Use compile-time parameters when values are known during compilation and program logic branches based on them. This enables early validation and compiler optimizations.

**Use runtime parameters when flexibility is needed**

* Choose runtime parameters when values are dynamic or not available until execution. This keeps the code adaptable to varying inputs.

**Avoid raw** `int` **template parameters**

* If only a limited set of values is valid:
  * ✅ Define an `enum` to represent the allowed values, or
  * ✅ Use `static_assert` to explicitly restrict the set of acceptable values.

**Balance trade-offs**

* Compile-time checks improve safety and performance, but runtime parameters keep code flexible and maintainable.

### **📌** Strategy for assert placement in a Call Stack

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

### **📌** Strategy conclusion:

**Cover all call stacks**

* Begin by ensuring that every call stack through which the asserted variable can propagate within the LLK library is accounted for.

**Identify the convergence point**

* Find the narrow point common to all relevant call stacks (whether higher or lower). Placing asserts at this convergence point typically provides full coverage with minimal duplication and helps ensure that future code paths remain protected automatically.
* **LLK-specific considerations:**
  * In LLK, the lowest point in the call stack often corresponds to setting a configuration or register value. This makes LLK particularly well-suited to a low-in-the-stack assertion strategy, as invariants can be enforced precisely where they matter most.
  * In cases where a single API governs specific call stacks (e.g., SFPU), it can be more effective to place asserts higher in the stack, at the API boundary.

**Balanced approach**

* Assertion placement should balance between low-level and high-level positions depending on the concrete case. In LLK, low-in-the-stack asserts are generally expected and favorable, but higher-level asserts may be appropriate when a single API naturally serves as the control point.

# 📌 Performance Overhead of Runtime Asserts

**Nature of runtime asserts**

* Runtime asserts introduce a **small performance overhead**, as they must evaluate conditions during program execution.

**Usage context**

* Because of this overhead, runtime asserts are typically treated as **internal defensive tools**. They can be toggled on or off depending on the situation — for example, often disabled during performance testing to avoid measurement bias.

**Current status in LLK**

* At present, `LLK_ASSERT`s are **disabled in tt-metal**.

**Recommended direction**

* The goal should be to **enable and adopt runtime asserts as a standard defensive mechanism**, ensuring stronger runtime validation while still maintaining the flexibility to disable them when conducting performance-sensitive benchmarks.
