# README: Constant Propagation LLVM Pass

## Overview

This repository contains two implementations of **Constant Propagation** as LLVM passes:

1. **Iterative Constant Propagation**
2. **SSA-Based Constant Propagation**

Both approaches are optimization techniques that simplify the Intermediate Representation (IR) by replacing variables or computations with their constant values whenever possible. This reduces redundant instructions and enhances the efficiency of the generated code.

---

## Key Features

### Iterative Constant Propagation

- **Worklist Algorithm**: Iteratively propagates constants across basic blocks using a fixed-point computation.
- **Control Flow Support**: Handles branching and control flow structures effectively.
- **Memory Operations**: Supports `load` and `store` instructions for constant values.

### SSA-Based Constant Propagation

- **SSA Form Compliance**: Leverages SSA (Static Single Assignment) form for efficient propagation.
- **PHI Node Handling**: Resolves constants through PHI nodes in SSA form.
- **Binary Operations**: Optimizes arithmetic instructions (e.g., addition, subtraction, multiplication).

---

## Implementation Details

### Iterative Constant Propagation

#### Data Structures

1. **IN and OUT Maps**: Track constant values for variables at the entry and exit points of each basic block.
2. **Instruction-to-Line Mapping**: Maps instructions to their line numbers for debugging and tracking.
3. **Worklist Queue**: Manages basic blocks to process iteratively.

#### Algorithm

1. **Initialization**:
   - Variables are initialized to `INT_MAX` (unknown values).
   - The entry block's `IN` map is set to `INT_MIN` (constant).
2. **Meet Operation**:
   - Merges constants from predecessor blocks.
3. **Transfer Function**:
   - Updates constant values based on instructions:
     - `store`: Updates memory with constant values.
     - `load`: Retrieves constants from memory.
     - Binary operations: Computes constant results where possible.
4. **Iteration**:
   - Repeats until the `OUT` map stabilizes.

### SSA-Based Constant Propagation

#### Data Structures

1. **Worklist Queues**:
   - **Flow Worklist**: Tracks control flow edges.
   - **SSA Worklist**: Tracks SSA dependency edges.
2. **Constant Value Map**: Tracks constant values for each instruction.

#### Algorithm

1. **Initialization**:
   - All variables are initialized to `INT_MAX` (unknown values).
2. **PHI Node Processing**:
   - Resolves constants by evaluating PHI node operands.
3. **Binary Operations**:
   - Computes constant results for arithmetic operations.
4. **Branch Simplification**:
   - Simplifies branches by resolving constants in comparison instructions.
5. **Instruction Replacement**:
   - Replaces instructions with constants and removes redundant instructions.

---

## Comparison: SSA-Based vs. Iterative Constant Propagation

| Feature                      | SSA-Based Algorithm                                   | Iterative Algorithm                                   |
|------------------------------|------------------------------------------------------|-----------------------------------------------------|
| **Time Complexity**          | \(O(N * V)\), where \(N\) = statements, \(V\) = variables | \(O(N^2 * V)\), due to def-use chains          |
| **Handling PHI Nodes**       | Efficiently handles PHI nodes in SSA form            | More complex due to def-use analysis                |
| **Dependence Tracking**      | Relies on SSA edges (direct dependencies)            | Relies on def-use chains                            |
| **Redundant Computations**   | Reduced through direct SSA edge traversal            | Higher due to complex def-use chains                |
| **Suitability**              | Ideal for SSA-based IR with many variables           | Better suited for simpler CFGs without SSA          |

---

## How to Use

### Compilation

To compile either pass:

```bash
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make
```

### Running the Pass

1. Compile the input code to LLVM IR:

   ```bash
   clang -S -emit-llvm input.c -o input.ll
   ```

2. Run the pass using `opt`:

   ```bash
   # Iterative Constant Propagation
   opt -load ./libConstantPropagation.so -ConstantPropagation < input.ll > output.ll

   # SSA-Based Constant Propagation
   opt -load ./libSSAConstantPropagation.so -SSAConstantPropagation < input.ll > output.ll
   ```

3. Inspect the optimized LLVM IR:

   ```bash
   cat output.ll
   ```

---

## Examples

### Iterative Constant Propagation

#### Input IR (Before Pass):

```llvm
%1 = add i32 2, 3
store i32 %1, i32* %ptr
%2 = load i32, i32* %ptr
%3 = add i32 %2, 4
ret i32 %3
```

#### Output IR (After Pass):

```llvm
store i32 5, i32* %ptr
ret i32 9
```

### SSA-Based Constant Propagation

#### Input IR (Before Pass):

```llvm
%1 = add i32 2, 3
%2 = phi i32 [ %1, %entry ], [ 4, %other ]
%3 = add i32 %2, 5
ret i32 %3
```

#### Output IR (After Pass):

```llvm
ret i32 10
```

---

## Limitations

- **Dynamic Values**: Neither pass handles runtime-determined values.
- **Side Effects**: Assumes no side effects in external function calls or memory operations.
- **Complex Control Flow**: Optimization may be limited in deeply nested or complex structures.

---

## Contribution

Feel free to contribute improvements or report issues by creating a pull request or opening an issue in the project repository.

---

## License

MIT License

Copyright (c) [2025] [Rohan Bennur]

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

