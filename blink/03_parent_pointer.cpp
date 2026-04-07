/*
================================================================================
BLINK DOM LEARNING PROJECT — FILE 03
Topic: What parent* means in RAM

================================================================================
BACKGROUND: THE PARENT POINTER
================================================================================

Every element in an HTML document (except the root) has exactly one parent.
In C++, "has a parent" means: the element object holds a pointer to another
element object.

A pointer is 8 bytes on a 64-bit system. It stores a RAM address — the
address of the first byte of the object it points to. That is all a pointer
is: an 8-byte integer holding an address.

When there is no parent (the element is the root), the pointer holds zero.
In C++ this is written nullptr. Its numeric value is 0x0000000000000000.

So the parent relationship in RAM looks like this:

    [child object]               [parent object]
    ┌───────────────────┐        ┌───────────────────┐
    │ tag_name  (32 B)  │        │ tag_name  (32 B)  │
    │ id        (32 B)  │        │ id        (32 B)  │
    │ parent*   ( 8 B)  │───────▶│ parent*   ( 8 B)  │
    └───────────────────┘        └───────────────────┘
       0x7fff...1000                0x7fff...2000

The arrow is not a metaphor. It means: bytes [64..71] of the child object
contain the integer 0x7fff...2000, which is the RAM address of the first byte
of the parent object.

================================================================================
WHAT nullptr MEANS PHYSICALLY
================================================================================

    BlinkElement* parent = nullptr;

This declares an 8-byte field whose value is 0. No object exists at address 0
(the OS protects that page). Dereferencing nullptr — writing parent->tag_name
when parent == nullptr — is undefined behavior and will crash (segfault) at
runtime.

Before using a pointer you must always check:

    if (parent != nullptr) { ... }

================================================================================
SETTING THE PARENT POINTER
================================================================================

When you write:

    child.parent = &root;

You are storing the address of `root` into the 8-byte field `child.parent`.
After that line, `child.parent` and `&root` hold the same integer value.
There is now one number in RAM (inside `child`) that lets you reach `root`.

You can then dereference it:

    child.parent->tag_name    // reads tag_name from the root object

The `->` operator means: take the address stored in the pointer, go to that
address in RAM, then access the named field at its offset from that address.

================================================================================
OWNERSHIP WARNING
================================================================================

A raw pointer does not own the object it points to. If `root` is destroyed
while `child.parent` still holds its address, `child.parent` becomes a
dangling pointer — an 8-byte integer that holds the address of memory that
no longer belongs to a live object. Reading through it is undefined behavior.

This file keeps both objects alive for the same duration (both on the stack
in main), so there is no dangling pointer here. File 06 covers ownership
and destruction order.

================================================================================
THIS FILE'S PROGRAM
================================================================================

We create two BlinkElements on the stack — a root and a child. We:
  - show the parent pointer value before and after assignment
  - show that child.parent and &root hold the same numeric address
  - navigate from child back to root via parent->
  - show what nullptr looks like as a number

================================================================================
*/

#include <iostream>
#include <string>
#include <cstdint>   // uintptr_t

class BlinkElement {
public:
    std::string    tag_name;
    std::string    id;
    BlinkElement*  parent;   // 8 bytes — holds an address or 0

    BlinkElement(const std::string& tag, const std::string& element_id)
        : tag_name(tag), id(element_id), parent(nullptr)
    {
        std::cout << "[constructor] <" << tag_name << " id=\"" << id << "\">  @" << this << "\n";
    }

    ~BlinkElement() {
        std::cout << "[destructor]  <" << tag_name << " id=\"" << id << "\">  @" << this << "\n";
    }
};

int main() {
    std::cout << "=== sizeof ===\n";
    std::cout << "sizeof(BlinkElement)   : " << sizeof(BlinkElement) << " bytes\n";
    std::cout << "sizeof(BlinkElement*)  : " << sizeof(BlinkElement*) << " bytes\n";
    std::cout << "\n";

    // -----------------------------------------------------------------
    std::cout << "=== create root and child ===\n";
    BlinkElement root("html", "root");
    BlinkElement child("div", "container");
    std::cout << "\n";

    // -----------------------------------------------------------------
    std::cout << "=== parent pointer before assignment ===\n";
    std::cout << "child.parent (raw)     : " << child.parent << "\n";
    std::cout << "child.parent as int    : " << reinterpret_cast<uintptr_t>(child.parent) << "\n";
    std::cout << "is null?               : " << (child.parent == nullptr ? "yes" : "no") << "\n";
    std::cout << "\n";

    // -----------------------------------------------------------------
    std::cout << "=== assign parent ===\n";
    child.parent = &root;
    std::cout << "&root                  : " << &root << "\n";
    std::cout << "child.parent           : " << child.parent << "\n";
    std::cout << "same address?          : " << (&root == child.parent ? "yes" : "no") << "\n";
    std::cout << "\n";

    // -----------------------------------------------------------------
    std::cout << "=== navigate from child to parent ===\n";
    std::cout << "child.parent->tag_name : " << child.parent->tag_name << "\n";
    std::cout << "child.parent->id       : " << child.parent->id       << "\n";

    // (*ptr).field is identical to ptr->field
    std::cout << "(*child.parent).id     : " << (*child.parent).id     << "\n";
    std::cout << "\n";

    // -----------------------------------------------------------------
    std::cout << "=== address arithmetic ===\n";
    uintptr_t child_base  = reinterpret_cast<uintptr_t>(&child);
    uintptr_t parent_field = reinterpret_cast<uintptr_t>(&child.parent);
    std::cout << "offset of parent field : " << (parent_field - child_base) << " bytes from child start\n";
    std::cout << "  (= sizeof(tag_name) + sizeof(id) = 32 + 32 = 64)\n";
    std::cout << "\n";

    // -----------------------------------------------------------------
    std::cout << "=== root's parent is still null ===\n";
    std::cout << "root.parent            : " << root.parent << "\n";
    std::cout << "is null?               : " << (root.parent == nullptr ? "yes" : "no") << "\n";
    std::cout << "\n";

    // -----------------------------------------------------------------
    std::cout << "=== guard against nullptr before use ===\n";
    auto printParentTag = [](const BlinkElement& el) {
        if (el.parent != nullptr) {
            std::cout << el.id << "'s parent tag : " << el.parent->tag_name << "\n";
        } else {
            std::cout << el.id << " has no parent (parent == nullptr)\n";
        }
    };

    printParentTag(child);
    printParentTag(root);
    std::cout << "\n";

    std::cout << "=== end of main — destructors fire in reverse order ===\n";
    return 0;
}

/*
================================================================================
EXERCISE
================================================================================

Add a second child element called `child2` with tag "p" and id "intro".

1. Set child2.parent = &root.
2. Print:
     - the address of child2
     - child2.parent (should equal &root)
     - child2.parent->id (should print "root")
3. Now set child2.parent = &child  (re-point to the div, not the html).
   Print child2.parent->tag_name before and after. Confirm the pointer
   value changed.
4. Observation (answer as a comment):
   After step 3, child2.parent points to `child`, but `child.parent` still
   points to `root`. Draw the chain on paper:
       child2 -> child -> root -> nullptr
   How many pointer dereferences would it take to reach root starting from
   child2?

Compile and run:
    g++ -std=c++17 03_parent_pointer.cpp -o out && ./out

================================================================================
*/
