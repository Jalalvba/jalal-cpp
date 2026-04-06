#include <iostream>
#include <string>
#include <vector>
#include <functional>
#include <cstdint>   // uintptr_t
#include <cstring>   // memcpy
#include <iomanip>   // hex, dec, setw

// ─────────────────────────────────────────────────────────────────────────────
// Section 13 — Chromium Convergence
//
// Everything from s08–s11 assembled into one Chromium-style renderer pipeline.
//
//   s08 → nodes allocated on the heap with new
//   s09 → class hierarchy, vtable pointer readable at offset 0 of every node
//   s10 → Handle<T> wraps raw pointers for temporary (V8-side) access
//   s11 → scoped_refptr<T> + RefCounted base manages lifetime automatically
//
// Real Chrome renderer process layout (simplified):
//
//   Blink (C++ DOM engine)          V8 (JS JIT engine)
//   ┌──────────────────┐            ┌────────────────────┐
//   │ BlinkNode        │◄──wrapper──│ Local<T> / Handle  │
//   │  refcount = N    │            │  (stack, 8 bytes)  │
//   │  vtable @ [0]    │            └────────────────────┘
//   │  parent/children │
//   └──────────────────┘
//         │  scoped_refptr keeps alive while JS holds a reference
//         ▼
//   When refcount → 0: ~BlinkNode() → PartitionAlloc frees the memory
//
// Chromium connection: this is the actual architecture of Chrome's renderer
// process. Blink owns the tree with ref counting; V8 holds temporary handles.
// When JS releases an object, the wrapper's persistent handle is cleared,
// scoped_refptr's count drops, and the DOM node is destroyed.
// ─────────────────────────────────────────────────────────────────────────────

// ═════════════════════════════════════════════════════════════════════════════
// RefCounted — s11 pattern. Blink equivalent: wtf/RefCounted.h
// Every BlinkNode inherits from this; its ref count lives inside the object
// (intrusive), not in a separate heap block like std::shared_ptr's.
// ═════════════════════════════════════════════════════════════════════════════
class RefCounted {
    mutable int ref_count_ = 0;
protected:
    virtual ~RefCounted() = default;
public:
    void addRef()  const { ++ref_count_; }
    void release() const { if (--ref_count_ == 0) delete this; }
    int  refCount() const { return ref_count_; }
};

// ═════════════════════════════════════════════════════════════════════════════
// scoped_refptr<T> — s11 pattern. Blink equivalent: wtf/scoped_refptr.h
// ═════════════════════════════════════════════════════════════════════════════
template<typename T>
class scoped_refptr {
    T* ptr_;
public:
    explicit scoped_refptr(T* p = nullptr) : ptr_(p)      { if (ptr_) ptr_->addRef(); }
    scoped_refptr(const scoped_refptr& o)  : ptr_(o.ptr_) { if (ptr_) ptr_->addRef(); }
    scoped_refptr(scoped_refptr&& o) noexcept : ptr_(o.ptr_) { o.ptr_ = nullptr; }
    ~scoped_refptr() { if (ptr_) ptr_->release(); }
    T* get()  const { return ptr_; }
    T* operator->() const { return ptr_; }
    int use_count() const { return ptr_ ? ptr_->refCount() : 0; }
};

// ═════════════════════════════════════════════════════════════════════════════
// Handle<T> — s10 pattern. V8 equivalent: v8::Local<T>
// A non-owning, stack-allocated wrapper around a raw pointer.
// sizeof(Handle<T>) == 8 on 64-bit for all T.
// ═════════════════════════════════════════════════════════════════════════════
template<typename T>
class Handle {
    T* ptr_;
public:
    explicit Handle(T* p = nullptr) : ptr_(p) {}
    T* get()  const { return ptr_; }
    T* operator->() const { return ptr_; }
    T& operator*()  const { return *ptr_; }
    explicit operator bool() const { return ptr_ != nullptr; }
};

// ═════════════════════════════════════════════════════════════════════════════
// BlinkNode — s09 pattern — base of the DOM tree.
//
// Inherits RefCounted so the same object is both a tree node (virtual dispatch)
// and a ref-counted resource (automatic lifetime).  In real Blink, Node
// inherits from GarbageCollected<Node> instead, but the vtable layout is
// identical: vptr occupies bytes [0,8) of every object.
// ═════════════════════════════════════════════════════════════════════════════
class BlinkElement;  // forward declaration for event types

class BlinkNode : public RefCounted {
public:
    virtual ~BlinkNode() = default;
    virtual int         nodeType() const = 0;
    virtual const char* nodeName() const = 0;

    BlinkNode*              parent_   = nullptr;
    std::vector<BlinkNode*> children_;                // raw, non-owning

    void appendChild(BlinkNode* child) {
        child->parent_ = this;
        children_.push_back(child);
    }
};

// ═════════════════════════════════════════════════════════════════════════════
// Event types
// ═════════════════════════════════════════════════════════════════════════════
struct BlinkEvent {
    std::string   type;
    BlinkElement* target          = nullptr;
    BlinkElement* currentTarget   = nullptr;
    bool          propagationStopped = false;
};

struct StoredListener {
    std::string           event_type;
    bool                  capture;
    std::string           fn_name;
    std::function<void()> fn;
};

// ═════════════════════════════════════════════════════════════════════════════
// BlinkElement — concrete DOM element (div, button, span …)
//
// Combines vtable dispatch (nodeType, nodeName overrides) with ref counting
// (inherited from BlinkNode → RefCounted) and event listener storage.
// ═════════════════════════════════════════════════════════════════════════════
class BlinkElement : public BlinkNode {
public:
    std::string                tag_;
    std::string                id_;
    std::vector<StoredListener> listeners_;

    BlinkElement(std::string tag, std::string id)
        : tag_(std::move(tag)), id_(std::move(id))
    {
        std::cout << "    BlinkElement(\"" << tag_ << "\", id=\"" << id_
                  << "\")  born  @ " << static_cast<void*>(this) << "\n";
    }

    ~BlinkElement() override {
        std::cout << "    ~BlinkElement(\"" << tag_ << "\", id=\"" << id_
                  << "\") died  @ " << static_cast<void*>(this) << "\n";
    }

    int         nodeType() const override { return 1; }
    const char* nodeName() const override { return tag_.c_str(); }

    std::string display() const {
        return "<" + tag_ + " id=\"" + id_ + "\">";
    }

    void addEventListener(std::string ev_type, bool capture,
                          std::string fn_name, std::function<void()> fn) {
        listeners_.push_back({ std::move(ev_type), capture,
                                std::move(fn_name), std::move(fn) });
    }
};

// ═════════════════════════════════════════════════════════════════════════════
// Helpers
// ═════════════════════════════════════════════════════════════════════════════

// Read the vtable pointer from any polymorphic object.
// The vptr is at byte offset 0; memcpy avoids strict-aliasing UB.
static void* read_vptr(const void* obj) {
    void* vptr;
    std::memcpy(&vptr, obj, sizeof(void*));
    return vptr;
}

// Simulate the V8-heap address of the wrapper object for a Blink node.
// In real Chrome this comes from blink::DOMDataStore (the WrapperMap).
static uintptr_t v8_wrapper_addr(const BlinkNode* node) {
    return reinterpret_cast<uintptr_t>(node) ^ 0xDEAD'0000ULL;
}

static void print_node_info(const char* label, BlinkElement* el, int refcount) {
    void* vptr = read_vptr(el);
    std::cout << "  " << label << " " << el->display() << "\n"
              << "    heap     @ " << static_cast<void*>(el) << "\n"
              << "    vtable   @ " << vptr
              << "  ← BlinkElement's vtable in .rodata\n"
              << "    refcount  = " << refcount << "\n";
}

// ═════════════════════════════════════════════════════════════════════════════
// Event dispatcher — DOM Level 3 capture / at-target / bubble algorithm
// ═════════════════════════════════════════════════════════════════════════════
static void fire_at(BlinkEvent& ev, BlinkElement* el, bool capture) {
    // ── Blink → V8 boundary crossing ────────────────────────────────────────
    // In real Chrome: V8::Function::Call(context, wrapper, 1, &event_wrapper)
    uintptr_t wrap = v8_wrapper_addr(el);
    std::cout << "    [Blink→V8] " << el->display()
              << "  node=0x"    << std::hex << reinterpret_cast<uintptr_t>(el)
              << "  wrapper=0x" << wrap     << std::dec
              << "  (" << (capture ? "capture" : "bubble") << ")\n";

    bool fired = false;
    for (auto& l : el->listeners_) {
        if (l.event_type != ev.type) continue;
        if (l.capture    != capture) continue;
        if (ev.propagationStopped)   break;
        ev.currentTarget = el;
        std::cout << "      JS: " << l.fn_name << "() — "
                  << "currentTarget=" << el->display() << "\n";
        l.fn();
        fired = true;
    }
    if (!fired)
        std::cout << "      (no " << (capture ? "capture" : "bubble")
                  << " listeners)\n";
}

static void dispatch_event(BlinkEvent& ev, BlinkElement* target) {
    ev.target = target;

    // Build ancestor chain: root first, immediate parent last
    std::vector<BlinkElement*> path;
    for (BlinkNode* n = target->parent_; n; n = n->parent_)
        if (auto* e = dynamic_cast<BlinkElement*>(n))
            path.insert(path.begin(), e);

    // ── Phase 1: CAPTURE  root → target ─────────────────────────────────────
    std::cout << "\n  ─── Phase 1: CAPTURE (root → target) ───\n";
    for (auto* ancestor : path) {
        if (ev.propagationStopped) break;
        fire_at(ev, ancestor, /*capture=*/true);
    }

    // ── Phase 2: AT-TARGET ──────────────────────────────────────────────────
    std::cout << "\n  ─── Phase 2: AT-TARGET (" << target->display() << ") ───\n";
    if (!ev.propagationStopped) {
        fire_at(ev, target, /*capture=*/true);   // capture handlers on target
        fire_at(ev, target, /*capture=*/false);  // bubble  handlers on target
    }

    // ── Phase 3: BUBBLE  target → root ──────────────────────────────────────
    std::cout << "\n  ─── Phase 3: BUBBLE (target → root) ───\n";
    for (auto it = path.rbegin(); it != path.rend(); ++it) {
        if (ev.propagationStopped) break;
        fire_at(ev, *it, /*capture=*/false);
    }
}

// ═════════════════════════════════════════════════════════════════════════════
int main() {
    std::cout << "══════════════════════════════════════════════\n";
    std::cout << "Section 13 — Chromium Convergence\n";
    std::cout << "══════════════════════════════════════════════\n";
    std::cout << "\n  Patterns in use:\n"
              << "    s08 → heap allocation with new\n"
              << "    s09 → vtable at byte offset 0 of every BlinkNode\n"
              << "    s10 → Handle<BlinkNode> wraps raw pointer (8 bytes)\n"
              << "    s11 → scoped_refptr<BlinkElement> manages lifetime\n";

    // ── 1. Allocate DOM tree on the heap ──────────────────────────────────────
    std::cout << "\n──────────────────────────────────────────────\n";
    std::cout << "1. Heap allocation — constructors fire\n";
    std::cout << "──────────────────────────────────────────────\n\n";

    // new allocates on the heap (s08).  scoped_refptr immediately addRefs to 1.
    scoped_refptr<BlinkElement> app_sr (new BlinkElement("div",    "app"));
    scoped_refptr<BlinkElement> cont_sr(new BlinkElement("div",    "container"));
    scoped_refptr<BlinkElement> btn_sr (new BlinkElement("button", "btn"));

    // Raw non-owning pointers for tree structure.  Ownership stays with scoped_reftrs.
    app_sr->appendChild(cont_sr.get());
    cont_sr->appendChild(btn_sr.get());

    std::cout << "\n  DOM tree built:\n"
              << "    " << app_sr->display()  << "\n"
              << "      " << cont_sr->display() << "\n"
              << "        " << btn_sr->display()  << "\n";

    // ── 2. Print addresses, vtable pointers, ref counts ───────────────────────
    std::cout << "\n──────────────────────────────────────────────\n";
    std::cout << "2. Addresses + vtable pointers + ref counts (s08 + s09 + s11)\n";
    std::cout << "──────────────────────────────────────────────\n\n";

    print_node_info("app_sr ", app_sr.get(),  app_sr.use_count());
    std::cout << "\n";
    print_node_info("cont_sr", cont_sr.get(), cont_sr.use_count());
    std::cout << "\n";
    print_node_info("btn_sr ", btn_sr.get(),  btn_sr.use_count());

    // All three BlinkElement objects share the same vtable (same class).
    std::cout << "\n  app and btn share the same vtable? "
              << (read_vptr(app_sr.get()) == read_vptr(btn_sr.get())
                    ? "YES — all BlinkElement objects point to one vtable in .rodata"
                    : "NO")
              << "\n";

    // ── 3. Handle<BlinkNode> — V8-side temporary references ──────────────────
    std::cout << "\n──────────────────────────────────────────────\n";
    std::cout << "3. Handle<BlinkNode> — V8-side wrappers (s10)\n";
    std::cout << "──────────────────────────────────────────────\n\n";

    Handle<BlinkNode> app_h (app_sr.get());
    Handle<BlinkNode> cont_h(cont_sr.get());
    Handle<BlinkNode> btn_h (btn_sr.get());

    // Handle is a stack variable (high address).  It holds a pointer to the
    // heap object (lower address).  It does NOT call addRef — it is not an owner.
    std::cout << "  btn_h:\n"
              << "    Handle itself     @ " << static_cast<void*>(&btn_h)
              << "  (stack)\n"
              << "    btn_h.get()        = " << static_cast<void*>(btn_h.get())
              << "  (heap — same as btn_sr.get())\n"
              << "    sizeof(btn_h)      = " << sizeof(btn_h)
              << " bytes  (just one pointer)\n"
              << "    btn_sr.use_count() = " << btn_sr.use_count()
              << "  (Handle does NOT addRef — non-owning)\n";

    // Virtual dispatch through Handle — operator->() returns raw ptr, vtable fires.
    std::cout << "\n  Virtual dispatch through Handle (s09 + s10):\n"
              << "    app_h->nodeType() = " << app_h->nodeType()
              << "  (vtable → BlinkElement::nodeType)\n"
              << "    btn_h->nodeName() = \"" << btn_h->nodeName()
              << "\"  (vtable → BlinkElement::nodeName)\n";

    // ── 4. Ref count lifecycle ─────────────────────────────────────────────────
    std::cout << "\n──────────────────────────────────────────────\n";
    std::cout << "4. Ref count lifecycle (s11)\n";
    std::cout << "──────────────────────────────────────────────\n\n";

    std::cout << "  btn_sr.use_count() = " << btn_sr.use_count()
              << "  (btn_sr is the sole owner)\n";

    std::cout << "\n  Simulating a JS variable holding a reference to btn:\n";
    {
        scoped_refptr<BlinkElement> js_ref = btn_sr;   // copy → addRef → count=2
        std::cout << "  btn_sr.use_count() = " << btn_sr.use_count()
                  << "  (btn_sr + js_ref both own it)\n";
        std::cout << "  js_ref.get()       = " << static_cast<void*>(js_ref.get())
                  << "  (same heap address)\n";
        std::cout << "  JS variable goes out of scope (GC collects the JS wrapper):\n";
    }   // js_ref → ~scoped_refptr → release() → count back to 1
    std::cout << "  btn_sr.use_count() = " << btn_sr.use_count()
              << "  (one owner — node still alive)\n";

    // ── 5. Register event listeners ───────────────────────────────────────────
    std::cout << "\n──────────────────────────────────────────────\n";
    std::cout << "5. Registering event listeners\n";
    std::cout << "──────────────────────────────────────────────\n\n";

    // Mirrors JS: element.addEventListener(type, handler, useCapture)
    app_sr->addEventListener("click", /*capture=*/false, "appBubble", []() {
        std::cout << "      [appBubble] <div id=app> caught the bubbling click\n";
    });
    cont_sr->addEventListener("click", /*capture=*/true, "containerCapture", []() {
        std::cout << "      [containerCapture] <div id=container> caught click in capture\n";
    });
    btn_sr->addEventListener("click", /*capture=*/false, "btnClick", []() {
        std::cout << "      [btnClick] <button id=btn> handled the click\n";
    });

    std::cout << "  app_sr listeners : " << app_sr->listeners_.size()  << "\n"
              << "  cont_sr listeners: " << cont_sr->listeners_.size() << "\n"
              << "  btn_sr listeners : " << btn_sr->listeners_.size()  << "\n";

    // ── 6. Event dispatch ──────────────────────────────────────────────────────
    std::cout << "\n──────────────────────────────────────────────\n";
    std::cout << "6. Event dispatch — 'click' on " << btn_sr->display() << "\n";
    std::cout << "──────────────────────────────────────────────\n";

    BlinkEvent ev{ "click" };
    dispatch_event(ev, btn_sr.get());

    std::cout << "\n  dispatch complete — propagationStopped="
              << (ev.propagationStopped ? "true" : "false") << "\n";

    // ── 7. Cleanup ─────────────────────────────────────────────────────────────
    // When main() returns, destructors fire in reverse declaration order:
    //   btn_h, cont_h, app_h  → Handle dtors, silent (non-owning)
    //   btn_sr                 → release() → refcount=0 → delete → ~BlinkElement
    //   cont_sr                → same
    //   app_sr                 → same
    std::cout << "\n──────────────────────────────────────────────\n";
    std::cout << "7. Cleanup — scoped_reftrs going out of scope\n";
    std::cout << "──────────────────────────────────────────────\n";
    std::cout << "\n  Handles destruct first (silent — non-owning, no release()).\n";
    std::cout << "  Then scoped_reftrs destruct in reverse order: btn → cont → app.\n";
    std::cout << "  Each release() hits 0, triggering delete and ~BlinkElement:\n\n";

    return 0;
    // destructors fire here — output appears after this line
}
