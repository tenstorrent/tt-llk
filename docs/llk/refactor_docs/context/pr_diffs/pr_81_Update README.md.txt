PR #81: Update README.md
URL: https://github.com/tenstorrent/tt-llk/pull/81

File: README.md
@@ -1,7 +1,26 @@
-# tt-llk #
-Tenstorrent low-level tensix kernels.
+# tt-llk: CPP Low Level Kernels (LLK) & test infrastructure #
 
-**Note:**
-Existing LLK repositories (https://github.com/tenstorrent/tt-llk-gs, https://github.com/tenstorrent/tt-llk-wh-b0, https://github.com/tenstorrent/tt-llk-bh) are merged here and will be archived soon.
+## Overview
+This repository contains low level kernels for Tenstorrent AI chips (Grayskull (deprecated), Wormhole_B0, and Blackhole), which represent foundational primitives of compute used as building blocks for higher level software stacks that implement ML OPs. Alongside the kernels is a test environment used for validating LLK APIs.
 
-Development will continue in this repository.
+LLKs are header-only.
+
+# Install
+1. Clone repo
+2. Set up test environment per https://github.com/tenstorrent/tt-llk/blob/main/tests/README.md
+
+# Software dependencies
+Test environment requires SFPI compiler for building, which is automatically ingested from https://github.com/tenstorrent/sfpi
+
+# Contributing
+1. Go over https://github.com/tenstorrent/tt-llk/blob/main/CONTRIBUTING.md
+2. Create a new branch.
+3. Make your changes and commit.
+4. Add new tests to cover your changes if needed and run existing ones.
+5. Start a pull request (PR).
+
+
+### Note
+Old LLK repositories (https://github.com/tenstorrent/tt-llk-gs, https://github.com/tenstorrent/tt-llk-wh-b0, https://github.com/tenstorrent/tt-llk-bh) are merged here and archived.
+
+Development continues in this repository.

