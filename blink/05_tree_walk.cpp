/*
================================================================================
BLINK DOM LEARNING PROJECT — FILE 05
Topic: Recursive tree traversal. Print the full DOM tree from root.

================================================================================
BACKGROUND: WHAT TRAVERSAL MEANS IN RAM
================================================================================

After files 03 and 04, every element has:
  - parent*   : one pointer up
  - children  : a vector of pointers down

The tree is not a contiguous array. The objects are scattered across RAM —
each one is an independent block of bytes at a different address. The only
thing connecting them is the pointer values stored inside those blocks.

Traversal means: start at one object's address, read its children vector to
get more addresses, go to each of those addresses, repeat.

The CPU does not "see" a tree. It sees: jump to address A, read bytes [72..79]
(the vector's heap pointer), follow that to the heap buffer, read 8-byte slots,
each slot is another address, jump there, repeat.

================================================================================
RECURSIVE TRAVERSAL: THE CALL STACK
================================================================================

A recursive function like printTree(node, depth) does this:

  1. Receive a BlinkElement* (an 8-byte address).
  2. Print the node at that address.
  3. For each pointer in node->children, call printTree(child, depth+1).
  4. Return.

Each call to printTree pushes a stack frame. The maximum depth of the call
stack equals the depth of the deepest node in the tree. For HTML documents
this is typically 10–30 levels. For pathological input it can be thousands,
causing a stack overflow — which is why real Chromium uses an explicit stack
(a std::vector used as a stack) rather than recursion for large trees.

For our learning tree (5–10 nodes), recursion is fine.

================================================================================
THREE TRAVERSAL ORDERS
================================================================================

PRE-ORDER  — process the node BEFORE its children
             Root is printed first. Used for: copying a tree, printing
             indented HTML, Chromium's layout tree dump.

             visit(node):
               print node
               for each child: visit(child)

POST-ORDER — process the node AFTER its children
             Leaves are printed first. Used for: deleting a tree
             (you must delete children before the parent that owns them),
             computing sizes bottom-up.

             visit(node):
               for each child: visit(child)
               print node

IN-ORDER   — only meaningful for binary trees (left, node, right).
             Not used for DOM trees.

================================================================================
DEPTH AND INDENTATION
================================================================================

Depth is not stored in any object. It is computed during traversal by passing
an integer argument that increments with each recursive call. At depth 0 there
is no indentation. At depth 1 there are 2 spaces. At depth N there are 2*N
spaces. This is purely a display artifact — the objects themselves have no
knowledge of their depth.

================================================================================
THIS FILE'S PROGRAM
================================================================================

We build a 7-node tree:

    html
    ├── head
    │   └── title
    └── body
        ├── h1
        ├── div  id="container"
        │   └── p  id="intro"
        └── footer

Then we run:
  - pre-order traversal  (prints root first)
  - post-order traversal (prints root last)
  - a depth-first search that finds a node by id

================================================================================
*/

#include <iostream>
#include <string>
#include <vector>

class BlinkElement {
public:
    std::string                 tag_name;
    std::string                 id;
    BlinkElement*               parent;
    std::vector<BlinkElement*>  children;

    BlinkElement(const std::string& tag, const std::string& element_id)
        : tag_name(tag), id(element_id), parent(nullptr) {}

    ~BlinkElement() {}

    void appendChild(BlinkElement* child) {
        children.push_back(child);
        child->parent = this;
    }
};

// -----------------------------------------------------------------------
// Pre-order: print node, then recurse into children.
// depth controls indentation — it is NOT stored in the node.
// -----------------------------------------------------------------------
void printPreOrder(const BlinkElement* node, int depth = 0) {
    // Print indentation
    for (int i = 0; i < depth; ++i) std::cout << "  ";

    // Print this node
    std::cout << "<" << node->tag_name;
    if (!node->id.empty()) std::cout << " id=\"" << node->id << "\"";
    std::cout << ">  @" << node << "\n";

    // Recurse into each child
    for (BlinkElement* child : node->children) {
        printPreOrder(child, depth + 1);
    }
}

// -----------------------------------------------------------------------
// Post-order: recurse into children first, then print node.
// -----------------------------------------------------------------------
void printPostOrder(const BlinkElement* node, int depth = 0) {
    // Recurse first
    for (BlinkElement* child : node->children) {
        printPostOrder(child, depth + 1);
    }

    // Print this node after children
    for (int i = 0; i < depth; ++i) std::cout << "  ";
    std::cout << "<" << node->tag_name;
    if (!node->id.empty()) std::cout << " id=\"" << node->id << "\"";
    std::cout << ">\n";
}

// -----------------------------------------------------------------------
// Depth-first search: return a pointer to the first element whose id
// matches, or nullptr if not found.
// -----------------------------------------------------------------------
BlinkElement* findById(BlinkElement* node, const std::string& target_id) {
    if (node->id == target_id) return node;

    for (BlinkElement* child : node->children) {
        BlinkElement* result = findById(child, target_id);
        if (result != nullptr) return result;
    }

    return nullptr;
}

// -----------------------------------------------------------------------
// Count all nodes in the subtree rooted at node (including node itself).
// -----------------------------------------------------------------------
int countNodes(const BlinkElement* node) {
    int count = 1;  // this node
    for (BlinkElement* child : node->children) {
        count += countNodes(child);
    }
    return count;
}

// -----------------------------------------------------------------------
// Return the depth of the deepest node below (and including) this node.
// -----------------------------------------------------------------------
int treeDepth(const BlinkElement* node) {
    if (node->children.empty()) return 0;

    int max_child_depth = 0;
    for (BlinkElement* child : node->children) {
        int d = treeDepth(child);
        if (d > max_child_depth) max_child_depth = d;
    }
    return max_child_depth + 1;
}

int main() {
    // -----------------------------------------------------------------
    // Build the tree
    // -----------------------------------------------------------------
    BlinkElement html   ("html",   "");
    BlinkElement head   ("head",   "");
    BlinkElement title  ("title",  "");
    BlinkElement body   ("body",   "");
    BlinkElement h1     ("h1",     "");
    BlinkElement div    ("div",    "container");
    BlinkElement p      ("p",      "intro");
    BlinkElement footer ("footer", "");

    html.appendChild(&head);
    html.appendChild(&body);
    head.appendChild(&title);
    body.appendChild(&h1);
    body.appendChild(&div);
    body.appendChild(&footer);
    div.appendChild(&p);

    // -----------------------------------------------------------------
    std::cout << "=== pre-order traversal (root first) ===\n";
    printPreOrder(&html);
    std::cout << "\n";

    // -----------------------------------------------------------------
    std::cout << "=== post-order traversal (root last) ===\n";
    printPostOrder(&html);
    std::cout << "\n";

    // -----------------------------------------------------------------
    std::cout << "=== tree statistics ===\n";
    std::cout << "total nodes : " << countNodes(&html) << "\n";
    std::cout << "tree depth  : " << treeDepth(&html)  << "\n";
    std::cout << "\n";

    // -----------------------------------------------------------------
    std::cout << "=== findById ===\n";
    BlinkElement* found = findById(&html, "intro");
    if (found != nullptr) {
        std::cout << "found \"intro\"  : @" << found << "\n";
        std::cout << "same as &p?    : " << (found == &p ? "yes" : "no") << "\n";
        std::cout << "parent tag     : " << found->parent->tag_name << "\n";
    }

    BlinkElement* missing = findById(&html, "ghost");
    std::cout << "findById(\"ghost\") : " << missing << "  (nullptr == not found)\n";
    std::cout << "\n";

    // -----------------------------------------------------------------
    std::cout << "=== walk from leaf to root via parent* ===\n";
    BlinkElement* cursor = &p;
    while (cursor != nullptr) {
        std::cout << "  <" << cursor->tag_name;
        if (!cursor->id.empty()) std::cout << " id=\"" << cursor->id << "\"";
        std::cout << ">  @" << cursor << "\n";
        cursor = cursor->parent;
    }

    return 0;
}

/*
================================================================================
EXERCISE
================================================================================

1. Add a node <span id="highlight"> as a child of p (the "intro" paragraph).
   Rebuild and re-run. Confirm:
     - total nodes becomes 9
     - tree depth becomes 4
     - pre-order still prints root first, span last among its siblings

2. Write a new function:

       void printAncestors(const BlinkElement* node);

   It walks the parent* chain from `node` up to the root and prints each
   ancestor's tag_name on its own line, starting with the immediate parent.
   Call it on the new <span> node. Expected output (bottom-up):

       p
       div
       body
       html

3. Write a new function:

       int childIndex(const BlinkElement* node);

   It looks at node->parent->children and returns the index (0-based) at
   which `node` appears. Return -1 if node->parent is nullptr (it is the
   root). Call it on &div and &footer and print the results.
   Hint: iterate parent->children, compare each pointer to node.

4. Observation (answer as a comment):
   In printPreOrder, depth is passed by value and incremented for each
   recursive call. The original caller's depth variable never changes.
   Why is this safe? What would break if depth were passed by reference
   instead?

Compile and run:
    g++ -std=c++17 05_tree_walk.cpp -o out && ./out

================================================================================
*/
