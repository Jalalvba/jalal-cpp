/*
================================================================================
BLINK DOM LEARNING PROJECT — FILE 01
Topic: What is a BlinkElement?

================================================================================
BACKGROUND: THE DOM IN RAM
================================================================================

When a browser parses an HTML file, it builds a tree of objects in RAM. Each
HTML tag becomes one C++ object. A <div> becomes a BlinkElement. A <span>
becomes a BlinkElement. Every attribute, every tag name, every id — they all
live somewhere in RAM as bytes inside these objects.

In real Chromium, the base class for all of this is `blink::Node`, and
`blink::Element` inherits from it. We are building a simplified version called
`BlinkElement` to understand what is physically happening.

================================================================================
WHAT IS AN OBJECT IN RAM?
================================================================================

When you write:

    BlinkElement div("div", "container");

The runtime calls `new` (or allocates on the stack) and reserves a contiguous
block of bytes large enough to hold all the fields of BlinkElement. The size of
that block is what `sizeof(BlinkElement)` returns.

Inside that block:
  - bytes [0..N]   hold one field
  - bytes [N..M]   hold the next field
  - etc.

The "address" of the object is the address of its first byte. Every field has
an offset from that first byte — you can compute it with `offsetof(Type, field)`.

================================================================================
WHAT FIELDS DOES A BlinkElement NEED?
================================================================================

At minimum, a DOM element needs:
  - tag_name   : the HTML tag, e.g. "div", "span", "p"
  - id         : the value of the id="" attribute

Both are std::string. A std::string on most 64-bit systems occupies 32 bytes
(it stores a pointer to heap-allocated character data, a size, and a capacity).
So two std::string fields = 64 bytes minimum, before any padding.

================================================================================
CONSTRUCTOR AND DESTRUCTOR
================================================================================

The constructor runs exactly once when the object is created. It sets the
initial field values. The destructor runs exactly once when the object's
lifetime ends — either when it goes out of scope (stack) or when `delete` is
called (heap). The destructor for std::string will free the heap memory that
the string's character data lives in.

We print messages in both so you can see exactly when construction and
destruction happen relative to the rest of the program.

================================================================================
THIS FILE'S PROGRAM
================================================================================

We create one BlinkElement on the stack, print:
  - the object's RAM address
  - sizeof the object
  - the address and value of each field
  - offsetof each field from the start of the object

Then the object goes out of scope and the destructor fires.

================================================================================
*/

#include <iostream>
#include <string>
#include <cstddef>   // offsetof

class BlinkElement {
public:
    std::string tag_name;
    std::string id;

    BlinkElement(const std::string& tag, const std::string& element_id)
        : tag_name(tag), id(element_id)
    {
        std::cout << "[constructor] BlinkElement created: <" << tag_name << " id=\"" << id << "\">\n";
    }

    ~BlinkElement() {
        std::cout << "[destructor]  BlinkElement destroyed: <" << tag_name << " id=\"" << id << "\">\n";
    }
};

int main() {
    std::cout << "=== sizeof and offsetof ===\n";
    std::cout << "sizeof(BlinkElement)      : " << sizeof(BlinkElement) << " bytes\n";
    std::cout << "offsetof tag_name         : " << offsetof(BlinkElement, tag_name) << "\n";
    std::cout << "offsetof id               : " << offsetof(BlinkElement, id) << "\n";
    std::cout << "sizeof(std::string)       : " << sizeof(std::string) << " bytes\n";
    std::cout << "\n";

    std::cout << "=== creating element on the stack ===\n";
    BlinkElement div("div", "container");
    std::cout << "\n";

    std::cout << "=== RAM layout ===\n";
    std::cout << "address of div            : " << &div << "\n";
    std::cout << "address of div.tag_name   : " << (void*)&div.tag_name << "\n";
    std::cout << "address of div.id         : " << (void*)&div.id << "\n";
    std::cout << "\n";

    // Compute byte distance between object start and each field
    uintptr_t base   = reinterpret_cast<uintptr_t>(&div);
    uintptr_t f_tag  = reinterpret_cast<uintptr_t>(&div.tag_name);
    uintptr_t f_id   = reinterpret_cast<uintptr_t>(&div.id);

    std::cout << "div.tag_name is " << (f_tag - base) << " bytes from object start\n";
    std::cout << "div.id       is " << (f_id  - base) << " bytes from object start\n";
    std::cout << "\n";

    std::cout << "=== field values ===\n";
    std::cout << "div.tag_name = \"" << div.tag_name << "\"\n";
    std::cout << "div.id       = \"" << div.id       << "\"\n";
    std::cout << "\n";

    std::cout << "=== end of main — destructor fires next ===\n";
    return 0;
}

/*
================================================================================
EXERCISE
================================================================================

Add a third field to BlinkElement:

    std::string class_name;

1. Update the constructor to accept and store a third argument `class_name`.
2. Update the constructor print line to include the class_name value.
3. In main, recreate the element with a class name of your choice.
4. Print:
     - sizeof(BlinkElement) after adding the field — does it grow by exactly
       sizeof(std::string)?
     - offsetof(BlinkElement, class_name)
     - the address of div.class_name
     - the byte distance from the object start to class_name

Compile and run:
    g++ -std=c++17 01_element_basic.cpp -o out && ./out

================================================================================
*/
