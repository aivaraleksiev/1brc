# 1️⃣🐝🏎️ The One Billion Row Challenge

- **Challenge Blog Post**: [One Billion Row Challenge](https://www.morling.dev/blog/one-billion-row-challenge/)
- **Challenge Repository**: [gunnarmorling/1brc](https://github.com/gunnarmorling/1brc)

## Challenge Overview

The task is to perform basic floating-point arithmetic on 1 billion rows as quickly as possible, without relying on external dependencies.

## Approach

This solution prioritizes speed over safety and convenience, focusing on raw performance when processing large datasets. <br> _**Note that**_  this code is optimized for specific use cases and is not intended as a model for reusable or maintainable code.

### Key Techniques

- **Optimized CSV Parsing**: Fast reading of large CSV files.
- **Memory-Mapped Files**: Reduces I/O overhead by mapping files directly into memory.
- **Asynchronous Processing**: Splits the file into memory blocks, processed in parallel to maximize performance.
- **Custom Integer-Based Number Parser**: Avoids floating-point operations during parsing for enhanced speed.
- **Custom Hash Function**: Efficiently hashes short keys with minimal collision risk, leveraging knowledge of the fixed number of keys.
- **Specialized Hash Map**: Designed for scenarios with a predefined, small set of elements—perfect for handling fixed keys like city names.
- **Custom Iterator**: Provides STL-style iteration over the custom hash map for ease of use.

### Out of Scope (But Worth Mentioning)
While the spirit of the challenge is to rely strictly on your own code, a few techniques though outside the official scope are worth mentioning as potential optimizations:
- **Perfect Hashing for City Names**:
   - Since the list of city names is fixed and known ahead of time, a perfect hash function could be generated to provide constant-time lookups with no collisions. <br>
   - This approach isn't used here to stay within the challenge constraints, but it's an interesting idea to explore separately.

## Performance Benchmark
The original challenge was based on a machine with 32 threads.<br>
Benchmarks were run on i7-1185G7 CPU with 4 cores / 8 threads. 

| Rows       | 100 Million | 250 Million | 500 Million | 1 Billion |
|------------|-------------|-------------|-------------|-----------|
| Avg Time   | 0.8 s       | 2.0 s       | 4.25 s      | 12.9 s    |

## Build Instructions

To build and run the project, follow these steps:

1. **Install Dependencies**:
   ```bash
   vcpkg install boost:x64-windows
   ```
2. **Configure the Project**:
   - Set the C++ standard to C++20.
   - In Visual Studio 2022, choose `Release x64` as the solution platform.

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
   1br.exe "path_to_file"
   ```
