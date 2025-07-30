# Refactoring Guidelines for C++ Kernel Code

> **Goal**: Improve code **readability** and **maintainability** without changing the kernel’s **functionality** or **hardware-specific configurations**.

---

## ✅ Guiding Principles

1. **Preserve Behavior**
   - Functional parity with the original code is **mandatory**.
   - Do not alter hardware interaction logic or platform-specific implementations.

2. **Enhance Readability**
   - Use expressive naming and clear structure.
   - Minimize deep nesting and complex one-liners.
   - Favor clarity over micro-optimizations unless proven critical.

3. **Respect Constraints**
   - Keep all existing hardware configuration macros, flags, and memory mappings intact.
   - Avoid introducing external dependencies.

---

## 🧹 Refactoring Checklist

### 🔤 Naming

- Replace cryptic or single-letter identifiers (e.g., `a`, `tmp`, `x1`) with meaningful names.
- Use consistent naming conventions (`snake_case` or `camelCase`) as per the codebase.

### 🧱 Structure

- Break large functions (>40 lines) into smaller, self-contained units.
- Extract repeated code into helper functions.
- Flatten deep nested conditionals using early returns or guards.

### 📚 Comments & Documentation

- Add comments to describe hardware interactions, register usage, and assumptions.
- Remove outdated or redundant comments.
- Use Doxygen-style headers for functions and modules, if applicable.

### 🧪 Testing and Verification

- After each change, run:
  - Existing unit tests (if any).
  - Platform-specific integration or smoke tests.
  - Manual validation for critical I/O routines or boot sequences.

---

## 🚫 What NOT to Do

- ❌ Don’t replace low-level register access macros unless you fully understand their side effects.
- ❌ Don’t change initialization sequences without confirmation from hardware spec/docs.
- ❌ Don’t introduce exceptions or STL constructs that may not be kernel-safe.
- ❌ Don’t reformat code in a way that breaks diff/line tracking unnecessarily.

---

## 🛠 Example Refactor

### Before
```cpp
int init() {
    int a = hw_check();
    if (a == 0) {
        return -1;
    } else {
        do_init_sequence();
    }
    return 0;
}
