# XV6-RISCV Enhanced Operating System

This project extends the XV6 operating system (a simple Unix-like teaching operating system developed at MIT) with advanced features and scheduling algorithms. The enhancements are implemented for the RISC-V architecture using ANSI C, making the system more feature-rich and efficient.

Please refer to a detailed technical report on implementations and performance analysis [here](https://github.com/ishwarbb/Enhancing-xv6/blob/main/Technical_Report.md)

## Features

### 1. System Call Enhancements

#### Process Tracing (`strace`)
- Implemented system call tracing functionality
- Usage: `strace <mask_no> <command>`
- Tracks specified system calls based on mask value
- Traces propagate to child processes
- Helps in debugging and system call analysis

#### Process Alarm System (`sigalarm` & `sigreturn`)
- Periodic process alarm functionality
- Allows processes to set up timer-based handlers
- Accurately maintains process state through `sigreturn`
- Useful for implementing periodic tasks and timeouts

### 2. Advanced Scheduling Algorithms

The system implements four different scheduling policies, selectable at compile time:

```bash
make clean qemu SCHED_FLAG=<scheduling_policy>
```

Available policies:

#### a. First Come First Served (FCFS)
- Non-preemptive scheduling based on process creation time
- Guarantees process execution order based on arrival
- Suitable for batch processing systems

#### b. Lottery Based Scheduling (LBS)
- Probabilistic scheduling using ticket system
- Processes can be assigned variable tickets via `settickets()`
- Provides proportional share scheduling
- Good for fair resource allocation with weighted priorities

#### c. Priority Based Scheduling (PBS)
- Dynamic priority calculation based on multiple factors
- Considers process runtime and sleep time
- Priority modification via `set_priority()`
- Ideal for systems requiring fine-grained process control

#### d. Multi-Level Feedback Queue (MLFQ)
- Five priority queues with dynamic process movement
- Implements aging to prevent starvation
- Automatic priority adjustment based on CPU usage
- Best for interactive and mixed workload environments

### 3. Copy-On-Write Fork Implementation

- Memory-efficient process creation
- Pages are shared between parent and child processes
- Physical copying deferred until write operations
- Significant memory savings for fork-heavy workloads

## Performance Analysis

Based on scheduler testing with modified parameters:

| Algorithm | Wait Time | Run Time | Total Time |
|-----------|-----------|----------|------------|
| FCFS      | 108       | 25       | 134        |
| LBS       | 111       | 24       | 135        |
| PBS       | 186       | 29       | 215        |
| MLFQ      | 103       | 26       | 128        |

Key findings:
- MLFQ shows best overall performance
- PBS shows higher latency due to non-preemptive implementation
- LBS and FCFS show similar performance characteristics
- MLFQ provides best balance between responsiveness and fairness

## Technical Implementation Details

- System calls implemented through careful modification of kernel structures
- Scheduling policies integrated into core kernel functionality
- Copy-on-Write fork uses page table manipulation and trap handling
- Memory management enhanced with reference counting
- Process control blocks extended to support new features

## Building and Running

1. Clone the repository:
```bash
git clone https://github.com/ishwarbb/Enhancing-xv6
cd Enhancing-xv6
```

2. Clean previous builds:
```bash
make clean
```

2. Build and run with desired scheduler:
```bash
make qemu SCHED_FLAG=<FCFS|LBS|PBS|MLFQ>
```

## Use Cases

- **FCFS**: Batch processing systems where order matters
- **LBS**: Fair resource sharing with weighted priorities
- **PBS**: Systems requiring manual priority control
- **MLFQ**: Interactive systems with mixed workloads
- **Copy-on-Write**: Systems with frequent process creation


### MLFQ analysis

Running MLFQ on the given  `schedulertest` with some modifications to avoid compiler optimisation

<img title="MLFQ analysis" src="./MLFQ.png">

This enhanced version of XV6 serves as both an educational tool and a demonstration of core operating system concepts, making it valuable for OS education and research purposes.