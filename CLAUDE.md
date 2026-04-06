# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build & Run

```bash
# Compile project.cpp (sections 1–7)
g++ -std=c++17 -Wall -Wextra -o project project.cpp

# Run (sections 1–6 run automatically; section 7 is interactive)
./project

# Run with piped input to drive the interactive section 7
echo -e "btn\nclick\nquit" | ./project

# Compile and run individual section files (sections 8–13)
g++ -std=c++17 -Wall -Wextra -o s08 s08_heap_stack.cpp && ./s08
g++ -std=c++17 -Wall -Wextra -o s09 s09_inheritance_vtable.cpp && ./s09
g++ -std=c++17 -Wall -Wextra -o s10 s10_templates_handle.cpp && ./s10
g++ -std=c++17 -Wall -Wextra -o s11 s11_smart_pointers.cpp && ./s11
g++ -std=c++17 -Wall -Wextra -o s12 s12_compiler_pipeline.cpp && ./s12
g++ -std=c++17 -Wall -Wextra -o s13 s13_chromium_convergence.cpp && ./s13
```

## Architecture

Each file is a self-contained, standalone C++ program teaching one competence needed to read Blink or V8 source code. Every section prints real addresses, real `sizeof` values, and real hex bytes — nothing hardcoded.

### project.cpp — sections 1–7 (CPU & memory fundamentals + Blink/V8 simulation)

| Section | Topic |
|---|---|
| 1 | Struct with two consecutive fields — alignment, byte layout |
| 2 | Struct with a method — vtable vs data separation |
| 3 | Array of structs — stride = `sizeof(Element)`, cache locality |
| 4 | Nested struct — padding, `offsetof`, multi-level address arithmetic |
| 5 | Pointer to struct — `->` dereference, aliasing |
| 6 | Reference to struct — reference as hidden pointer, same address |
| 7 | **Blink + V8 pipeline simulation** — DOM tree, event dispatch, wrapper map |

### Standalone section files — sections 8–13 (C++ competences for Chromium reading)

| File | Section | Competence | Chromium connection |
|---|---|---|---|
| `s08_heap_stack.cpp` | 08 | Heap vs Stack — allocate on heap and stack, explain address difference | Blink DOM nodes live on heap — they outlive the function that creates them |
| `s09_inheritance_vtable.cpp` | 09 | Inheritance + vtable — build class hierarchy, read vtable pointer at offset 0 | `BlinkNode` → `BlinkElement`, every DOM node's first 8 bytes are a vtable pointer |
| `s10_templates_handle.cpp` | 10 | Templates + `Handle<T>` — write `Handle<T>` wrapping a raw pointer | V8's `Local<T>` is this pattern — prevents GC from moving objects |
| `s11_smart_pointers.cpp` | 11 | Smart pointers — `unique_ptr`, `shared_ptr`, `use_count` | Blink's `scoped_refptr<T>` is a hand-rolled `shared_ptr` |
| `s12_compiler_pipeline.cpp` | 12 | Compiler pipeline — trace `int x = 1+1` from source to x86-64 bytes | What `g++` does before any byte reaches the CPU |
| `s13_chromium_convergence.cpp` | 13 | Chromium convergence — DOM tree with vtable + `Handle<T>` + ref counting + event dispatch | Complete Blink+V8 renderer pipeline in one file |

### Section 7 design (Blink + V8)

Section 7 simulates Chrome's renderer-process architecture:

- **`BlinkNode` / `BlinkElement`** — DOM tree with parent/children pointers and a `listeners` list; mirrors `blink::Node` / `blink::Element`. Virtual destructor puts vtable pointer at offset 0.
- **`V8Function`** — a named `std::function<void(BlinkEvent*)>` representing a JS callback.
- **`V8Wrapper`** — one wrapper per `BlinkNode`, lazily created on first JS access; mirrors the IDL-generated `v8::Object` subclass. Holds a back-pointer to the Blink node and a simulated V8-heap address.
- **`WrapperMap`** — singleton cache mapping `BlinkNode*` → `V8Wrapper*`; mirrors `blink::DOMDataStore`.
- **`BlinkEvent`** — carries `type`, `target`, `currentTarget`, `defaultPrevented`, `propagationStopped`.
- **`EventDispatcher`** — implements the DOM Events 3-phase algorithm: CAPTURE (root→target), AT-TARGET, BUBBLE (target→root). At each element it crosses the Blink→V8 boundary by calling the stored `V8Function::body`.

The interactive loop at the end of `section7()` accepts an element id and event type from stdin and runs the full dispatch pipeline, printing each boundary crossing.
