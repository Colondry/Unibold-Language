# UniBold Language

**UniBold** is a symbol-heavy, low-level esoteric programming language designed to transpile directly to clean ISO C (`out.c`). It features configurable memory backends (Stack vs. Dynamic Heap), mode-based token interpretation, and built-in hardware profiling.

---

## Key Features

* **ISO C Code Generation**: Compiles down to ANSI/ISO C using standard libraries (`stdio.h`, `stdint.h`, `stdlib.h`).
* **Configurable Memory Architecture**: Choose between stack-allocated fixed arrays and dynamically allocated heap memory.
* **Mode-Switching Engine**: Directs input/output and tape operations via explicit hexadecimal hardware modes (`$<0x...>`).
* **Real-time Performance Profiling**: Monitor process RAM (Min/Avg/Peak) and total CPU execution time on Windows using `-wdebug`.

---

## Memory Setup Directives

UniBold requires declaring a memory model at the top of the program before executing tape commands.

| Syntax | Memory Region | C Code Output (`out.c`) |
| :--- | :--- | :--- |
| `!s<30000>` | **Stack Memory** | `int64_t tape[30000] = {0};` |
| `!h<50000>` | **Heap Memory** | `int64_t *tape = (int64_t*)calloc(50000, sizeof(int64_t));` |

---

## Mode Control Commands

UniBold switches execution behavior using mode signatures:

| Mode Directive | Name | Purpose |
| :--- | :--- | :--- |
| `$<0x#>` | **Tape Mode** | Enables data assignment (`$~`), cell manipulation (`$X`), and pointer movement (`$>`). |
| `$<0x01>` | **Output Mode** | Enables string literal printing (`$"..."`), cell printing (`$#`), and newlines (`$N`). |
| `$<0x02>` | **Input Mode** | Prompts user input and writes result directly into `tape[ptr]`. |
| `$<0x00>` | **Exit (0)** | Cleanly exits with return code `0`. |
| `$<0xF>` | **Exit (15)** | Exits with error/custom return code `15`. |

---

## Syntax Reference

### Data Assignment & Pointer Manipulation (Mode 1)

```text
$<0x#>          ; Switch to Tape Mode
$~'H'           ; Insert character ASCII value into tape[ptr]
$~105~          ; Insert integer value 105 into tape[ptr]
$>              ; Shift pointer right by 1 cell (ptr += 1)
$>>>>            ; Shift pointer right by 4 cells (ptr += 4)
$X              ; Clear current cell (tape[ptr] = 0)
```

### Console I/O (Modes 01 & 02)

```text
$<0x01>         ; Switch to Output Mode
$"Hello World"  ; Print raw string literal
$N              ; Print newline (\n)
$#'             ; Print tape[ptr] as an ASCII character
$#              ; Print tape[ptr] as a 64-bit integer

$<0x02>         ; Switch to Input Mode
$"Enter number: "; Display prompt and store user input in tape[ptr]

```

### Control Flow & Loops

```text
![              ; Begin infinite loop (while (1))
    $*br        ; Break loop
    $*con       ; Continue loop
]               ; End loop

?'A'[           ; Conditional: execute block if tape[ptr] == 'A'
    $X          ; Clear cell
]               ; End conditional

```

---

## Code Examples

### 1. Hello World (Character-by-Character)

```text
!s<30000> ; Allocate stack memory tape

$<0x#> $~'H' $<0x01> $#' $X $>
$<0x#> $~'e' $<0x01> $#' $X $>
$<0x#> $~'l' $<0x01> $#' $X $>
$<0x#> $~'l' $<0x01> $#' $X $>
$<0x#> $~'o' $<0x01> $#' $X $>
$<0x#> $~',' $<0x01> $#' $X $>
$<0x#> $~' ' $<0x01> $#' $X $>
$<0x#> $~'W' $<0x01> $#' $X $>
$<0x#> $~'o' $<0x01> $#' $X $>
$<0x#> $~'r' $<0x01> $#' $X $>
$<0x#> $~'l' $<0x01> $#' $X $>
$<0x#> $~'d' $<0x01> $#' $X $>
$<0x#> $~'!' $<0x01> $#' $X $N

$<0x00> ; Exit program

```

### 2. High-Level String Printing & Heap Memory

```text
!h<10000> ; Allocate heap memory tape

$<0x01> $"Welcome to UniBold Esoteric Language!" $N
$<0x02> $"Enter your age: "
$<0x01> $"Your age is: " $# $N

$<0x00> ; Clean exit & free heap memory

```

---

## Building and Running the Transpiler

### Compilation

Build the C++ transpiler using GCC or MSVC:

```bash
g++ -std=c++17 -O2 main.cpp -o unibold.exe -lpsapi

```

### Execution Commands

Run a UniBold source file:

```cmd
unibold.exe main.ub

```

Enable performance profiling (RAM/CPU metrics):

```cmd
unibold.exe main.ub -wdebug

```

### Output Report Example (`-wdebug`)

```text
Successfully generated out.c!
Hello World!

--- Resource Report ---
RAM Peak : 1.42 MB
RAM Avg  : 1.18 MB
RAM Min  : 0.95 MB
Total CPU Time Spent: 0.002100 seconds

```

---

## Transpiler Pipeline Architecture

```text
  [ Source File (.ub) ]
            │
            ▼
          Lexer     --> Tokenizes symbols & modes
            │
            ▼
          Parser    --> Builds unified Node AST
            │
            ▼
       Code Emitter --> Generates clean C source (out.c)
            │
            ▼
   [ GCC Compilation ]  --> Emits native out.exe

```
