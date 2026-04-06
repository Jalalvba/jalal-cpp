#include <iostream>
#include <cstdint>   // uintptr_t

// ─────────────────────────────────────────────────────────────────────────────
// Section 10 — Templates and Handle<T>
//
// Competence: write a generic wrapper class and connect it to V8's Local<T>.
//
// V8's Local<T> (simplified):
//
//   template <class T>
//   class Local {
//    public:
//     T* operator->() const { return val_; }
//     T* operator*()  const { return val_; }
//    private:
//     T* val_;   // ← the only data member: one pointer
//   };
//
// sizeof(Local<T>) == sizeof(void*) == 8 for every T on a 64-bit machine.
// The wrapper adds zero overhead over a raw pointer.
//
// Why wrap at all?  In real V8, Local<T> does not store a raw T* directly —
// it stores a pointer to a slot in a HandleScope stack.  The GC can move the
// heap object and update that slot; every Local<T> automatically sees the new
// address because it goes through the slot, not directly to the object.
//
// Our Handle<T> stores the raw pointer to keep the demo simple and show the
// structural equivalence.
//
// Chromium note: V8's Local<T> is this pattern. It wraps a raw pointer to a
// V8 heap object. The wrapper is 8 bytes — just the pointer. The GC can update
// the pointer inside the Handle when it moves objects, without the C++ code
// knowing.
// ─────────────────────────────────────────────────────────────────────────────

// ── Minimal class hierarchy (same roles as s09) ───────────────────────────────
class Node {
public:
    explicit Node(int id) : id_(id) {}
    virtual ~Node() = default;
    virtual int nodeType() const { return 0; }
    int id_;
};

class Element : public Node {
public:
    explicit Element(int id) : Node(id) {}
    int nodeType() const override { return 1; }
};

// ─────────────────────────────────────────────────────────────────────────────
// Handle<T> — a generic typed wrapper around a raw pointer.
//
// Template mechanics:
//   - The compiler generates a separate class body for each T used.
//   - Handle<Node> and Handle<Element> are distinct types with identical layout.
//   - Because Handle<T> has only one data member (T* ptr_), the compiler
//     guarantees sizeof(Handle<T>) == sizeof(T*) == 8 on 64-bit.
// ─────────────────────────────────────────────────────────────────────────────
template<typename T>
class Handle {
public:
    // Construct from a raw pointer (matches V8's Local<T>::New(isolate, ptr))
    explicit Handle(T* ptr) : ptr_(ptr) {}

    // Pointer operators — forward every member access to the wrapped object
    T* operator->() const { return ptr_; }
    T& operator*()  const { return *ptr_; }

    // Raw pointer access — used when a C API needs the underlying pointer
    T* get() const { return ptr_; }

    // Print the address this Handle holds (what V8 calls "the slot value")
    void print(const char* label) const {
        std::cout << "  Handle<" << label << ">"
                  << "  ptr_=" << static_cast<void*>(ptr_)
                  << "  &ptr_=" << static_cast<const void*>(&ptr_)
                  << "  sizeof=" << sizeof(*this) << " bytes\n";
    }

private:
    T* ptr_;   // sole data member — 8 bytes on 64-bit
};

// ─────────────────────────────────────────────────────────────────────────────
int main() {
    std::cout << "══════════════════════════════════════════════\n";
    std::cout << "Section 10 — Templates and Handle<T>\n";
    std::cout << "══════════════════════════════════════════════\n";

    // ── 1. Allocate objects ───────────────────────────────────────────────────
    Node*    raw_node = new Node(10);
    Element* raw_elem = new Element(20);

    // ── 2. Wrap in Handle<T> ──────────────────────────────────────────────────
    // The template is instantiated twice: once for T=Node, once for T=Element.
    // The generated code is structurally identical — only the type name differs.
    std::cout << "\n── 2. Wrapping raw pointers in Handle<T> ──\n";
    Handle<Node>    h_node(raw_node);
    Handle<Element> h_elem(raw_elem);

    h_node.print("Node   ");
    h_elem.print("Element");

    // ── 3. sizeof — wrapper is exactly one pointer ────────────────────────────
    std::cout << "\n── 3. sizeof ──\n";
    std::cout << "  sizeof(Node*)           = " << sizeof(Node*)           << " bytes\n";
    std::cout << "  sizeof(Element*)        = " << sizeof(Element*)        << " bytes\n";
    std::cout << "  sizeof(Handle<Node>)    = " << sizeof(Handle<Node>)    << " bytes\n";
    std::cout << "  sizeof(Handle<Element>) = " << sizeof(Handle<Element>) << " bytes\n";
    std::cout << "  All 8 — the wrapper adds zero size over a raw pointer.\n";

    // ── 4. Pointer compatibility — Handle<Node> wrapping an Element* ──────────
    // A raw Element* is implicitly convertible to Node* because Element is a
    // subclass of Node.  Handle<Node>(element_ptr) works for the same reason:
    // the constructor takes a T* (Node*), and Element* converts to Node*.
    //
    // This mirrors how V8 handles upcasting: Local<Value> can hold any JS value
    // (Number, String, Object…) because they all inherit from v8::Value.
    std::cout << "\n── 4. Handle<Node> wrapping an Element* (upcast) ──\n";
    Handle<Node> h_upcast(raw_elem);   // Element* → Node* implicit conversion
    h_upcast.print("Node←Elem");
    std::cout << "  raw_elem address    = " << static_cast<void*>(raw_elem)  << "\n";
    std::cout << "  h_upcast.get()      = " << static_cast<void*>(h_upcast.get()) << "\n";
    std::cout << "  same address?         "
              << (h_upcast.get() == static_cast<Node*>(raw_elem) ? "YES" : "NO") << "\n";

    // ── 5. operator->() dispatches through the vtable ─────────────────────────
    // h_upcast holds a Node*, but the object is really an Element.
    // operator->() returns that Node*, and the virtual call does the rest.
    std::cout << "\n── 5. operator->() + vtable dispatch ──\n";
    std::cout << "  h_node->nodeType()    = " << h_node->nodeType()
              << "  (Node::nodeType)\n";
    std::cout << "  h_elem->nodeType()    = " << h_elem->nodeType()
              << "  (Element::nodeType)\n";
    std::cout << "  h_upcast->nodeType()  = " << h_upcast->nodeType()
              << "  (Element::nodeType via vtable — Handle does not break dispatch)\n";

    // ── 6. Address inside Handle == address of raw pointer ────────────────────
    // The Handle is not a separate heap allocation.  It is a value type —
    // typically a local variable on the stack.  The pointer it stores is the
    // exact same address as the raw pointer passed to the constructor.
    std::cout << "\n── 6. Handle address vs raw pointer address ──\n";
    std::cout << "  raw_node              = " << static_cast<void*>(raw_node)   << "  (address of Node object on heap)\n";
    std::cout << "  h_node.get()          = " << static_cast<void*>(h_node.get()) << "  (value stored inside Handle)\n";
    std::cout << "  &h_node (Handle itself)= " << static_cast<const void*>(&h_node) << "  (Handle lives on the stack)\n";
    std::cout << "  identical value?        "
              << (h_node.get() == raw_node ? "YES — Handle stores the exact same address" : "NO") << "\n";

    // ── 7. operator*() — dereference the Handle like a pointer ───────────────
    std::cout << "\n── 7. operator*() ──\n";
    Node& ref_via_handle = *h_node;
    std::cout << "  (*h_node).id_         = " << ref_via_handle.id_ << "  (Node id=10)\n";
    std::cout << "  (*h_elem).id_         = " << (*h_elem).id_       << "  (Element id=20)\n";

    // ── 8. Cleanup ────────────────────────────────────────────────────────────
    // Handles themselves need no cleanup — they are value types on the stack.
    // The underlying objects were allocated with new and must be deleted.
    // In real V8, the GC deletes heap objects; in Blink, scoped_refptr does it.
    std::cout << "\n── 8. Cleanup ──\n";
    std::cout << "  Handles go out of scope automatically (stack variables).\n";
    std::cout << "  Deleting underlying heap objects...\n";
    delete raw_node;
    delete raw_elem;
    std::cout << "  Done.\n";

    return 0;
}
