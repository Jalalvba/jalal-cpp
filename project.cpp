#include <iostream>
#include <iomanip>      // std::setw, std::left, std::right
#include <cstdint>      // uintptr_t
#include <cstring>      // memset
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <memory>
#include <cassert>
#include <algorithm>    // std::remove_if

// ─────────────────────────────────────────────────────────────────────────────
// Helper: print the raw hex bytes of an object so you can see exact RAM layout
// ─────────────────────────────────────────────────────────────────────────────
template<typename T>
void dump_bytes(const char* label, const T& obj) {
    const unsigned char* p = reinterpret_cast<const unsigned char*>(&obj);
    std::cout << "  bytes of " << label << ": ";
    for (std::size_t i = 0; i < sizeof(T); ++i)
        std::printf("%02X ", p[i]);
    std::cout << "\n";
}

// ═════════════════════════════════════════════════════════════════════════════
// SECTION 1 — Struct with two fields
//
// The compiler lays out fields in declaration order.  Each field starts at
// the lowest address available that satisfies its alignment requirement.
// For two ints (align = 4) there is no padding between them; they sit
// back-to-back in RAM.
// ═════════════════════════════════════════════════════════════════════════════
struct TwoFields {
    int x;   // offset 0 — CPU loads this with a 4-byte aligned read
    int y;   // offset 4 — immediately follows x, no padding needed
};

void section1() {
    std::cout << "\n══════════════════════════════════════════════\n";
    std::cout << "SECTION 1 — struct with two consecutive fields\n";
    std::cout << "══════════════════════════════════════════════\n";

    TwoFields tf;
    tf.x = 0xAAAA;
    tf.y = 0xBBBB;

    // The CPU pushes the whole struct onto the stack as a single contiguous
    // block.  &tf is the address of the block's first byte.
    std::cout << "  sizeof(TwoFields)  = " << sizeof(TwoFields) << " bytes\n";
    std::cout << "  &tf                = " << &tf << "\n";
    std::cout << "  &tf.x              = " << &tf.x << "\n";
    std::cout << "  &tf.y              = " << &tf.y << "\n";

    // Prove fields are consecutive:
    uintptr_t ax = reinterpret_cast<uintptr_t>(&tf.x);
    uintptr_t ay = reinterpret_cast<uintptr_t>(&tf.y);
    std::cout << "  gap (y - x)        = " << (ay - ax) << " bytes  "
              << "(== sizeof(int) = " << sizeof(int) << ")\n";

    // Show the actual bytes in memory: first 4 bytes = x, next 4 = y
    dump_bytes("tf", tf);
}

// ═════════════════════════════════════════════════════════════════════════════
// SECTION 2 — Struct with a method
//
// Member functions are NOT stored inside the struct.  The compiler turns
//   obj.greet()
// into a normal function call, passing a hidden pointer-to-self:
//   TwoFieldsPlus::greet(&obj)   ← the "this" pointer
// The struct's size is therefore identical to a struct with the same fields
// but no methods.
// ═════════════════════════════════════════════════════════════════════════════
struct WithMethod {
    int a;
    int b;

    // This method lives in the code segment (.text), NOT in the struct's RAM
    int sum() const {
        // CPU: load this->a, load this->b, add, return result in register
        return a + b;
    }
};

void section2() {
    std::cout << "\n══════════════════════════════════════════════\n";
    std::cout << "SECTION 2 — struct with a method\n";
    std::cout << "══════════════════════════════════════════════\n";

    WithMethod wm;
    wm.a = 3;
    wm.b = 7;

    std::cout << "  sizeof(WithMethod) = " << sizeof(WithMethod) << " bytes "
              << "(same as two raw ints — method takes NO space)\n";
    std::cout << "  &wm                = " << &wm << "\n";
    std::cout << "  &wm.a              = " << &wm.a << "\n";
    std::cout << "  &wm.b              = " << &wm.b << "\n";
    std::cout << "  wm.sum()           = " << wm.sum() << "\n";

    // The method's machine code lives at a fixed address in the code segment.
    // We can take its address to confirm it is nowhere near &wm.
    // Member-function pointers may be larger than uintptr_t on some ABIs;
    // copy only the first sizeof(uintptr_t) bytes (the code address portion).
    auto fn_ptr = &WithMethod::sum;
    uintptr_t fn_addr = 0;
    std::memcpy(&fn_addr, &fn_ptr,
                (sizeof(fn_ptr) < sizeof(fn_addr)) ? sizeof(fn_ptr)
                                                   : sizeof(fn_addr));
    std::cout << "  address of sum()   ≈ " << (void*)fn_addr
              << "  (code segment, far from stack data above)\n";
}

// ═════════════════════════════════════════════════════════════════════════════
// SECTION 3 — Array of structs
//
// An array stores elements contiguously.  The CPU computes the address of
// element i as:
//   base_address + i * sizeof(Element)
// This is one instruction (LEA on x86): no pointer chasing needed.
// ═════════════════════════════════════════════════════════════════════════════
struct Point {
    int x;
    int y;
};

void section3() {
    std::cout << "\n══════════════════════════════════════════════\n";
    std::cout << "SECTION 3 — array of structs\n";
    std::cout << "══════════════════════════════════════════════\n";

    Point pts[4] = { {1,2}, {3,4}, {5,6}, {7,8} };

    std::cout << "  sizeof(Point)       = " << sizeof(Point) << " bytes\n";
    std::cout << "  base address pts[0] = " << &pts[0] << "\n\n";

    for (int i = 0; i < 4; ++i) {
        uintptr_t addr = reinterpret_cast<uintptr_t>(&pts[i]);
        uintptr_t base = reinterpret_cast<uintptr_t>(&pts[0]);
        std::cout << "  pts[" << i << "]  addr=" << &pts[i]
                  << "  offset=" << (addr - base)
                  << "  (== " << i << " * " << sizeof(Point) << ")"
                  << "  val=(" << pts[i].x << "," << pts[i].y << ")\n";
    }

    // Verify stride
    uintptr_t stride = reinterpret_cast<uintptr_t>(&pts[1])
                     - reinterpret_cast<uintptr_t>(&pts[0]);
    std::cout << "\n  stride between elements = " << stride
              << " bytes == sizeof(Point)\n";
}

// ═════════════════════════════════════════════════════════════════════════════
// SECTION 4 — Nested struct
//
// Outer contains Inner as a value (not a pointer).  The Inner object is
// literally embedded at offset outer.inner_offset inside Outer's memory block.
// The CPU accesses inner.z as:  base + offsetof(Outer,inner) + offsetof(Inner,z)
// ═════════════════════════════════════════════════════════════════════════════
struct Inner {
    char  flag;   // 1 byte  (offset 0 inside Inner)
    // 3 bytes of padding here so that 'value' is 4-byte aligned
    int   value;  // 4 bytes (offset 4 inside Inner)
};                // total Inner = 8 bytes

struct Outer {
    double  d;    // 8 bytes (offset 0)
    Inner   in;   // 8 bytes (offset 8) — embedded directly
    int     tail; // 4 bytes (offset 16)
    // 4 bytes padding to keep Outer's size a multiple of 8 (largest member)
};                // total Outer = 24 bytes

void section4() {
    std::cout << "\n══════════════════════════════════════════════\n";
    std::cout << "SECTION 4 — nested struct memory layout\n";
    std::cout << "══════════════════════════════════════════════\n";

    Outer obj;
    obj.d       = 3.14;
    obj.in.flag = 'A';
    obj.in.value = 999;
    obj.tail    = 42;

    uintptr_t base = reinterpret_cast<uintptr_t>(&obj);

    std::cout << "  sizeof(Inner)      = " << sizeof(Inner)  << " bytes\n";
    std::cout << "  sizeof(Outer)      = " << sizeof(Outer)  << " bytes\n\n";

    // Print as raw pointers — operator<< on void* already adds "0x"
    auto pp = [](const void* p){ return p; };

    auto off = [&](auto& member) -> std::ptrdiff_t {
        return reinterpret_cast<uintptr_t>(&member) - base;
    };

    std::cout << "  &obj               = " << pp(&obj)           << "\n";
    std::cout << "  &obj.d             = " << pp(&obj.d)         << "  offset=" << off(obj.d)       << "\n";
    std::cout << "  &obj.in            = " << pp(&obj.in)        << "  offset=" << off(obj.in)      << "\n";
    std::cout << "  &obj.in.flag       = " << pp(&obj.in.flag)   << "  offset=" << off(obj.in.flag) << "\n";
    std::cout << "  &obj.in.value      = " << pp(&obj.in.value)  << "  offset=" << off(obj.in.value)<< "\n";
    std::cout << "  &obj.tail          = " << pp(&obj.tail)      << "  offset=" << off(obj.tail)    << "\n";

    std::cout << "\n  Memory map (each row = 1 byte, left = low address):\n";
    std::cout << "    [0-7]   obj.d       (double, 8 bytes)\n";
    std::cout << "    [8]     obj.in.flag (char,   1 byte)\n";
    std::cout << "    [9-11]  <padding>   (3 bytes to align int)\n";
    std::cout << "    [12-15] obj.in.value(int,    4 bytes)\n";
    std::cout << "    [16-19] obj.tail    (int,    4 bytes)\n";
    std::cout << "    [20-23] <padding>   (4 bytes to align Outer to 8)\n";
}

// ═════════════════════════════════════════════════════════════════════════════
// SECTION 5 — Pointer to struct
//
// A pointer stores an address (8 bytes on 64-bit).  Dereferencing it with ->
// tells the CPU: "go to that address, then read/write at the given offset."
// Assigning through the pointer modifies the original object directly — no
// copy is made.
// ═════════════════════════════════════════════════════════════════════════════
void section5() {
    std::cout << "\n══════════════════════════════════════════════\n";
    std::cout << "SECTION 5 — pointer to struct\n";
    std::cout << "══════════════════════════════════════════════\n";

    TwoFields original;
    original.x = 10;
    original.y = 20;

    // p stores the ADDRESS of 'original'.  p itself is 8 bytes on the stack.
    TwoFields* p = &original;

    std::cout << "  sizeof(TwoFields*) = " << sizeof(p) << " bytes (just an address)\n";
    std::cout << "  &original          = " << &original << "\n";
    std::cout << "  p (value of ptr)   = " << p
              << "  ← same address\n";
    std::cout << "  &p (ptr's own addr)= " << &p
              << "  ← ptr lives here on the stack\n";

    // -> dereference: CPU reads the address stored in p, then adds offset of x
    std::cout << "\n  Before: p->x = " << p->x << ",  p->y = " << p->y << "\n";

    // This writes 0xDEAD to the 4 bytes at address (p + offsetof(x))
    p->x = 0xDEAD;
    p->y = 0xBEEF;

    std::cout << "  After p->x=0xDEAD, p->y=0xBEEF:\n";
    std::cout << "  original.x = " << std::hex << original.x
              << "  original.y = " << original.y << std::dec
              << "  ← original was modified through the pointer\n";
    dump_bytes("original after pointer write", original);
}

// ═════════════════════════════════════════════════════════════════════════════
// SECTION 6 — Reference to struct
//
// A reference is an alias.  The compiler typically implements it as a hidden
// pointer, but unlike a pointer you cannot reseat it or take its address to
// get the reference variable itself.  Using 'ref' is identical to using
// 'original' — the CPU generates the exact same memory accesses.
// ═════════════════════════════════════════════════════════════════════════════
void section6() {
    std::cout << "\n══════════════════════════════════════════════\n";
    std::cout << "SECTION 6 — reference to struct\n";
    std::cout << "══════════════════════════════════════════════\n";

    TwoFields original;
    original.x = 100;
    original.y = 200;

    // 'ref' binds to 'original'.  No copy.  The CPU treats &ref as &original.
    TwoFields& ref = original;

    std::cout << "  &original = " << &original << "\n";
    std::cout << "  &ref      = " << &ref
              << "  ← identical address — same object\n";
    std::cout << "  (&original == &ref) is "
              << ((&original == &ref) ? "true" : "false") << "\n";

    // Modifying through ref is the same machine instruction as modifying original
    ref.x = 999;
    std::cout << "\n  After ref.x = 999:\n";
    std::cout << "  original.x = " << original.x
              << "  ← changed because ref IS original\n";
    dump_bytes("original after ref write", original);

    std::cout << "\n  sizeof(ref) reports the referred-to type, not the ref itself:\n";
    std::cout << "  sizeof(ref) = " << sizeof(ref) << " bytes (== sizeof(TwoFields))\n";
}

// ═════════════════════════════════════════════════════════════════════════════
// SECTION 7 — Blink + V8 Pipeline Simulation
//
// Chrome's renderer process has two major engines that share the same process:
//
//   ┌─────────────────────────────────────────────────────────────────────┐
//   │  Renderer Process                                                   │
//   │                                                                     │
//   │   ┌─────────────────────┐   IDL bindings   ┌──────────────────┐    │
//   │   │       Blink          │ ◄──────────────► │       V8         │    │
//   │   │  (DOM / CSS / Layout │                  │  (JS JIT engine) │    │
//   │   │   / Paint / Events)  │                  │                  │    │
//   │   └─────────────────────┘                   └──────────────────┘    │
//   └─────────────────────────────────────────────────────────────────────┘
//
// THE WRAPPER PATTERN (core of the bridge):
//   Every Blink DOM Node has exactly one V8 "wrapper" object on the V8 heap.
//   The wrapper keeps the C++ node alive (via a persistent handle) while JS
//   holds a reference, and lets JS read/write DOM properties via generated
//   accessor callbacks.
//
//   BlinkNode  ◄──── raw ptr ────  V8Wrapper (lives on V8 heap)
//      │                               │
//      │  stores back-ref              │  JS can reach this via
//      └── wrapper_                    │  document.getElementById(...)
//
// EVENT DISPATCH ACROSS THE BRIDGE (what this section simulates):
//
//   JS side (V8):
//     btn.addEventListener("click", function handler(e) { ... })
//     │
//     │  V8 calls the Blink binding for EventTarget::addEventListener
//     ▼
//   Blink side:
//     Stores the JS function as a V8EventListener inside the element's
//     listener list.
//     │
//     │  User clicks → InputEvent → Blink EventDispatcher runs:
//     │    Phase 1 CAPTURE  — root → target (ancestors first)
//     │    Phase 2 AT-TARGET — target element
//     │    Phase 3 BUBBLE   — target → root (ancestors last)
//     │
//     │  At each element, for each matching listener:
//     ▼
//   Back to V8:
//     V8::Function::Call(handler, event_wrapper)
//     JS callback runs, can call e.preventDefault(), e.stopPropagation()
//
// This simulation models all of the above with real classes and memory
// addresses so you can see how objects are laid out and connected.
// ═════════════════════════════════════════════════════════════════════════════

// ─── Forward declarations ────────────────────────────────────────────────────
class BlinkNode;
class BlinkElement;
class BlinkEvent;

// ─── V8 simulation ───────────────────────────────────────────────────────────
// In real Chrome, V8 objects live on a managed heap.  Here we simulate them
// with plain C++ objects to expose the same structural relationships.

struct V8Function {
    // A JS function is a first-class V8 heap object.
    // We represent the "body" as a std::function<void(BlinkEvent*)>.
    std::string        name;           // function name (for tracing)
    std::function<void(BlinkEvent*)> body; // the actual JS code
};

// A V8Wrapper wraps one Blink DOM node.
// In Chrome it is a v8::Object subclass generated by the Web IDL compiler.
struct V8Wrapper {
    BlinkNode*  blink_node;    // raw pointer back to the C++ Blink object
    std::string js_class_name; // e.g. "HTMLButtonElement"
    uintptr_t   heap_address;  // fake V8-heap address (for illustration)

    explicit V8Wrapper(BlinkNode* n, std::string cls)
        : blink_node(n), js_class_name(std::move(cls))
    {
        // Simulate a V8 heap address: offset from the node's own address
        heap_address = reinterpret_cast<uintptr_t>(n) ^ 0xDEAD'0000ULL;
    }
};

// ─── Blink DOM ───────────────────────────────────────────────────────────────

// BlinkEvent mirrors blink::Event.
// It carries: type string, current target during dispatch, and two flags that
// JS can set via e.preventDefault() / e.stopPropagation().
class BlinkEvent {
public:
    std::string   type;
    BlinkElement* target    = nullptr;  // the element the event was dispatched on
    BlinkElement* currentTarget = nullptr; // element whose listeners are firing now
    bool          defaultPrevented = false;
    bool          propagationStopped = false;
    bool          bubbles = true;

    explicit BlinkEvent(std::string t, bool b = true)
        : type(std::move(t)), bubbles(b) {}

    // These simulate e.preventDefault() and e.stopPropagation() in JS
    void preventDefault()    { defaultPrevented     = true; }
    void stopPropagation()   { propagationStopped   = true; }
};

// An EventListener as stored inside a Blink element.
// In Chrome this is blink::EventListener, with a subclass V8EventListener
// that holds a persistent V8::Function handle.
struct StoredListener {
    std::string   event_type;
    bool          use_capture;  // capture phase vs bubble phase
    V8Function    js_fn;        // the JS function to call
};

// BlinkNode is the base of the Blink DOM tree.
// Its vtable pointer is the first 8 bytes of every node object.
class BlinkNode {
public:
    // Virtual destructor → vtable pointer at offset 0
    virtual ~BlinkNode() = default;

    // Node type constants (mirrors blink::Node::NodeType)
    enum NodeType : uint16_t {
        ELEMENT_NODE   = 1,
        TEXT_NODE      = 3,
        DOCUMENT_NODE  = 9,
    };

    virtual NodeType    nodeType() const = 0;
    virtual std::string nodeName() const = 0;

    BlinkNode*              parent   = nullptr;
    std::vector<BlinkNode*> children;

    // Back-pointer to this node's V8 wrapper (null until first JS access)
    V8Wrapper*  wrapper_ = nullptr;

    void appendChild(BlinkNode* child) {
        child->parent = this;
        children.push_back(child);
    }
};

// BlinkElement mirrors blink::Element.
// Adds: tagName, id attribute, inline style hint, and the listener list.
class BlinkElement : public BlinkNode {
public:
    std::string tag;   // "div", "button", "span", …
    std::string id;    // value of the id="" attribute
    std::string style_hint; // just for display

    std::vector<StoredListener> listeners; // blink::EventTarget's listener list

    explicit BlinkElement(std::string t, std::string i = "")
        : tag(std::move(t)), id(std::move(i)) {}

    NodeType    nodeType() const override { return ELEMENT_NODE; }
    std::string nodeName() const override { return tag; }

    // Simulate element.addEventListener(type, fn, useCapture)
    // Called by the V8 binding generated for EventTarget::addEventListener.
    void addEventListener(const std::string& type,
                          V8Function fn,
                          bool use_capture = false)
    {
        listeners.push_back({ type, use_capture, std::move(fn) });
    }

    std::string display() const {
        return "<" + tag + (id.empty() ? "" : " id=\"" + id + "\"") + ">";
    }
};

// ─── WrapperMap (the Blink-V8 bridge's wrapper cache) ────────────────────────
// In Chrome this is blink::DOMDataStore.
// It maps each BlinkNode* to its single V8Wrapper* (created on first access).
class WrapperMap {
    std::unordered_map<BlinkNode*, std::unique_ptr<V8Wrapper>> map_;
public:
    // Returns (or creates) the V8 wrapper for a Blink node.
    // In Chrome: V8DOMWrapper::associateObjectWithWrapper
    V8Wrapper* wrapperFor(BlinkNode* node, const std::string& js_class) {
        auto it = map_.find(node);
        if (it == map_.end()) {
            auto w = std::make_unique<V8Wrapper>(node, js_class);
            node->wrapper_ = w.get();
            it = map_.emplace(node, std::move(w)).first;
            std::cout << "    [WrapperMap] created V8Wrapper for "
                      << static_cast<BlinkElement*>(node)->display()
                      << "  V8-heap-addr=0x" << std::hex << node->wrapper_->heap_address
                      << std::dec << "\n";
        }
        return it->second.get();
    }
};

// ─── EventDispatcher (mirrors blink::EventDispatcher) ────────────────────────
class EventDispatcher {
    WrapperMap& wmap_;
public:
    explicit EventDispatcher(WrapperMap& wm) : wmap_(wm) {}

    // Dispatch 'event' targeting 'target'.
    // Implements the DOM Events spec dispatch algorithm:
    //   1. Build the ancestor chain (path)
    //   2. CAPTURE phase: walk root→target, fire capture listeners
    //   3. AT-TARGET phase: fire both capture AND bubble listeners on target
    //   4. BUBBLE phase: walk target→root, fire bubble listeners
    void dispatch(BlinkEvent& event, BlinkElement* target) {
        event.target = target;

        // ── Build ancestor path (root at front) ──────────────────────────
        std::vector<BlinkElement*> path;
        for (BlinkNode* n = target->parent; n; n = n->parent)
            if (auto* e = dynamic_cast<BlinkElement*>(n))
                path.insert(path.begin(), e);

        std::cout << "\n  [EventDispatcher] dispatch(" << event.type << ")"
                  << "  target=" << target->display() << "\n";
        std::cout << "  ancestor path (root→target): ";
        for (auto* e : path) std::cout << e->display() << " → ";
        std::cout << target->display() << "\n";

        // ── Phase 1: CAPTURE ─────────────────────────────────────────────
        std::cout << "\n  ── Phase 1: CAPTURE (root→target) ──\n";
        for (auto* ancestor : path) {
            if (event.propagationStopped) break;
            invokeListeners(event, ancestor, /*capture=*/true);
        }

        // ── Phase 2: AT-TARGET ───────────────────────────────────────────
        std::cout << "\n  ── Phase 2: AT-TARGET ──\n";
        if (!event.propagationStopped) {
            invokeListeners(event, target, /*capture=*/true);   // capture listeners on target
            invokeListeners(event, target, /*capture=*/false);  // bubble  listeners on target
        }

        // ── Phase 3: BUBBLE ──────────────────────────────────────────────
        if (event.bubbles) {
            std::cout << "\n  ── Phase 3: BUBBLE (target→root) ──\n";
            for (int i = (int)path.size() - 1; i >= 0; --i) {
                if (event.propagationStopped) break;
                invokeListeners(event, path[i], /*capture=*/false);
            }
        }

        std::cout << "\n  [EventDispatcher] dispatch done."
                  << "  defaultPrevented=" << event.defaultPrevented
                  << "  propagationStopped=" << event.propagationStopped << "\n";
    }

private:
    // Fire all listeners on 'element' that match event.type and the phase.
    void invokeListeners(BlinkEvent& event,
                         BlinkElement* element,
                         bool capture_phase)
    {
        bool fired_any = false;
        for (auto& sl : element->listeners) {
            if (sl.event_type != event.type) continue;
            if (sl.use_capture != capture_phase) continue;
            if (event.propagationStopped) break;

            event.currentTarget = element;

            // ── Blink → V8 boundary crossing ────────────────────────────
            // In Chrome: V8::Function::Call(context, receiver, argc, argv)
            // The receiver is the wrapper for currentTarget.
            // argv[0] is the wrapper for the Event object.
            V8Wrapper* recv = wmap_.wrapperFor(
                element,
                "HTML" + capitalise(element->tag) + "Element");

            std::cout << "    [Blink→V8] calling '" << sl.js_fn.name << "'"
                      << " on " << element->display()
                      << "  (wrapper@0x" << std::hex << recv->heap_address
                      << std::dec << ")"
                      << "  phase=" << (capture_phase ? "CAPTURE" : "BUBBLE") << "\n";

            // Actually invoke the stored JS function body
            sl.js_fn.body(&event);
            fired_any = true;
        }
        if (!fired_any) {
            std::cout << "    [" << element->display()
                      << "] no " << (capture_phase ? "capture" : "bubble")
                      << " listeners for '" << event.type << "'\n";
        }
    }

    static std::string capitalise(const std::string& s) {
        if (s.empty()) return s;
        std::string r = s;
        r[0] = (char)std::toupper((unsigned char)r[0]);
        return r;
    }
};

// ─── section7: interactive demo ──────────────────────────────────────────────
void section7() {
    std::cout << "\n══════════════════════════════════════════════\n";
    std::cout << "SECTION 7 — Blink + V8 Pipeline Simulation\n";
    std::cout << "══════════════════════════════════════════════\n";

    // ── 1. Build a tiny DOM tree (Blink side) ────────────────────────────────
    //
    //   <div id="app">               ← root
    //     <div id="container">
    //       <button id="btn">        ← default click target
    //         <span id="label">
    std::cout << "\n[Blink] Building DOM tree...\n";

    BlinkElement root   ("div",    "app");
    BlinkElement cont   ("div",    "container");
    BlinkElement button ("button", "btn");
    BlinkElement label  ("span",   "label");

    root.appendChild(&cont);
    cont.appendChild(&button);
    button.appendChild(&label);

    std::cout << "  " << root.display()
              << " → " << cont.display()
              << " → " << button.display()
              << " → " << label.display() << "\n";

    // ── 2. JS registers event listeners (V8→Blink binding) ──────────────────
    //
    //   In a real page these are called by generated binding code when JS does:
    //     document.getElementById("app").addEventListener("click", logCapture, true)
    std::cout << "\n[V8→Blink bindings] addEventListener calls...\n";

    button.addEventListener("click", {
        "handleClick",
        [](BlinkEvent* e) {
            std::cout << "      JS: handleClick fired!  target="
                      << e->target->display()
                      << "  currentTarget=" << e->currentTarget->display() << "\n";
        }
    }, /*capture=*/false);

    cont.addEventListener("click", {
        "containerCapture",
        [](BlinkEvent* e) {
            std::cout << "      JS: containerCapture (capture phase)"
                      << "  target=" << e->target->display() << "\n";
        }
    }, /*capture=*/true);

    root.addEventListener("click", {
        "appBubble",
        [](BlinkEvent* e) {
            std::cout << "      JS: appBubble (bubble phase)"
                      << "  target=" << e->target->display() << "\n";
        }
    }, /*capture=*/false);

    // ── 3. Memory layout overview ────────────────────────────────────────────
    std::cout << "\n[Memory] Blink DOM node addresses (stack in this demo):\n";
    auto showNode = [](const char* var, const BlinkElement& el) {
        std::cout << "  " << var << " " << el.display()
                  << "  @" << (void*)&el
                  << "  vtable_ptr=" << *(void**)&el
                  << "  sizeof=" << sizeof(el) << " bytes\n";
    };
    showNode("root  ", root);
    showNode("cont  ", cont);
    showNode("button", button);
    showNode("label ", label);

    // Measure live offsets from the vtable base (pointer arithmetic, no UB)
    // In a real Blink binary the vtable pointer occupies bytes [0,8).
    auto measured_offset = [](const void* base, const void* field) -> std::ptrdiff_t {
        return static_cast<const char*>(field) - static_cast<const char*>(base);
    };
    std::cout << "\n  live offsets inside 'button' object (vtable at offset 0):\n";
    std::cout << "    tag       field @ offset "
              << measured_offset(&button, &button.tag) << "\n";
    std::cout << "    id        field @ offset "
              << measured_offset(&button, &button.id) << "\n";
    std::cout << "    listeners field @ offset "
              << measured_offset(&button, &button.listeners) << "\n";
    std::cout << "    wrapper_  field @ offset "
              << measured_offset(&button, &button.wrapper_) << "\n";
    std::cout << "    sizeof(BlinkElement) = " << sizeof(BlinkElement) << " bytes\n";

    // ── 4. Interactive dispatch loop ─────────────────────────────────────────
    WrapperMap    wmap;
    EventDispatcher dispatcher(wmap);

    // Map id → element pointer for user lookup
    std::unordered_map<std::string, BlinkElement*> id_map = {
        {"app",       &root},
        {"container", &cont},
        {"btn",       &button},
        {"label",     &label},
    };

    std::cout << "\n──────────────────────────────────────────────\n";
    std::cout << "Interactive Blink+V8 event dispatch simulator\n";
    std::cout << "Available element IDs: app  container  btn  label\n";
    std::cout << "Available event types: click  mouseover  keydown\n";
    std::cout << "Type 'quit' for either prompt to exit.\n";
    std::cout << "──────────────────────────────────────────────\n";

    while (true) {
        std::cout << "\nEnter target element id: ";
        std::string elem_id;
        if (!(std::cin >> elem_id) || elem_id == "quit") break;

        auto it = id_map.find(elem_id);
        if (it == id_map.end()) {
            std::cout << "  Unknown id '" << elem_id << "'. Try: app container btn label\n";
            continue;
        }

        std::cout << "Enter event type       : ";
        std::string ev_type;
        if (!(std::cin >> ev_type) || ev_type == "quit") break;

        // Create the Blink event and run the full dispatch pipeline
        BlinkEvent ev(ev_type, /*bubbles=*/true);
        dispatcher.dispatch(ev, it->second);
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// SECTION 8 — Compiler Pipeline Simulation
//
// When you run:   g++ -std=c++17 -o project project.cpp
// the compiler (GCC/Clang) executes these stages in order:
//
//   Source text
//       │
//   [1] Preprocessor  — expands #include / #define / #ifdef
//       │
//   [2] Lexer          — converts characters → stream of tokens
//       │
//   [3] Parser         — tokens → Abstract Syntax Tree (AST)
//       │
//   [4] Semantic       — type-check, name resolution, build symbol table
//   Analysis
//       │
//   [5] IR Generation  — AST → platform-independent Intermediate Representation
//       │
//   [6] Optimiser      — constant folding, dead-code elimination, inlining, …
//       │
//   [7] Code Gen       — IR → target machine instructions (x86-64 asm here)
//       │
//   [8] Assembler       — text asm → relocatable object file (.o)
//       │
//   [9] Linker          — combine .o files + libraries → executable
//
// This section runs a tiny self-contained pipeline on the expression:
//
//     int result = 2 + 3 * 4;
//
// and prints every data structure the compiler builds along the way.
// ═════════════════════════════════════════════════════════════════════════════

// ─── Stage 1 helper: Preprocessor ───────────────────────────────────────────
// Real work: resolve #include, substitute #define macros, strip comments.
// Here we just demonstrate macro substitution on a source string.
static std::string stage1_preprocess(const std::string& src) {
    // Substitute a simple #define MAX_VAL 42 so the demo shows textual replacement.
    std::string out = src;
    const std::string macro_name  = "MAX_VAL";
    const std::string macro_value = "42";
    std::size_t pos = 0;
    while ((pos = out.find(macro_name, pos)) != std::string::npos) {
        out.replace(pos, macro_name.size(), macro_value);
        pos += macro_value.size();
    }
    return out;
}

// ─── Stage 2: Lexer ──────────────────────────────────────────────────────────
// The lexer (tokeniser) scans characters left-to-right and groups them into
// tokens.  Each token has a kind and the original text.
enum class TokenKind {
    KW_INT,     // "int"
    IDENT,      // identifier: "result"
    NUMBER,     // integer literal: "2", "3", "4"
    PLUS,       // '+'
    STAR,       // '*'
    EQUALS,     // '='
    SEMICOLON,  // ';'
    END,        // end of input
};

static const char* token_kind_name(TokenKind k) {
    switch (k) {
        case TokenKind::KW_INT:    return "KW_INT";
        case TokenKind::IDENT:     return "IDENT";
        case TokenKind::NUMBER:    return "NUMBER";
        case TokenKind::PLUS:      return "PLUS";
        case TokenKind::STAR:      return "STAR";
        case TokenKind::EQUALS:    return "EQUALS";
        case TokenKind::SEMICOLON: return "SEMICOLON";
        case TokenKind::END:       return "END";
    }
    return "?";
}

struct Token {
    TokenKind   kind;
    std::string text;
    int         col;    // column in source (1-based)
};

static std::vector<Token> stage2_lex(const std::string& src) {
    std::vector<Token> tokens;
    std::size_t i = 0;
    while (i < src.size()) {
        // skip whitespace
        if (std::isspace((unsigned char)src[i])) { ++i; continue; }

        int col = (int)i + 1;
        // number
        if (std::isdigit((unsigned char)src[i])) {
            std::string num;
            while (i < src.size() && std::isdigit((unsigned char)src[i]))
                num += src[i++];
            tokens.push_back({TokenKind::NUMBER, num, col});
            continue;
        }
        // keyword or identifier
        if (std::isalpha((unsigned char)src[i]) || src[i] == '_') {
            std::string word;
            while (i < src.size() && (std::isalnum((unsigned char)src[i]) || src[i] == '_'))
                word += src[i++];
            TokenKind k = (word == "int") ? TokenKind::KW_INT : TokenKind::IDENT;
            tokens.push_back({k, word, col});
            continue;
        }
        // single-char tokens
        TokenKind k;
        switch (src[i]) {
            case '+': k = TokenKind::PLUS;      break;
            case '*': k = TokenKind::STAR;      break;
            case '=': k = TokenKind::EQUALS;    break;
            case ';': k = TokenKind::SEMICOLON; break;
            default:  ++i; continue;  // unknown char, skip
        }
        tokens.push_back({k, std::string(1, src[i]), col});
        ++i;
    }
    tokens.push_back({TokenKind::END, "", (int)src.size() + 1});
    return tokens;
}

// ─── Stage 3: Parser — builds an AST ─────────────────────────────────────────
// Grammar (tiny subset):
//   stmt   := "int" IDENT "=" expr ";"
//   expr   := term  ("+" term)*          ← left-assoc addition
//   term   := factor ("*" factor)*       ← left-assoc multiplication
//   factor := NUMBER
//
// Operator precedence is handled by nesting: expr calls term, term calls factor.

enum class ASTKind { IntDecl, BinOp, Literal, Var };

struct ASTNode {
    ASTKind     kind;
    std::string text;        // literal value or operator or variable name
    std::unique_ptr<ASTNode> left, right; // children for BinOp / IntDecl
    int depth = 0;           // for pretty-printing
};

// Recursive-descent parser — each function consumes tokens from 'pos'
static std::unique_ptr<ASTNode> parse_factor(const std::vector<Token>& toks, std::size_t& pos);
static std::unique_ptr<ASTNode> parse_term  (const std::vector<Token>& toks, std::size_t& pos);
static std::unique_ptr<ASTNode> parse_expr  (const std::vector<Token>& toks, std::size_t& pos);

static std::unique_ptr<ASTNode> parse_factor(const std::vector<Token>& toks, std::size_t& pos) {
    auto node = std::make_unique<ASTNode>();
    node->kind = ASTKind::Literal;
    node->text = toks[pos].text;   // the number
    ++pos;
    return node;
}

static std::unique_ptr<ASTNode> parse_term(const std::vector<Token>& toks, std::size_t& pos) {
    auto node = parse_factor(toks, pos);
    while (pos < toks.size() && toks[pos].kind == TokenKind::STAR) {
        ++pos; // consume '*'
        auto bin = std::make_unique<ASTNode>();
        bin->kind  = ASTKind::BinOp;
        bin->text  = "*";
        bin->left  = std::move(node);
        bin->right = parse_factor(toks, pos);
        node = std::move(bin);
    }
    return node;
}

static std::unique_ptr<ASTNode> parse_expr(const std::vector<Token>& toks, std::size_t& pos) {
    auto node = parse_term(toks, pos);
    while (pos < toks.size() && toks[pos].kind == TokenKind::PLUS) {
        ++pos; // consume '+'
        auto bin = std::make_unique<ASTNode>();
        bin->kind  = ASTKind::BinOp;
        bin->text  = "+";
        bin->left  = std::move(node);
        bin->right = parse_term(toks, pos);
        node = std::move(bin);
    }
    return node;
}

static std::unique_ptr<ASTNode> stage3_parse(const std::vector<Token>& toks) {
    // Expect:  KW_INT  IDENT  EQUALS  <expr>  SEMICOLON
    std::size_t pos = 0;
    auto decl       = std::make_unique<ASTNode>();
    decl->kind      = ASTKind::IntDecl;
    decl->text      = "int_decl";

    ++pos; // KW_INT
    auto var        = std::make_unique<ASTNode>();
    var->kind       = ASTKind::Var;
    var->text       = toks[pos].text;   // "result"
    ++pos;  // IDENT
    ++pos;  // EQUALS

    decl->left  = std::move(var);
    decl->right = parse_expr(toks, pos);
    // pos now points at SEMICOLON — skip it
    return decl;
}

// Pretty-print AST with indentation
static void print_ast(const ASTNode* n, int depth = 0) {
    if (!n) return;
    std::string indent(depth * 4, ' ');
    switch (n->kind) {
        case ASTKind::IntDecl: std::cout << indent << "IntDecl\n"; break;
        case ASTKind::BinOp:   std::cout << indent << "BinOp(" << n->text << ")\n"; break;
        case ASTKind::Literal: std::cout << indent << "Literal(" << n->text << ")\n"; break;
        case ASTKind::Var:     std::cout << indent << "Var(" << n->text << ")\n"; break;
    }
    print_ast(n->left.get(),  depth + 1);
    print_ast(n->right.get(), depth + 1);
}

// ─── Stage 4: Semantic Analysis ──────────────────────────────────────────────
// Walks the AST:
//  - checks every Literal is a valid integer
//  - records the declared variable in a symbol table
//  - computes and annotates each node's type (always "int" here)
struct Symbol {
    std::string name;
    std::string type;
    int         stack_offset; // byte offset from frame pointer (simulated)
};

static std::vector<Symbol> symbol_table;

static std::string stage4_analyse(const ASTNode* n, int frame_offset = 0) {
    if (!n) return "";
    switch (n->kind) {
        case ASTKind::IntDecl: {
            std::string var_name = n->left ? n->left->text : "?";
            symbol_table.push_back({var_name, "int", frame_offset});
            stage4_analyse(n->right.get(), frame_offset);
            return "int";
        }
        case ASTKind::BinOp: {
            std::string lt = stage4_analyse(n->left.get(),  frame_offset);
            std::string rt = stage4_analyse(n->right.get(), frame_offset);
            if (lt != rt) {
                std::cout << "    [Semantic ERROR] type mismatch: "
                          << lt << " " << n->text << " " << rt << "\n";
            }
            return lt; // both are "int"
        }
        case ASTKind::Literal:
            return "int";
        case ASTKind::Var:
            return "int";
    }
    return "";
}

// ─── Stage 5+6: IR Generation + Constant Folding ────────────────────────────
// We use a three-address code (TAC) IR, the kind Clang/GCC use internally.
// Each instruction is:   result = left  op  right
// or                     result = immediate
//
// Constant folding (optimisation) evaluates expressions with all-constant
// operands at compile time, eliminating the runtime computation entirely.

struct IRInstr {
    std::string result;  // destination temp, e.g. "%t0"
    std::string op;      // "+", "*", "=", "const"
    std::string left;
    std::string right;
};

static std::vector<IRInstr> ir_code;
static int temp_counter = 0;

static std::string new_temp() {
    return "%t" + std::to_string(temp_counter++);
}

// Returns the temp that holds the value of 'n' after emitting IR.
static std::string stage5_emit(const ASTNode* n) {
    if (!n) return "";
    switch (n->kind) {
        case ASTKind::Literal: {
            std::string t = new_temp();
            ir_code.push_back({t, "const", n->text, ""});
            return t;
        }
        case ASTKind::BinOp: {
            std::string l = stage5_emit(n->left.get());
            std::string r = stage5_emit(n->right.get());
            // ── Constant folding (optimiser pass) ────────────────────────
            // If both operands are compile-time constants we can fold them now.
            // Returns the integer value of a temp if it was emitted as a const
            // (either "const" or "const_folded").  Returns false if not found.
            auto find_const = [](const std::string& tmp, int& out) -> bool {
                for (const auto& ins : ir_code)
                    if (ins.result == tmp &&
                        (ins.op == "const" || ins.op == "const_folded")) {
                        out = std::stoi(ins.left);
                        return true;
                    }
                return false;
            };
            int lval = 0, rval = 0;
            bool lv = find_const(l, lval);
            bool rv = find_const(r, rval);
            if (lv && rv) {
                int folded = (n->text == "+") ? (lval + rval) : (lval * rval);
                std::string t = new_temp();
                // Remove the two const instructions (they are now dead)
                ir_code.erase(
                    std::remove_if(ir_code.begin(), ir_code.end(),
                        [&](const IRInstr& ins){
                            return ins.result == l || ins.result == r;
                        }),
                    ir_code.end());
                ir_code.push_back(IRInstr{t, "const_folded",
                                   std::to_string(folded),
                                   "(" + std::to_string(lval) + n->text + std::to_string(rval) + ")"});
                return t;
            }
            std::string t = new_temp();
            ir_code.push_back({t, n->text, l, r});
            return t;
        }
        case ASTKind::IntDecl: {
            std::string rhs = stage5_emit(n->right.get());
            std::string var = n->left ? n->left->text : "?";
            ir_code.push_back({var, "=", rhs, ""});
            return var;
        }
        default: return "";
    }
}

// ─── Stage 7: Code Generation (x86-64 assembly) ──────────────────────────────
// Translates each IR instruction to an x86-64 assembly mnemonic.
// Registers used: rax (accumulator), rbx (scratch), [rbp-4] (local variable).
static void stage7_codegen() {
    std::cout << "  x86-64 assembly output (AT&T syntax):\n";
    std::cout << "  ┌─────────────────────────────────────────────────────┐\n";

    for (const auto& ins : ir_code) {
        if (ins.op == "const") {
            std::cout << "  │  movl   $" << ins.left
                      << ", " << ins.result
                      << "         # " << ins.result << " = " << ins.left << "\n";
        } else if (ins.op == "const_folded") {
            // After constant folding the whole expression is one immediate.
            std::cout << "  │  movl   $" << ins.left
                      << ", %eax              # const-folded: " << ins.right << " = " << ins.left << "\n";
        } else if (ins.op == "+") {
            std::cout << "  │  movl   " << ins.left  << ", %eax\n";
            std::cout << "  │  addl   " << ins.right << ", %eax         # " << ins.result << " = " << ins.left << " + " << ins.right << "\n";
        } else if (ins.op == "*") {
            std::cout << "  │  movl   " << ins.left  << ", %eax\n";
            std::cout << "  │  imull  " << ins.right << ", %eax         # " << ins.result << " = " << ins.left << " * " << ins.right << "\n";
        } else if (ins.op == "=") {
            // If the RHS temp was the most-recently computed value it is already
            // in %eax (register allocation trivially avoids the redundant move).
            bool already_in_eax = (!ir_code.empty() &&
                &ins != &ir_code.front() &&
                (&ins - 1)->result == ins.left);
            if (!already_in_eax)
                std::cout << "  │  movl   " << ins.left << ", %eax\n";
            std::cout << "  │  movl   %eax, -4(%rbp)       # store '" << ins.result << "' to stack\n";
        }
    }
    std::cout << "  └─────────────────────────────────────────────────────┘\n";
}

// ─── section8: run the full pipeline ─────────────────────────────────────────
void section8() {
    std::cout << "\n══════════════════════════════════════════════\n";
    std::cout << "SECTION 8 — Compiler Pipeline Simulation\n";
    std::cout << "══════════════════════════════════════════════\n";

    // ── Input source ─────────────────────────────────────────────────────────
    // We intentionally include a macro so the preprocessor stage has work to do.
    const std::string raw_source = "int result = 2 + 3 * 4;";
    std::cout << "\n  Input source:  \"" << raw_source << "\"\n";

    // ══ Stage 1: Preprocessor ════════════════════════════════════════════════
    std::cout << "\n── Stage 1: Preprocessor ──\n";
    std::cout << "  (No macros in this snippet — output identical to input)\n";
    std::string preprocessed = stage1_preprocess(raw_source);
    std::cout << "  After preprocessing: \"" << preprocessed << "\"\n";

    // Show macro substitution on a separate example
    const std::string macro_example = "int limit = MAX_VAL;";
    std::cout << "\n  Macro demo — before: \"" << macro_example << "\"\n";
    std::cout << "               after:  \"" << stage1_preprocess(macro_example) << "\"\n";
    std::cout << "  What the preprocessor does in RAM:\n";
    std::cout << "    Reads source bytes into a char buffer.\n";
    std::cout << "    Scans for '#define' directives and builds a macro table.\n";
    std::cout << "    Replaces each macro name with its token-sequence value.\n";
    std::cout << "    Outputs a new char buffer — no AST yet, purely textual.\n";

    // ══ Stage 2: Lexer ════════════════════════════════════════════════════════
    std::cout << "\n── Stage 2: Lexer (Tokeniser) ──\n";
    std::cout << "  Input string → stream of tokens (each has kind + text + column)\n";
    auto tokens = stage2_lex(preprocessed);
    for (const auto& tok : tokens) {
        if (tok.kind == TokenKind::END) break;
        std::cout << "    col " << std::setw(2) << tok.col
                  << "  " << std::setw(10) << std::left << token_kind_name(tok.kind)
                  << "  \"" << tok.text << "\"\n";
    }
    std::cout << std::right;
    std::cout << "  In memory: vector of Token structs, each ~48 bytes (kind + string + int)\n";
    std::cout << "  sizeof(Token) = " << sizeof(Token) << " bytes"
              << "  |  " << tokens.size() << " tokens total\n";

    // ══ Stage 3: Parser → AST ════════════════════════════════════════════════
    std::cout << "\n── Stage 3: Parser (builds Abstract Syntax Tree) ──\n";
    std::cout << "  Recursive-descent: parse_expr → parse_term → parse_factor\n";
    std::cout << "  * precedence is encoded in the call depth, not a table\n\n";
    auto ast = stage3_parse(tokens);
    std::cout << "  AST for \"" << preprocessed << "\":\n";
    print_ast(ast.get());
    std::cout << "\n  Each ASTNode is heap-allocated (std::unique_ptr).\n";
    std::cout << "  sizeof(ASTNode) = " << sizeof(ASTNode) << " bytes"
              << "  (kind + text + 2 unique_ptrs + depth)\n";
    // Show actual pointers so the memory layout is visible
    std::cout << "  IntDecl node @ " << (void*)ast.get() << "\n";
    if (ast->left)  std::cout << "  Var node     @ " << (void*)ast->left.get()  << "\n";
    if (ast->right) std::cout << "  BinOp(+) node@ " << (void*)ast->right.get() << "\n";

    // ══ Stage 4: Semantic Analysis ═══════════════════════════════════════════
    std::cout << "\n── Stage 4: Semantic Analysis ──\n";
    std::cout << "  Walk AST, check types, build symbol table.\n";
    symbol_table.clear();
    std::string root_type = stage4_analyse(ast.get(), /*stack frame offset*/ -4);
    std::cout << "  Root expression type: " << root_type << "\n";
    std::cout << "  Symbol table:\n";
    for (const auto& sym : symbol_table) {
        std::cout << "    name=\"" << sym.name << "\""
                  << "  type=" << sym.type
                  << "  stack_offset=" << sym.stack_offset << "(%rbp)\n";
    }

    // ══ Stage 5+6: IR Generation + Constant Folding ══════════════════════════
    std::cout << "\n── Stage 5: IR Generation  +  Stage 6: Constant Folding ──\n";
    std::cout << "  Three-address code (TAC) — same form as LLVM IR / GCC GIMPLE:\n";
    ir_code.clear();
    temp_counter = 0;
    stage5_emit(ast.get());

    std::cout << "  IR instructions (after optimiser):\n";
    for (const auto& ins : ir_code) {
        if (ins.op == "const")
            std::cout << "    " << ins.result << " = " << ins.left << "\n";
        else if (ins.op == "const_folded")
            std::cout << "    " << ins.result << " = " << ins.left
                      << "   ; const-folded from " << ins.right << "\n";
        else if (ins.op == "=")
            std::cout << "    " << ins.result << " = " << ins.left << "\n";
        else
            std::cout << "    " << ins.result << " = "
                      << ins.left << " " << ins.op << " " << ins.right << "\n";
    }
    std::cout << "  Constant folding eliminated the runtime '+' and '*':\n";
    std::cout << "    2 + 3 * 4  →  2 + 12  →  14  (computed at compile time)\n";

    // ══ Stage 7: Code Generation ══════════════════════════════════════════════
    std::cout << "\n── Stage 7: Code Generation (x86-64) ──\n";
    stage7_codegen();
    std::cout << "  Key CPU concepts visible in the output:\n";
    std::cout << "    movl  $14, %eax        — load immediate 14 into 32-bit register eax\n";
    std::cout << "    movl  %eax, -4(%rbp)   — store to stack slot at [frame_pointer - 4]\n";
    std::cout << "    Because of folding, zero arithmetic instructions are needed.\n";

    // ══ Stages 8–9: Assembler & Linker (conceptual) ══════════════════════════
    std::cout << "\n── Stages 8–9: Assembler + Linker (conceptual) ──\n";
    std::cout << "  Assembler (as):\n";
    std::cout << "    Reads the .s text above and writes an ELF .o (object file).\n";
    std::cout << "    Each instruction becomes 1-15 bytes of machine code.\n";
    std::cout << "    movl $14, %eax  →  b8 0e 00 00 00  (5 bytes: opcode + imm32)\n";
    std::cout << "    movl %eax,-4(%rbp) → 89 45 fc       (3 bytes: opcode + ModRM + disp8)\n";
    std::cout << "\n  Linker (ld / gold / lld):\n";
    std::cout << "    Combines .o files, resolves symbol references (printf, malloc, …),\n";
    std::cout << "    patches relocations, and writes the final ELF executable.\n";
    std::cout << "    The OS loader then maps that ELF into virtual memory and calls main().\n";

    std::cout << "\n  Full pipeline done for:  int result = 2 + 3 * 4;\n";
    std::cout << "  Compile-time answer:     result = 14\n";
}

// ─────────────────────────────────────────────────────────────────────────────
int main() {
    // std::hex integers: use noshowbase + manual "0x" for clean output,
    // since pointer operator<< already adds "0x" automatically.

    section1();
    section2();
    section3();
    section4();
    section5();
    section6();
    section7();
    section8();

    std::cout << "\nDone.\n";
    return 0;
}
