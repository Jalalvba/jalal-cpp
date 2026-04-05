# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build & Run

```bash
# Compile
g++ -std=c++17 -Wall -Wextra -o project project.cpp

# Run (sections 1–6 run automatically; section 7 is interactive)
./project

# Run with piped input to drive the interactive section 7
echo -e "btn\nclick\nquit" | ./project
```

## Architecture

Single-file C++ project (`project.cpp`) structured as self-contained numbered sections, each teaching one memory-layout or systems concept. All sections are called in sequence from `main()`.

### Section layout

| Section | Topic |
|---|---|
| 1 | Struct with two consecutive fields — alignment, byte layout |
| 2 | Struct with a method — vtable vs data separation |
| 3 | Array of structs — stride = `sizeof(Element)`, cache locality |
| 4 | Nested struct — padding, `offsetof`, multi-level address arithmetic |
| 5 | Pointer to struct — `->` dereference, aliasing |
| 6 | Reference to struct — reference as hidden pointer, same address |
| 7 | **Blink + V8 pipeline simulation** — DOM tree, event dispatch, wrapper map |

### Section 7 design (Blink + V8)

Section 7 simulates Chrome's renderer-process architecture:

- **`BlinkNode` / `BlinkElement`** — DOM tree with parent/children pointers and a `listeners` list; mirrors `blink::Node` / `blink::Element`. Virtual destructor puts vtable pointer at offset 0.
- **`V8Function`** — a named `std::function<void(BlinkEvent*)>` representing a JS callback.
- **`V8Wrapper`** — one wrapper per `BlinkNode`, lazily created on first JS access; mirrors the IDL-generated `v8::Object` subclass. Holds a back-pointer to the Blink node and a simulated V8-heap address.
- **`WrapperMap`** — singleton cache mapping `BlinkNode*` → `V8Wrapper*`; mirrors `blink::DOMDataStore`.
- **`BlinkEvent`** — carries `type`, `target`, `currentTarget`, `defaultPrevented`, `propagationStopped`.
- **`EventDispatcher`** — implements the DOM Events 3-phase algorithm: CAPTURE (root→target), AT-TARGET, BUBBLE (target→root). At each element it crosses the Blink→V8 boundary by calling the stored `V8Function::body`.

The interactive loop at the end of `section7()` accepts an element id and event type from stdin and runs the full dispatch pipeline, printing each boundary crossing.
