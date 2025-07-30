PR #323: docs: fix links in README
URL: https://github.com/tenstorrent/tt-llk/pull/323

File: README.md
@@ -10,8 +10,8 @@
 <br><br>
 **TT-LLK** is Tenstorrent's Low Level Kernel library.
 
-[![C++](https://img.shields.io/badge/C++-20-green.svg)](#)
-[![Python](https://img.shields.io/badge/python-3.8%20|%203.10-green.svg)](#)
+[![C++](https://img.shields.io/badge/C++-17-green.svg)](#)
+[![Python](https://img.shields.io/badge/python-3.10-green.svg)](#)
 </div>
 
 ## Overview ##
@@ -43,10 +43,10 @@ The following documentation is available to help you understand and use low-leve
 1. **[Intro](docs/llk/l1/intro.md)**
    A concise introduction to LLKs, designed for both technical and non-technical audiences. This document outlines the scope of the LLK software stack and its relationship to other Tenstorrent software components.
 
-2. **[Top-level Overview](docs/llk/l1/top_level_overview.md)**
+2. **[Top-level Overview](docs/llk/l2/top_level_overview.md)**
    Provides a high-level look at the Tensix Core and Tensix Engine architecture, including data organization for efficient LLK usage and operations supported by LLKs. This document is not tied to any specific chip generation (such as Wormhole) and is aimed at engineers and technical readers who want to understand the general architecture and capabilities.
 
-3. **[LLK Programming Model](docs/llk/l1/llk_programming_model.md)**
+3. **[LLK Programming Model](docs/llk/l3/programming_model.md)**
    This document dives into architectural details to best explain the usage of the LLK API. It is intended for op writers and advanced users, and connects LLK concepts with our runtime stack, [tt-metal](https://github.com/tenstorrent/tt-metal), providing practical guidance on how to leverage LLKs for efficient kernel development.
 
 ## Contributing ##

