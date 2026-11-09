# 1️⃣🐝🏎️ The One Billion Row Challenge

- **Challenge Blog Post**: [One Billion Row Challenge](https://www.morling.dev/blog/one-billion-row-challenge/)
- **Challenge Repository**: [gunnarmorling/1brc](https://github.com/gunnarmorling/1brc)

## Challenge Overview

The task is to perform basic floating-point arithmetic on 1 billion rows as quickly as possible.

## Approach

This solution prioritizes speed over safety and convenience, focusing on raw performance when processing large datasets. <br> _**Note that**_  this code is optimized for specific use cases and is not intended as a model for reusable or maintainable code.

### Key Techniques

- **Optimized CSV Parsing**: Fast reading of large CSV files.
- **Memory-Mapped Files**: Reduces I/O overhead by mapping files directly into memory.
- **Parallel Processing with Dynamic Work Distribution:**: Splits the file into fixed-size chunks (16 MiB by default) and processes them in parallel to maximize performance. Optimized for hybrid CPUs (P/E/LP-E cores), allowing faster cores to process more chunks instead of waiting for slower cores to finish.
- **Custom Integer-Based Number Parser**: Avoids floating-point operations during parsing for enhanced speed.
- **Custom Hash Function**: Efficiently hashes short keys with minimal collision risk, leveraging knowledge of the fixed number of keys.
- **Specialized Hash Map**: Designed for scenarios with a predefined, small set of elements—perfect for handling fixed keys like city names.
- **Custom Iterator**: Provides STL-style iteration over the custom hash map for ease of use.
- **Compiler Optimization Hints**: Functions on the critical path are force-inlined and marked as hot to guide aggressive compiler optimizations and improve instruction cache locality.

### Out of Scope (But Worth Mentioning)
While the spirit of the challenge is to rely strictly on your own code, a few techniques though outside the official scope are worth mentioning as potential optimizations:
- **Perfect Hashing for City Names**:
   - Since the list of city names is fixed and known ahead of time, a perfect hash function could be generated to provide constant-time lookups with no collisions. <br>
   - This approach isn't used here to stay within the challenge constraints, but it's an interesting idea to explore separately.
- **SPDK (Storage Performance Development Kit)**:
   - Instead of memory-mapped files, SPDK bypasses the kernel I/O stack and could theoretically enable even faster data ingestion for NVMe storage.
   - It's overkill for a file-based benchmark like this and well outside the challenge’s intended scope.
   - **`Boost Asio` + `io_uring`**
- **SIMD / SWAR Parsing with CPU-Specific Compiler Flags**:
   - Instead of scanning each city name byte by byte, 8 bytes (SWAR, using plain 64-bit integer operations) or 32 bytes (SIMD, e.g. AVX2) can be checked at once to find the `;` and `\n` separators. The hash can then be computed from whole 64-bit words instead of one character at a time.
   - This requires building for a specific instruction set, e.g. `-march=x86-64-v3` (AVX2, BMI1/BMI2), so that `std::countr_zero` compiles to a single `tzcnt` instruction and AVX2 intrinsics are available. The resulting binary only runs on CPUs that support these instructions.
   - Not used here to keep the code portable and simple, but it is one of the largest single-thread optimizations available.

##  Performance Benchmark

- Original challenge was benchmarked on a machine with **32 threads**.
- These benchmarks were run on:
  - AMD Ryzen AI 7 350 (4P cores + 4E cores = 16 threads)


| CPU / Threads                   | 100M Rows | 250M Rows | 500M Rows | 1B Rows |
|---------------------------------|-----------|-----------|-----------|---------|
| AMD Ryzen AI 7 350 (16 threads) | 0.20 s    | 0.49 s    | 1.00 s    | 1.90 s  |

## Build Instructions

Tested on Windows with xmake and clang-cl. Linux and macOS builds are configured but untested.

1. **Prerequisites**:
- xmake
- LLVM/Clang with C++23 support

2. **Build the Project**
```bash
xmake               # Default build configuration
xmake f -m debug    # Set Build mode (debug, release, releasedbg)   
```

3. **Generate Sample CSV Files**:
- Install Python 3.
- Install required packages:
  ```bash
  pip install numpy pandas polars tqdm
  ```
- Create CSV files:
  ```bash
  python createMeasurements.py
  python createMeasurements.py -r NumberOfRecords
  ```

4. **Run the Program**:
```bash
xmake run 1brc "absolute_path_to_file"   # Run with an input file
```
