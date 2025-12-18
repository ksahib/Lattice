Distributed Compiler-Driven Computing Platform


This project presents a compiler-assisted, hardware-architecture-agnostic distributed computing platform for C++ programs. 
Instead of requiring developers to explicitly write parallel or distributed code, the system analyzes user-submitted C++ programs at the compiler level, identifies safe parallel regions, and distributes execution across multiple worker nodes using a master–worker model.

The project is designed as a proof of concept, demonstrating the feasibility of combining LLVM-based program analysis with distributed execution. The focus is on correctness, architectural design, and end-to-end integration—not on performance optimization.

Key Features:

1. Architecture agnostic execution
Workers can run on heterogeneous hardware without source-level changes.

2. Compiler-level parallelization
Parallel regions are identified using LLVM Intermediate Representation (IR) and program dependence analysis.

3. IR-level loop outlining
Parallelizable loops are outlined at the IR level, preserving correctness.

4. State capture and reconstruction
Program state is captured into a serialized environment structure with metadata-driven reconstruction on workers.

5. Master–worker orchestration
A centralized master distributes tasks to registered workers using gRPC.

6. Minimal user effort
Users upload standard C++ source code; no parallel programming model is required.

System Architecture:

User (Flutter UI)
        |
        v
Master Server (Drogon)
  - C++ → LLVM IR
  - Program Dependence Graph (PDG)
  - Loop outlining
  - Environment serialization
        |
        v
 gRPC Communication
        |
        v
Worker Nodes
  - Receive LLVM IR + environment
  - Reconstruct state
  - Execute outlined code
  - Return results


Technology Stack:

LLVM
Used for IR generation, dependence analysis, and IR-level transformations.

Drogon (C++)
Backend framework for the master server.

gRPC
Communication layer between master and worker nodes.

Flutter
Front-end interface for code submission and job management.
