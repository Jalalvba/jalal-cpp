#include <iostream>
#include <cstdint>   // uintptr_t

// ─────────────────────────────────────────────────────────────────────────────
// Section 08 — Heap vs Stack
//
// Competence: given two addresses, identify which is stack and which is heap.
//
// On Linux x86-64 the virtual address space is laid out roughly like this:
//
//   0xFFFF'FFFF'FFFF'FFFF  ← kernel
//   ────────────────────────
//   0x7FFF'FFFF'FFFF       ← top of user stack (grows DOWN ↓)
//       stack frames
//   ────────────────────────
//          ... gap ...
//   ────────────────────────
//       heap               ← grows UP ↑  (new / malloc)
//   0x0000'0055'xxxx'xxxx  ← typical heap base with ASLR
//   ────────────────────────
//   text / data / BSS      ← code, globals, statics
//   0x0000'0000'0000'0000
//
// Rule of thumb: stack addresses start with 0x7f...  heap addresses start
// with 0x55... or 0x7f... but are LOWER than the stack pointer.
//
// Chromium note: Blink allocates DOM nodes with `new` because nodes must
// outlive the function that created them.  A stack-allocated node would be
// destroyed when the function returns, leaving dangling pointers in the tree.
// ─────────────────────────────────────────────────────────────────────────────

struct Point {
    int x;
    int y;

    Point(int x_, int y_) : x(x_), y(y_) {
        std::cout << "    Point(" << x << ", " << y << ")  constructed  @ "
                  << static_cast<void*>(this) << "\n";
    }

    ~Point() {
        std::cout << "    ~Point(" << x << ", " << y << ") destroyed     @ "
                  << static_cast<void*>(this) << "\n";
    }
};

int main() {
    std::cout << "══════════════════════════════════════════════\n";
    std::cout << "Section 08 — Heap vs Stack\n";
    std::cout << "══════════════════════════════════════════════\n";

    // ── 1. Stack allocation ───────────────────────────────────────────────────
    // `stack_pt` lives inside this stack frame.  When main() returns the frame
    // is popped and the object is gone — no explicit action needed (or possible).
    std::cout << "\n── 1. Stack allocation ──\n";
    std::cout << "  Point stack_pt(1, 2);\n";
    Point stack_pt(1, 2);

    uintptr_t stack_addr = reinterpret_cast<uintptr_t>(&stack_pt);
    std::cout << "  address : " << static_cast<void*>(&stack_pt) << "\n";
    std::cout << "  region  : STACK (address 0x"
              << std::hex << stack_addr << std::dec
              << " — near 0x7fff..., high end of user space)\n";

    // ── 2. Heap allocation ────────────────────────────────────────────────────
    // `new` asks the OS / allocator for memory from the heap.  The returned
    // pointer is the only way to reach that memory; we must keep it and call
    // delete when we're done.
    std::cout << "\n── 2. Heap allocation ──\n";
    std::cout << "  Point* heap_pt = new Point(3, 4);\n";
    Point* heap_pt = new Point(3, 4);

    uintptr_t heap_addr = reinterpret_cast<uintptr_t>(heap_pt);
    std::cout << "  address : " << static_cast<void*>(heap_pt) << "\n";
    std::cout << "  region  : HEAP  (address 0x"
              << std::hex << heap_addr << std::dec
              << " — lower in address space than the stack)\n";

    // ── 3. Compare addresses ──────────────────────────────────────────────────
    std::cout << "\n── 3. Address comparison ──\n";
    std::cout << "  stack_pt  @ " << static_cast<void*>(&stack_pt) << "\n";
    std::cout << "  heap_pt   @ " << static_cast<void*>(heap_pt)   << "\n";
    std::cout << "  stack > heap? "
              << (stack_addr > heap_addr ? "YES — stack is higher in memory" : "NO")
              << "\n";

    // ── 4. sizeof(pointer) is always 8 ───────────────────────────────────────
    // A pointer is just an address — 64 bits on a 64-bit CPU regardless of
    // what it points to.  sizeof(Point) is the size of the object; sizeof(Point*)
    // is always 8.
    std::cout << "\n── 4. sizeof ──\n";
    std::cout << "  sizeof(Point)    = " << sizeof(Point)
              << " bytes  (two ints: x[4] + y[4])\n";
    std::cout << "  sizeof(Point*)   = " << sizeof(Point*)
              << " bytes  (a pointer — always 8 on 64-bit, regardless of pointee)\n";
    std::cout << "  sizeof(int*)     = " << sizeof(int*)     << " bytes\n";
    std::cout << "  sizeof(double*)  = " << sizeof(double*)  << " bytes\n";
    std::cout << "  sizeof(void*)    = " << sizeof(void*)    << " bytes\n";

    // ── 5. delete fires the destructor immediately ────────────────────────────
    // delete does two things in order:
    //   a) calls the destructor   (~Point runs right now)
    //   b) returns memory to the heap allocator (address may be reused later)
    // After delete the pointer value is stale — reading it is undefined behaviour.
    std::cout << "\n── 5. Explicit delete — destructor fires immediately ──\n";
    std::cout << "  delete heap_pt;   ← calling this now\n";
    delete heap_pt;
    heap_pt = nullptr;   // good practice: null the pointer after delete
    std::cout << "  (destructor fired at the delete line above)\n";

    // ── 6. Memory leak — no delete ────────────────────────────────────────────
    // The pointer `leaked` goes out of scope at the closing brace.  Because it
    // is a raw pointer, C++ does NOT call the destructor or free the memory.
    // The heap block is permanently inaccessible for the lifetime of the process.
    //
    // Chromium guards against this with scoped_refptr<T> (section 13) — a
    // smart pointer that calls Release() automatically when it goes out of scope.
    std::cout << "\n── 6. Memory leak — going out of scope without delete ──\n";
    {
        std::cout << "  entering inner block\n";
        std::cout << "  Point* leaked = new Point(5, 6);\n";
        Point* leaked = new Point(5, 6);
        std::cout << "  leaked @ " << static_cast<void*>(leaked) << "\n";
        std::cout << "  leaving inner block WITHOUT delete...\n";
        // `leaked` goes out of scope here.  No destructor.  No free.
    }
    std::cout << "  back in outer block — ~Point was NOT called above\n";
    std::cout << "  the heap memory at that address is now leaked (unreachable)\n";

    // ── 7. Stack object destructs automatically at scope end ──────────────────
    // `stack_pt` was created at the top of main().  Its destructor runs when
    // main() returns — the compiler inserts the call automatically.
    std::cout << "\n── 7. Stack object destructs automatically at scope end ──\n";
    std::cout << "  main() is about to return; stack_pt destructor will fire:\n";

    return 0;
    // ~Point(1, 2) fires here for stack_pt
}
