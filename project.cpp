#include <iostream>
#include <cstdint>      // uintptr_t
#include <cstring>      // memset
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <memory>
#include <cassert>

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

    std::cout << "\nDone.\n";
    return 0;
}
