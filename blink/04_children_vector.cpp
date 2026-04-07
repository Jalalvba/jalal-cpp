/*
================================================================================
BLINK DOM LEARNING PROJECT — FILE 04
Topic: std::vector of child pointers. appendChild.

================================================================================
BACKGROUND: THE CHILDREN LIST
================================================================================

File 03 gave each element a pointer to its parent. That lets you walk UP the
tree. To walk DOWN — from a parent to its children — each element also needs
a list of pointers to the objects below it.

In real Chromium, `blink::Node` stores children as a doubly-linked list
(first_child_ / last_child_ / next_sibling_ / previous_sibling_ pointers).
We use std::vector<BlinkElement*> because it is easier to index and iterate,
and it teaches the same ownership concept.

================================================================================
WHAT std::vector<BlinkElement*> IS IN RAM
================================================================================

A std::vector<T> object (the vector itself, stored inside BlinkElement) is
exactly 24 bytes on a 64-bit system:

    [ptr to heap buffer]  8 bytes   — points to the array of T values
    [size]                8 bytes   — how many elements are currently stored
    [capacity]            8 bytes   — how many elements fit before reallocation

When you push_back a pointer, the vector stores that 8-byte address into its
heap buffer. The heap buffer grows automatically when size == capacity.

So children[0] is not a BlinkElement — it is an 8-byte value inside the
vector's heap buffer. That value is the RAM address of a BlinkElement
somewhere else in memory.

Visually:

    [parent object in memory]
    ┌──────────────────────────────────┐
    │ tag_name   (32 B)                │
    │ id         (32 B)                │
    │ parent*    ( 8 B)  ──────────────┼──▶ grandparent object
    │ children:                        │
    │   heap ptr ( 8 B)  ──────────────┼──▶ [heap buffer]
    │   size     ( 8 B) = 2            │        [0]: addr of child A ──▶ child A object
    │   capacity ( 8 B) = 2            │        [1]: addr of child B ──▶ child B object
    └──────────────────────────────────┘

================================================================================
appendChild: WHAT IT DOES IN RAM
================================================================================

appendChild(child) must do two things to keep the tree consistent:

  1. Store child's address into parent's children vector.
  2. Store parent's address into child's parent pointer.

After both steps, you can navigate in either direction — up via parent,
down via children[i].

If you only do step 1 (or only step 2), the tree is inconsistent:
one direction works, the other does not.

================================================================================
OBJECT LIFETIME AND POINTERS
================================================================================

The vector stores raw pointers. It does NOT own the objects. If a child
object is destroyed while the parent's children vector still holds its
address, that entry becomes a dangling pointer — an address pointing at
memory that is no longer a live object. Accessing it is undefined behavior.

In this file, all objects live on the stack for the same duration, so no
dangling pointers occur. File 06 covers what happens when objects die in
different orders.

================================================================================
THIS FILE'S PROGRAM
================================================================================

We build a small tree:

    html (root)
    ├── head
    └── body

Then we inspect:
  - sizeof the vector field
  - the numeric contents of children[0] vs &head
  - navigation: root.children[1]->id, then back up via parent
  - iteration over all children

================================================================================
*/

#include <iostream>
#include <string>
#include <vector>
#include <cstdint>   // uintptr_t

class BlinkElement {
public:
    std::string                   tag_name;
    std::string                   id;
    BlinkElement*                 parent;
    std::vector<BlinkElement*>    children;

    BlinkElement(const std::string& tag, const std::string& element_id)
        : tag_name(tag), id(element_id), parent(nullptr)
    {
        std::cout << "[constructor] <" << tag_name << " id=\"" << id << "\">  @" << this << "\n";
    }

    ~BlinkElement() {
        std::cout << "[destructor]  <" << tag_name << " id=\"" << id << "\">  @" << this << "\n";
    }

    void appendChild(BlinkElement* child) {
        children.push_back(child);   // step 1: parent knows about child
        child->parent = this;        // step 2: child knows about parent
    }
};

int main() {
    std::cout << "=== sizeof ===\n";
    std::cout << "sizeof(BlinkElement)                  : " << sizeof(BlinkElement) << " bytes\n";
    std::cout << "sizeof(std::vector<BlinkElement*>)    : " << sizeof(std::vector<BlinkElement*>) << " bytes\n";
    std::cout << "sizeof(BlinkElement*)                 : " << sizeof(BlinkElement*) << " bytes\n";
    std::cout << "\n";

    // -----------------------------------------------------------------
    std::cout << "=== build tree ===\n";
    BlinkElement root("html", "root");
    BlinkElement head("head", "head");
    BlinkElement body("body", "body");
    std::cout << "\n";

    root.appendChild(&head);
    root.appendChild(&body);
    std::cout << "appendChild called twice\n";
    std::cout << "\n";

    // -----------------------------------------------------------------
    std::cout << "=== vector state after appendChild ===\n";
    std::cout << "root.children.size()     : " << root.children.size()    << "\n";
    std::cout << "root.children.capacity() : " << root.children.capacity() << "\n";
    std::cout << "\n";

    // -----------------------------------------------------------------
    std::cout << "=== children[0] is an address, not an object ===\n";
    std::cout << "&head                    : " << &head              << "\n";
    std::cout << "root.children[0]         : " << root.children[0]  << "\n";
    std::cout << "same?                    : " << (&head == root.children[0] ? "yes" : "no") << "\n";

    // The vector's heap buffer holds raw 8-byte addresses
    uintptr_t* buf = reinterpret_cast<uintptr_t*>(root.children.data());
    std::cout << "children.data()          : " << root.children.data() << "  (heap buffer address)\n";
    std::cout << "buf[0] as hex            : 0x" << std::hex << buf[0] << std::dec << "\n";
    std::cout << "&head as hex             : 0x" << std::hex << reinterpret_cast<uintptr_t>(&head) << std::dec << "\n";
    std::cout << "\n";

    // -----------------------------------------------------------------
    std::cout << "=== navigate down then back up ===\n";
    BlinkElement* first  = root.children[0];
    BlinkElement* second = root.children[1];
    std::cout << "first child tag          : " << first->tag_name  << "\n";
    std::cout << "second child tag         : " << second->tag_name << "\n";
    std::cout << "second->parent->tag_name : " << second->parent->tag_name << "\n";
    std::cout << "back at root?            : " << (second->parent == &root ? "yes" : "no") << "\n";
    std::cout << "\n";

    // -----------------------------------------------------------------
    std::cout << "=== iterate all children ===\n";
    for (int i = 0; i < static_cast<int>(root.children.size()); ++i) {
        BlinkElement* c = root.children[i];
        std::cout << "  children[" << i << "] : <" << c->tag_name << " id=\"" << c->id << "\">  @" << c << "\n";
    }
    std::cout << "\n";

    // -----------------------------------------------------------------
    std::cout << "=== head and body have no children yet ===\n";
    std::cout << "head.children.size() : " << head.children.size() << "\n";
    std::cout << "body.children.size() : " << body.children.size() << "\n";
    std::cout << "\n";

    std::cout << "=== end of main — destructors fire in reverse declaration order ===\n";
    return 0;
}

/*
================================================================================
EXERCISE
================================================================================

Extend the tree to match this HTML structure:

    html (root)
    ├── head
    └── body
        ├── div  id="container"
        └── p    id="intro"

1. Create two more BlinkElement objects: div ("div", "container") and
   para ("p", "intro").

2. Call body.appendChild(&div) and body.appendChild(&para).

3. Print:
     - body.children.size()         — should be 2
     - body.children[0]->tag_name   — should be "div"
     - body.children[1]->parent->id — should be "body"

4. Write a loop that iterates root.children and for each child, prints
   that child's id AND how many children it has. Expected output:
       head  : 0 children
       body  : 2 children

5. Observation (answer as a comment):
   root.children.data() is the address of the vector's heap buffer.
   Each slot in that buffer is sizeof(BlinkElement*) = 8 bytes.
   What is the address of the second slot (children[1]) in the buffer?
   Express it as: children.data() + 1 (pointer arithmetic) and verify by
   printing both (void*)&root.children[1] and (void*)(root.children.data()+1).

Compile and run:
    g++ -std=c++17 04_children_vector.cpp -o out && ./out

================================================================================
*/
