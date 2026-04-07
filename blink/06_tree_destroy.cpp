/*
================================================================================
BLINK DOM LEARNING PROJECT — FILE 06
Topic: Who owns memory? Stack vs heap. What happens when objects die.

================================================================================
BACKGROUND: TWO PLACES OBJECTS CAN LIVE
================================================================================

Every object in C++ lives in exactly one of two places:

STACK
-----
    BlinkElement div("div", "container");

The object's bytes occupy a frame on the call stack. The compiler knows its
size at compile time and reserves that many bytes automatically. The object is
destroyed — its destructor called, its bytes released — when execution leaves
the enclosing scope (the closing brace of the block it was declared in).
You do not call delete. You cannot control when it dies. It dies when the
scope ends, period.

HEAP
----
    BlinkElement* div = new BlinkElement("div", "container");

`new` asks the OS for a block of memory of size sizeof(BlinkElement). It
constructs the object there and returns its address. The object lives until
you call:

    delete div;

If you never call delete, the object never dies during the program's lifetime.
The memory is returned to the OS when the process exits, but the destructor
is never called — any resources the object holds (open files, other heap
allocations) are not cleaned up. This is a memory leak.

================================================================================
WHO OWNS THE OBJECT?
================================================================================

"Ownership" means: who is responsible for calling delete?

A raw pointer (BlinkElement*) does not encode ownership. It is just an address.
Storing a BlinkElement* in a vector does not make the vector the owner. The
vector will destroy itself (and the 8-byte address slots it holds), but it will
NOT call delete on the objects those addresses point to.

This means: if you build a tree of heap-allocated nodes and delete the root
without first deleting the children, the children keep living in heap memory
with no pointer to reach them. That is a memory leak.

The correct destruction order for a heap tree is POST-ORDER (same as file 05):
delete children first, then the parent. If you delete the parent first, you
lose the only pointers to the children.

================================================================================
STACK DESTRUCTION ORDER
================================================================================

Stack objects are destroyed in REVERSE DECLARATION ORDER within a scope.
If you declare:

    BlinkElement a(...);
    BlinkElement b(...);
    BlinkElement c(...);

They are destroyed as: c, then b, then a.

This matters because if b.parent == &a, then when b is destroyed, a is still
alive — no dangling pointer. But if a.parent == &c, then when a is destroyed,
c is already gone — a.parent is a dangling pointer (even though you never use
it, its mere existence is technically undefined behavior in C++).

================================================================================
WHAT A DESTRUCTOR ACTUALLY DOES
================================================================================

When `delete ptr` is called (or a stack object goes out of scope):
  1. The destructor body runs (the code you write inside ~BlinkElement()).
  2. The destructors of all member fields run, in reverse declaration order.
  3. The memory is returned to the allocator.

For a BlinkElement, step 2 means:
  - ~children  runs: the vector's destructor frees its heap buffer
                     (the 8-byte address slots), but NOT the objects
                     those addresses point to
  - ~parent    runs: nothing — a raw pointer has no destructor
  - ~id        runs: the string frees its heap character data
  - ~tag_name  runs: the string frees its heap character data

================================================================================
THIS FILE'S PROGRAM
================================================================================

We run three scenarios:

SCENARIO A — Stack tree, correct destruction (automatic, reverse order)
SCENARIO B — Heap tree, correct destruction (manual post-order delete)
SCENARIO C — Heap tree, wrong destruction (delete root first — leak + dangling)

Each scenario uses a fresh scope ({}) so destructors fire visibly.

================================================================================
*/

#include <iostream>
#include <string>
#include <vector>

// Global counter so we can detect leaks: constructed - destroyed should == 0
static int live_count = 0;

class BlinkElement {
public:
    std::string                 tag_name;
    std::string                 id;
    BlinkElement*               parent;
    std::vector<BlinkElement*>  children;

    BlinkElement(const std::string& tag, const std::string& element_id)
        : tag_name(tag), id(element_id), parent(nullptr)
    {
        ++live_count;
        std::cout << "  [+] <" << tag_name << " id=\"" << id << "\">  @" << this
                  << "  (live=" << live_count << ")\n";
    }

    ~BlinkElement() {
        --live_count;
        std::cout << "  [-] <" << tag_name << " id=\"" << id << "\">  @" << this
                  << "  (live=" << live_count << ")\n";
    }

    void appendChild(BlinkElement* child) {
        children.push_back(child);
        child->parent = this;
    }
};

// Post-order delete: children first, then self.
// This is the only safe order for heap trees with raw pointers.
void destroyTree(BlinkElement* node) {
    // copy the list first — we'll be deleting children, invalidating the vector
    std::vector<BlinkElement*> kids = node->children;
    for (BlinkElement* child : kids) {
        destroyTree(child);
    }
    delete node;
}

int main() {

    // ================================================================
    std::cout << "=== SCENARIO A: stack tree — automatic destruction ===\n";
    {
        BlinkElement root ("html", "root");
        BlinkElement head ("head", "");
        BlinkElement body ("body", "");
        BlinkElement child("div",  "container");

        root.appendChild(&head);
        root.appendChild(&body);
        body.appendChild(&child);

        std::cout << "  --- end of scope, destructors fire in reverse declaration order ---\n";
        // child dies first, then body, then head, then root
        // When root dies, its children vector is destroyed — the vector's heap
        // buffer (the 8-byte address slots) is freed, but head/body/child are
        // already gone by then anyway (stack, reverse order)
    }
    std::cout << "live_count after scenario A : " << live_count << "  (should be 0)\n\n";

    // ================================================================
    std::cout << "=== SCENARIO B: heap tree — correct post-order delete ===\n";
    {
        BlinkElement* root  = new BlinkElement("html", "root");
        BlinkElement* head  = new BlinkElement("head", "");
        BlinkElement* body  = new BlinkElement("body", "");
        BlinkElement* child = new BlinkElement("div",  "container");

        root->appendChild(head);
        root->appendChild(body);
        body->appendChild(child);

        std::cout << "  --- calling destroyTree(root) ---\n";
        destroyTree(root);   // post-order: child, body, head, root
        // root, head, body, child are now all deleted — do NOT use these pointers again
    }
    std::cout << "live_count after scenario B : " << live_count << "  (should be 0)\n\n";

    // ================================================================
    std::cout << "=== SCENARIO C: heap tree — WRONG: delete root first ===\n";
    {
        BlinkElement* root  = new BlinkElement("html", "root");
        BlinkElement* head  = new BlinkElement("head", "");
        BlinkElement* body  = new BlinkElement("body", "");

        root->appendChild(head);
        root->appendChild(body);

        std::cout << "  --- deleting root WITHOUT deleting children first ---\n";
        delete root;
        // root's destructor ran. The children vector inside root was destroyed,
        // freeing the 8-byte address slots. But head and body themselves were
        // NOT deleted. They are now unreachable heap memory (memory leak).
        // head and body still have parent == root, which is now a dangling pointer.
        // We cannot reach head or body anymore through root — those pointers are gone.
        // We still hold local pointers head and body, so we CAN still delete them,
        // but in a real program you might not have them.

        std::cout << "  --- deleting head and body manually to recover ---\n";
        delete head;
        delete body;
    }
    std::cout << "live_count after scenario C : " << live_count << "  (should be 0)\n\n";

    // ================================================================
    std::cout << "=== SCENARIO D: heap tree — leak with no recovery ===\n";
    std::cout << "  (we simulate, but do NOT actually leak — we just explain)\n";
    {
        // In a real leak scenario you would do:
        //   BlinkElement* root = new BlinkElement("html", "root");
        //   BlinkElement* head = new BlinkElement("head", "");
        //   root->appendChild(head);
        //   delete root;          // head is now unreachable
        //   // head is never deleted — 96 bytes leaked per node
        //   // live_count would be 1, not 0, at end of program

        // We build the tree and use destroyTree to keep live_count clean.
        BlinkElement* root = new BlinkElement("html", "root");
        BlinkElement* head = new BlinkElement("head", "");
        root->appendChild(head);
        std::cout << "  live_count with 2 heap nodes alive : " << live_count << "\n";
        std::cout << "  if we only deleted root here, live_count would stay at 1\n";
        std::cout << "  --- correctly destroying ---\n";
        destroyTree(root);
    }
    std::cout << "live_count after scenario D : " << live_count << "  (should be 0)\n\n";

    // ================================================================
    std::cout << "=== destructor field order (reverse of declaration) ===\n";
    std::cout << "BlinkElement field declaration order:\n";
    std::cout << "  1. tag_name  (std::string)\n";
    std::cout << "  2. id        (std::string)\n";
    std::cout << "  3. parent    (raw pointer — no destructor)\n";
    std::cout << "  4. children  (std::vector)\n";
    std::cout << "Destructor runs field destructors in reverse: children, parent (noop), id, tag_name\n";
    std::cout << "The children vector frees its internal heap buffer (the address slots),\n";
    std::cout << "but does NOT call delete on the BlinkElement objects those addresses point to.\n";

    return 0;
}

/*
================================================================================
EXERCISE
================================================================================

1. Write a function:

       BlinkElement* buildTree();

   It allocates the following tree entirely on the heap and returns the root:

       html
       ├── head
       │   └── title
       └── body
           └── div  id="container"
               └── p  id="intro"

   In main, call buildTree(), use findById (copy it from file 05) to find
   "intro", print its tag and parent's tag, then call destroyTree() on the
   root. Confirm live_count returns to 0.

2. Deliberately cause a leak to see it:
   Build the same tree. Delete only the root (not the full tree). Print
   live_count — it should be 5, not 0, because the 5 children were never
   destroyed. Then recover by calling destroyTree on each remaining pointer
   you still hold.

3. Observation (answer as a comment):
   In Scenario A, the stack destructor order is: child, body, head, root.
   root.children still holds the addresses of head and body at the moment
   root's destructor runs. But head and body have already been destroyed by
   then. Is root.children[0] a dangling pointer at that moment?
   Does it matter, given that root's destructor does not dereference it?
   What rule does this suggest about what a destructor should NOT do?

Compile and run:
    g++ -std=c++17 06_tree_destroy.cpp -o out && ./out

================================================================================
*/
