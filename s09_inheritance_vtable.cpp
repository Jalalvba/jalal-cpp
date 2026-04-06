#include <iostream>
#include <cstdint>   // uintptr_t
#include <cstring>   // memcpy

// ─────────────────────────────────────────────────────────────────────────────
// Section 09 — Inheritance and the Vtable
//
// Competence: build a class hierarchy, explain vtable dispatch physically,
// and read the vtable pointer directly from the first 8 bytes of an object.
//
// What the compiler generates for a class with virtual methods:
//
//   1. A vtable (virtual dispatch table) — a static array of function pointers
//      living in the read-only data segment (.rodata).  One table per class.
//
//   2. A vptr — a hidden pointer at byte offset 0 of every object, written by
//      the constructor.  It points to the class's vtable.
//
// Object memory layout (single inheritance, 64-bit):
//
//   Node object:               Element object:
//   ┌─────────────────┐        ┌─────────────────┐
//   │ vptr [8 bytes]  │───┐    │ vptr [8 bytes]  │───┐
//   ├─────────────────┤   │    ├─────────────────┤   │
//   │ (Node fields)   │   │    │ (Node fields)   │   │
//   └─────────────────┘   │    ├─────────────────┤   │
//                          │    │ (Element fields)│   │
//                          │    └─────────────────┘   │
//                          │                           │
//                          ▼                           ▼
//                    Node::vtable             Element::vtable
//                    ┌────────────┐           ┌────────────┐
//                    │ ~Node      │           │ ~Element   │
//                    │ nodeType() │           │ nodeType() │ ← returns 1
//                    │ nodeName() │           │ nodeName() │ ← returns "Element"
//                    └────────────┘           └────────────┘
//
// Chromium note: BlinkNode → BlinkElement is this exact pattern.  Every DOM
// node's first 8 bytes are a vtable pointer.  When Blink calls
// node->nodeType() on a Node* that actually points to an Element, the CPU
// reads the vptr, indexes into the vtable, and jumps to Element::nodeType().
// No if/else.  No type tag checked at runtime.  One pointer dereference.
// ─────────────────────────────────────────────────────────────────────────────

// ── Helper: read the vtable pointer from any polymorphic object ───────────────
// The vptr is always at byte offset 0.  We cannot legally dereference a void**
// alias of a polymorphic object without memcpy (strict-aliasing rule), so we
// copy the first sizeof(void*) bytes into a separate variable.
static void* read_vptr(const void* obj) {
    void* vptr;
    std::memcpy(&vptr, obj, sizeof(void*));
    return vptr;
}

// ─────────────────────────────────────────────────────────────────────────────
class Node {
public:
    Node() {
        std::cout << "    Node()     constructed  @ " << static_cast<void*>(this) << "\n";
    }

    // Virtual destructor — without this, `delete base_ptr` would only call
    // ~Node() and leak Element's resources.  This is the most common vtable bug.
    virtual ~Node() {
        std::cout << "    ~Node()    destroyed     @ " << static_cast<void*>(this) << "\n";
    }

    virtual int         nodeType() const { return 0; }
    virtual const char* nodeName() const { return "Node"; }
};

// ─────────────────────────────────────────────────────────────────────────────
class Element : public Node {
public:
    Element() {
        // Node() runs first (base subobject), then Element() runs.
        std::cout << "    Element()  constructed  @ " << static_cast<void*>(this) << "\n";
    }

    // ~Element() runs first, then ~Node() runs automatically.
    ~Element() override {
        std::cout << "    ~Element() destroyed     @ " << static_cast<void*>(this) << "\n";
    }

    int         nodeType() const override { return 1; }
    const char* nodeName() const override { return "Element"; }
};

// ─────────────────────────────────────────────────────────────────────────────
int main() {
    std::cout << "══════════════════════════════════════════════\n";
    std::cout << "Section 09 — Inheritance and the Vtable\n";
    std::cout << "══════════════════════════════════════════════\n";

    // ── 1. Allocate both types on the heap ────────────────────────────────────
    std::cout << "\n── 1. Allocate Node and Element on the heap ──\n";
    std::cout << "  new Node():\n";
    Node* node_ptr = new Node();

    std::cout << "  new Element():\n";
    Node* elem_as_node = new Element();   // stored as Node* — this is the key move

    // ── 2. Print vtable pointers ──────────────────────────────────────────────
    // The first 8 bytes of every polymorphic object are the vptr.
    // Two objects of different classes have different vptrs even if they share
    // the same base: each class gets its own vtable at link time.
    std::cout << "\n── 2. Vtable pointer at byte offset 0 of each object ──\n";

    void* node_vptr = read_vptr(node_ptr);
    void* elem_vptr = read_vptr(elem_as_node);

    std::cout << "  Node    object  @ " << static_cast<void*>(node_ptr)    << "\n";
    std::cout << "  Node    vptr    = " << node_vptr                        << "  ← points to Node's vtable\n";
    std::cout << "  Element object  @ " << static_cast<void*>(elem_as_node) << "\n";
    std::cout << "  Element vptr    = " << elem_vptr                        << "  ← points to Element's vtable\n";
    std::cout << "  vptrs differ?     "
              << (node_vptr != elem_vptr
                    ? "YES — different vtables, so different dispatch"
                    : "NO  — something is wrong")
              << "\n";

    // ── 3. Virtual dispatch through a base pointer ────────────────────────────
    // elem_as_node is declared Node* but actually points to an Element object.
    // When we call nodeType(), the CPU:
    //   a) reads vptr  = *(elem_as_node)           → Element's vtable
    //   b) reads fn    = vtable[slot_for_nodeType]  → &Element::nodeType
    //   c) calls fn(elem_as_node)                   → returns 1
    // The declared type (Node*) is irrelevant at runtime.
    std::cout << "\n── 3. Virtual dispatch through Node* pointing at an Element ──\n";
    std::cout << "  node_ptr->nodeType()     = " << node_ptr->nodeType()
              << "  (Node::nodeType, returns 0)\n";
    std::cout << "  elem_as_node->nodeType() = " << elem_as_node->nodeType()
              << "  (Element::nodeType via vtable, returns 1)\n";
    std::cout << "  elem_as_node->nodeName() = \""
              << elem_as_node->nodeName() << "\""
              << "  (Element::nodeName via vtable)\n";

    // ── 4. dynamic_cast ───────────────────────────────────────────────────────
    // dynamic_cast checks the RTTI block (stored just before the vtable) to
    // verify the runtime type before adjusting the pointer.
    // With single public inheritance and no virtual bases the addresses are
    // identical — the Element subobject starts at the same byte as the Node
    // subobject because Node is first in memory.
    // With multiple inheritance or virtual bases an offset would be added.
    std::cout << "\n── 4. dynamic_cast<Element*> ──\n";
    Element* elem_ptr = dynamic_cast<Element*>(elem_as_node);

    std::cout << "  elem_as_node (Node*)    = " << static_cast<void*>(elem_as_node) << "\n";
    std::cout << "  elem_ptr     (Element*) = " << static_cast<void*>(elem_ptr)     << "\n";

    uintptr_t a = reinterpret_cast<uintptr_t>(elem_as_node);
    uintptr_t b = reinterpret_cast<uintptr_t>(elem_ptr);
    std::cout << "  address offset          = " << static_cast<std::ptrdiff_t>(b - a)
              << " bytes  (0 with single inheritance — same base address)\n";
    std::cout << "  cast succeeded?           "
              << (elem_ptr != nullptr ? "YES" : "NO — would be nullptr on type mismatch") << "\n";

    // Demonstrate a failed cast: Node* pointing at a real Node cannot become Element*
    Element* bad = dynamic_cast<Element*>(node_ptr);
    std::cout << "  dynamic_cast<Element*>(node_ptr) = "
              << static_cast<void*>(bad)
              << "  (nullptr — node_ptr really is a Node, not an Element)\n";

    // ── 5. sizeof ─────────────────────────────────────────────────────────────
    std::cout << "\n── 5. sizeof ──\n";
    std::cout << "  sizeof(Node)    = " << sizeof(Node)
              << " bytes  (vptr[8] — no data fields declared)\n";
    std::cout << "  sizeof(Element) = " << sizeof(Element)
              << " bytes  (inherits Node layout, no extra fields declared)\n";
    std::cout << "  Both carry a vptr at offset 0 even though we never wrote 'void* vptr;'.\n";
    std::cout << "  The compiler inserts it automatically for any class with virtual methods.\n";

    // ── 6. delete through Node* — both destructors fire in correct order ──────
    // Because ~Node() is virtual, `delete node_ptr` looks up the vtable and
    // calls the most-derived destructor first.  For elem_as_node (an Element)
    // that means: ~Element() then ~Node().
    // If ~Node() were NOT virtual, only ~Node() would run — a resource leak.
    std::cout << "\n── 6. delete through Node* — vtable ensures correct destructor chain ──\n";
    std::cout << "  delete node_ptr;  (Node* → real Node)\n";
    delete node_ptr;

    std::cout << "  delete elem_as_node;  (Node* → real Element — both dtors must fire)\n";
    delete elem_as_node;

    return 0;
}
