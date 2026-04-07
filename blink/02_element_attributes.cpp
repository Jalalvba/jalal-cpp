/*
================================================================================
BLINK DOM LEARNING PROJECT — FILE 02
Topic: How attributes are stored

================================================================================
BACKGROUND: WHAT IS AN ATTRIBUTE?
================================================================================

In HTML, an element can carry any number of named values called attributes:

    <div id="container" class="box" data-index="3">

Here "id", "class", and "data-index" are attribute names. "container", "box",
and "3" are their values. Every attribute is a name→value pair of strings.

In real Chromium, `blink::Element` stores attributes in a compact structure
called `AttributeCollection` (ultimately a flat array of `Attribute` objects,
each holding a `QualifiedName` and an `AtomicString`). We will use the
standard C++ equivalent: `std::map<std::string, std::string>`.

================================================================================
TWO STRATEGIES FOR STORING ATTRIBUTES
================================================================================

STRATEGY A — FIXED FIELDS
--------------------------
Promote the most-used attributes (id, class) to named fields directly on the
object:

    std::string id;
    std::string class_name;

Pro: no map overhead, direct field access.
Con: every element pays for these fields whether it uses them or not.
     A <br> with no id still holds two empty std::string objects (64 bytes).

STRATEGY B — ATTRIBUTE MAP
---------------------------
Store all attributes in a std::map<std::string, std::string>:

    std::map<std::string, std::string> attrs;

Pro: any number of attributes, no wasted fixed fields.
Con: std::map is a heap-allocated red-black tree. Every key-value pair is a
     separate heap node. Even an empty std::map occupies 48 bytes on the
     object itself (header pointer + size + allocator), and each inserted
     pair allocates ~80 bytes of heap memory for the tree node.

CHROMIUM'S ACTUAL CHOICE
-------------------------
Chromium uses neither a raw map nor raw fields. It uses a flat array that is
null until the first attribute is set — so elements with no attributes pay
almost nothing. We simulate this with a std::map but measure the real cost
so the trade-off is visible.

================================================================================
WHAT std::map COSTS IN RAM
================================================================================

On a 64-bit system:
  - sizeof(std::map<std::string,std::string>) == 48 bytes (the map object itself,
    stored inside BlinkElement — always, even when empty)
  - Each inserted entry allocates a tree node on the heap. The node contains:
      - left/right/parent pointers: 3 × 8 = 24 bytes
      - color bit: 1 byte (padded to 8)
      - key std::string:   32 bytes
      - value std::string: 32 bytes
    Total per entry: ~96 bytes of heap memory (implementation-dependent)

So setting 3 attributes on one element costs:
  48 bytes (map object) + 3 × ~96 bytes (heap nodes) ≈ 336 bytes total
  for something that looks like three short HTML strings.

================================================================================
THIS FILE'S PROGRAM
================================================================================

We build a BlinkElement that carries both fixed fields (tag_name, id) and an
attribute map. We then:
  - print sizeof the whole object and each field
  - insert several attributes and read them back
  - show what happens when you access a key that does not exist
  - show what operator[] does vs .at() vs .find()

================================================================================
*/

#include <iostream>
#include <string>
#include <map>
#include <cstdint>   // uintptr_t

class BlinkElement {
public:
    std::string tag_name;
    std::string id;
    std::map<std::string, std::string> attrs;

    BlinkElement(const std::string& tag, const std::string& element_id)
        : tag_name(tag), id(element_id)
    {
        std::cout << "[constructor] <" << tag_name << " id=\"" << id << "\">\n";
    }

    ~BlinkElement() {
        std::cout << "[destructor]  <" << tag_name << " id=\"" << id << "\"> — map had "
                  << attrs.size() << " attribute(s)\n";
    }

    // Set or overwrite an attribute
    void setAttribute(const std::string& name, const std::string& value) {
        attrs[name] = value;
    }

    // Return the attribute value, or "" if not present
    std::string getAttribute(const std::string& name) const {
        auto it = attrs.find(name);
        if (it == attrs.end()) return "";
        return it->second;
    }

    bool hasAttribute(const std::string& name) const {
        return attrs.find(name) != attrs.end();
    }

    void printAllAttributes() const {
        if (attrs.empty()) {
            std::cout << "  (no attributes)\n";
            return;
        }
        for (const auto& pair : attrs) {
            std::cout << "  " << pair.first << " = \"" << pair.second << "\"\n";
        }
    }
};

int main() {
    std::cout << "=== sizeof breakdown ===\n";
    std::cout << "sizeof(BlinkElement)                     : " << sizeof(BlinkElement) << " bytes\n";
    std::cout << "sizeof(std::string)                      : " << sizeof(std::string)  << " bytes\n";
    std::cout << "sizeof(std::map<std::string,std::string>): " << sizeof(std::map<std::string,std::string>) << " bytes\n";
    std::cout << "\n";

    std::cout << "=== create element ===\n";
    BlinkElement div("div", "container");

    // Compute field offsets via pointer arithmetic — avoids offsetof on non-standard-layout type
    uintptr_t base = reinterpret_cast<uintptr_t>(&div);
    std::cout << "address of div                           : " << &div << "\n";
    std::cout << "offset of tag_name                       : " << (reinterpret_cast<uintptr_t>(&div.tag_name) - base) << "\n";
    std::cout << "offset of id                             : " << (reinterpret_cast<uintptr_t>(&div.id)       - base) << "\n";
    std::cout << "offset of attrs                          : " << (reinterpret_cast<uintptr_t>(&div.attrs)    - base) << "\n";
    std::cout << "address of div.attrs                     : " << (void*)&div.attrs << "\n";
    std::cout << "attrs.size() before inserts              : " << div.attrs.size() << "\n";
    std::cout << "\n";

    std::cout << "=== setAttribute ===\n";
    div.setAttribute("class", "box highlight");
    div.setAttribute("data-index", "3");
    div.setAttribute("hidden", "true");
    std::cout << "attrs.size() after 3 inserts: " << div.attrs.size() << "\n";
    std::cout << "\n";

    std::cout << "=== getAttribute ===\n";
    std::cout << "getAttribute(\"class\")      : \"" << div.getAttribute("class")      << "\"\n";
    std::cout << "getAttribute(\"data-index\") : \"" << div.getAttribute("data-index") << "\"\n";
    std::cout << "getAttribute(\"missing\")    : \"" << div.getAttribute("missing")    << "\"\n";
    std::cout << "\n";

    std::cout << "=== hasAttribute ===\n";
    std::cout << "hasAttribute(\"hidden\")  : " << div.hasAttribute("hidden")  << "\n";
    std::cout << "hasAttribute(\"missing\") : " << div.hasAttribute("missing") << "\n";
    std::cout << "\n";

    std::cout << "=== WARNING: operator[] inserts a blank entry if key is absent ===\n";
    std::cout << "attrs.size() before operator[] : " << div.attrs.size() << "\n";
    std::string val = div.attrs["typo"];  // INSERTS "typo" -> ""
    std::cout << "attrs.size() after  operator[] : " << div.attrs.size() << "\n";
    std::cout << "value read : \"" << val << "\"\n";
    std::cout << "(a blank key \"typo\" now lives in the map)\n";
    std::cout << "\n";

    std::cout << "=== all attributes (including the accidental one) ===\n";
    div.printAllAttributes();
    std::cout << "\n";

    std::cout << "=== overwrite an attribute ===\n";
    div.setAttribute("class", "box");   // replaces "box highlight"
    std::cout << "class after overwrite: \"" << div.getAttribute("class") << "\"\n";
    std::cout << "\n";

    std::cout << "=== end of main ===\n";
    return 0;
}

/*
================================================================================
EXERCISE
================================================================================

1. Add a method to BlinkElement:

       void removeAttribute(const std::string& name);

   It should erase the key from attrs. Use std::map::erase(key).
   After calling it, hasAttribute(name) must return false.

2. In main, after the program runs:
   a. Remove the "hidden" attribute.
   b. Print attrs.size() before and after removal.
   c. Confirm hasAttribute("hidden") returns 0.

3. Add a second method:

       int attributeCount() const;

   It returns attrs.size() as an int.

4. Observation question (no code needed — answer as a comment at the bottom):
   After the accidental operator[] call that inserted "typo", you called
   setAttribute("class", "box"). Did that affect the "typo" entry?
   How would you remove it cleanly?

Compile and run:
    g++ -std=c++17 02_element_attributes.cpp -o out && ./out

================================================================================
*/
