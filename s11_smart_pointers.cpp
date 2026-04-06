#include <iostream>
#include <memory>    // unique_ptr, shared_ptr
#include <utility>   // std::move
#include <cstdint>   // uintptr_t

// ─────────────────────────────────────────────────────────────────────────────
// Section 11 — Smart Pointers and Ref Counting
//
// Competence: unique_ptr, shared_ptr, and a hand-rolled scoped_refptr.
//
// Why raw pointers are dangerous:
//   Node* n = new Node(1);
//   // ... exception thrown here ...
//   delete n;   // ← never reached; memory leaked
//
// Smart pointers solve this by tying the lifetime of the heap object to the
// lifetime of a stack variable.  When the stack variable is destroyed (scope
// exit, exception unwind, or return), the destructor runs automatically.
//
// Chromium note: Blink's scoped_refptr<T> is exactly section 3.  Every
// BlinkNode inherits from GarbageCollected or RefCounted.  When the last
// scoped_refptr goes out of scope, release() hits 0, delete fires, ~Node runs.
// This is how DOM nodes are destroyed in Chrome.
// ─────────────────────────────────────────────────────────────────────────────

class Node {
public:
    explicit Node(int id) : id_(id) {
        std::cout << "    Node(" << id_ << ") created    @ "
                  << static_cast<void*>(this) << "\n";
    }
    ~Node() {
        std::cout << "    ~Node(" << id_ << ") destroyed  @ "
                  << static_cast<void*>(this) << "\n";
    }
    int id_;
};

// ═════════════════════════════════════════════════════════════════════════════
// Section 3 support — minimal RefCounted base and scoped_refptr<T>
//
// This is a direct simplification of:
//   wtf/RefCounted.h       — the RefCounted<T> base class in Blink
//   wtf/scoped_refptr.h    — the smart pointer that manages it
// ═════════════════════════════════════════════════════════════════════════════

class RefCounted {
public:
    RefCounted() : ref_count_(0) {}

    void addRef() {
        ++ref_count_;
        std::cout << "    addRef()  → count now " << ref_count_ << "\n";
    }

    void release() {
        --ref_count_;
        std::cout << "    release() → count now " << ref_count_ << "\n";
        if (ref_count_ == 0) {
            std::cout << "    count hit 0 — calling delete this\n";
            delete this;   // fires the most-derived destructor via virtual dtor
        }
    }

    int refCount() const { return ref_count_; }

protected:
    // Protected so only delete-via-release() can destroy the object.
    // This matches Blink's pattern: you must not call `delete node` directly.
    virtual ~RefCounted() = default;

private:
    int ref_count_;
};

// scoped_refptr<T> — mirrors Blink's wtf/scoped_refptr.h
//
// Invariant: whenever scoped_refptr holds a non-null pointer, the object's
// ref count includes one count for this scoped_refptr.
template<typename T>
class scoped_refptr {
public:
    // Construct from raw pointer — takes ownership, increments count.
    explicit scoped_refptr(T* ptr = nullptr) : ptr_(ptr) {
        if (ptr_) ptr_->addRef();
    }

    // Copy — both instances own the object, so count increments.
    scoped_refptr(const scoped_refptr& other) : ptr_(other.ptr_) {
        if (ptr_) ptr_->addRef();
    }

    // Move — transfer ownership without touching the count.
    // The moved-from pointer becomes null, so its destructor is a no-op.
    scoped_refptr(scoped_refptr&& other) noexcept : ptr_(other.ptr_) {
        other.ptr_ = nullptr;
    }

    // Assignment — release old, acquire new.
    scoped_refptr& operator=(const scoped_refptr& other) {
        if (ptr_ != other.ptr_) {
            if (other.ptr_) other.ptr_->addRef();
            if (ptr_)       ptr_->release();
            ptr_ = other.ptr_;
        }
        return *this;
    }

    // Destructor — release our count; if we were the last owner, object dies.
    ~scoped_refptr() {
        if (ptr_) ptr_->release();
    }

    T* get()           const { return ptr_; }
    T* operator->()    const { return ptr_; }
    T& operator*()     const { return *ptr_; }
    explicit operator bool() const { return ptr_ != nullptr; }
    int use_count()    const { return ptr_ ? ptr_->refCount() : 0; }

private:
    T* ptr_;
};

// RefNode — a Node that also participates in ref counting
class RefNode : public RefCounted {
public:
    explicit RefNode(int id) : id_(id) {
        std::cout << "    RefNode(" << id_ << ") created    @ "
                  << static_cast<void*>(this) << "\n";
    }
    ~RefNode() override {
        std::cout << "    ~RefNode(" << id_ << ") destroyed  @ "
                  << static_cast<void*>(this) << "\n";
    }
    int id_;
};

// ─────────────────────────────────────────────────────────────────────────────
int main() {
    std::cout << "══════════════════════════════════════════════\n";
    std::cout << "Section 11 — Smart Pointers and Ref Counting\n";
    std::cout << "══════════════════════════════════════════════\n";

    // ── Section 1: unique_ptr ─────────────────────────────────────────────────
    // unique_ptr enforces *exclusive* ownership: exactly one unique_ptr owns
    // the object at any time.  It cannot be copied — only moved.
    std::cout << "\n──────────────────────────────────────────────\n";
    std::cout << "Section 1 — unique_ptr (exclusive ownership)\n";
    std::cout << "──────────────────────────────────────────────\n";

    std::cout << "\n  Creating unique_ptr<Node>...\n";
    std::unique_ptr<Node> u1 = std::make_unique<Node>(1);
    std::cout << "  u1.get()  = " << static_cast<void*>(u1.get()) << "\n";
    std::cout << "  sizeof(u1) = " << sizeof(u1) << " bytes (same as a raw pointer)\n";

    // Copying is deleted:
    //   std::unique_ptr<Node> u2 = u1;   // ← compile error: copy constructor is deleted
    // This enforces the single-owner guarantee at compile time.
    std::cout << "\n  // unique_ptr CANNOT be copied — compile error if you try:\n";
    std::cout << "  // unique_ptr<Node> u2 = u1;  ← deleted copy constructor\n";

    // Transfer ownership with std::move.  After the move, u1 is null.
    std::cout << "\n  Moving u1 → u2 with std::move...\n";
    std::unique_ptr<Node> u2 = std::move(u1);
    std::cout << "  u1.get() after move = " << static_cast<void*>(u1.get())
              << "  (null — u1 no longer owns anything)\n";
    std::cout << "  u2.get() after move = " << static_cast<void*>(u2.get())
              << "  (u2 now owns the Node)\n";

    std::cout << "\n  Entering inner block — u2 will go out of scope at closing brace:\n";
    {
        std::unique_ptr<Node> u3 = std::move(u2);
        std::cout << "  u3 owns Node(1) inside block  @ "
                  << static_cast<void*>(u3.get()) << "\n";
        std::cout << "  Leaving block — u3 destructor fires:\n";
    }
    std::cout << "  (back in outer scope — Node(1) is gone)\n";

    // ── Section 2: shared_ptr ─────────────────────────────────────────────────
    // shared_ptr allows *shared* ownership: multiple shared_ptrs can point to
    // the same object.  An internal ref count tracks how many owners exist.
    // The object is destroyed when the count reaches 0.
    std::cout << "\n──────────────────────────────────────────────\n";
    std::cout << "Section 2 — shared_ptr (shared ownership)\n";
    std::cout << "──────────────────────────────────────────────\n";

    std::shared_ptr<Node> s1;

    std::cout << "\n  Creating shared_ptr<Node>...\n";
    s1 = std::make_shared<Node>(2);
    std::cout << "  s1.get()       = " << static_cast<void*>(s1.get()) << "\n";
    std::cout << "  s1.use_count() = " << s1.use_count() << "  (one owner)\n";

    std::cout << "\n  Copying s1 → s2 inside a block...\n";
    {
        std::shared_ptr<Node> s2 = s1;   // copy — count goes to 2
        std::cout << "  s2.get()       = " << static_cast<void*>(s2.get()) << "\n";
        std::cout << "  s1.use_count() = " << s1.use_count() << "  (two owners: s1, s2)\n";
        std::cout << "  s2.use_count() = " << s2.use_count() << "  (same count, same object)\n";
        std::cout << "  s1.get()==s2.get(): "
                  << (s1.get() == s2.get() ? "YES — same Node object" : "NO") << "\n";
        std::cout << "  Leaving block — s2 destroyed, count decrements:\n";
    }
    std::cout << "  s1.use_count() = " << s1.use_count() << "  (one owner again, Node still alive)\n";

    std::cout << "\n  Letting s1 go out of scope in its own block:\n";
    {
        std::shared_ptr<Node> s_last = std::move(s1);
        std::cout << "  s_last.use_count() = " << s_last.use_count() << "\n";
        std::cout << "  Leaving block — last owner destroyed, count → 0:\n";
    }
    std::cout << "  (Node(2) is gone)\n";

    // ── Section 3: manual scoped_refptr ──────────────────────────────────────
    // scoped_refptr uses *intrusive* ref counting: the count lives inside the
    // object itself (in the RefCounted base class), not in a separate control
    // block like shared_ptr.  This is cheaper: one allocation instead of two,
    // and the count is always next to the data.
    //
    // Blink uses intrusive counting because DOM nodes are frequently shared
    // across many owners (the tree, pending callbacks, JavaScript wrappers).
    // The cost of a separate control block per-node would be significant.
    std::cout << "\n──────────────────────────────────────────────\n";
    std::cout << "Section 3 — scoped_refptr (Blink's pattern)\n";
    std::cout << "──────────────────────────────────────────────\n";

    std::cout << "\n  Creating scoped_refptr<RefNode>...\n";
    scoped_refptr<RefNode> r1(new RefNode(3));
    std::cout << "  r1.get()       = " << static_cast<void*>(r1.get()) << "\n";
    std::cout << "  r1.use_count() = " << r1.use_count() << "  (one owner)\n";

    std::cout << "\n  Copying r1 → r2 inside a block...\n";
    {
        scoped_refptr<RefNode> r2 = r1;   // copy constructor → addRef()
        std::cout << "  r2.get()       = " << static_cast<void*>(r2.get()) << "\n";
        std::cout << "  r1.use_count() = " << r1.use_count() << "  (two owners: r1, r2)\n";
        std::cout << "  Leaving block — r2 destructor calls release():\n";
    }
    std::cout << "  r1.use_count() = " << r1.use_count() << "  (one owner again)\n";

    std::cout << "\n  Letting r1 go out of scope in its own block:\n";
    {
        scoped_refptr<RefNode> r_last = std::move(r1);
        // std::move transfers ownership; r1 is null, r_last owns the object.
        // use_count stays at 1 — no addRef on move (we just transfer the slot).
        // NOTE: our simple scoped_refptr calls addRef then release on move,
        // which keeps the count correct (1→2→1).
        std::cout << "  r_last.use_count() = " << r_last.use_count() << "\n";
        std::cout << "  Leaving block — last scoped_refptr destroyed, release() fires:\n";
    }
    std::cout << "  (RefNode(3) is gone)\n";

    std::cout << "\n══════════════════════════════════════════════\n";
    std::cout << "Summary\n";
    std::cout << "══════════════════════════════════════════════\n";
    std::cout << "  unique_ptr  — one owner, zero overhead, non-copyable\n";
    std::cout << "  shared_ptr  — N owners, count in external control block\n";
    std::cout << "  scoped_refptr — N owners, count inside the object (intrusive)\n";
    std::cout << "  In all three cases: destructor fires automatically when count→0.\n";
    std::cout << "  No manual delete. No memory leak.\n";

    return 0;
}
