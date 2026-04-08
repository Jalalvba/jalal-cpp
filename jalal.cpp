#include <iostream>
#include <cstdint>   // uint8_t, uintptr_t
#include <iomanip>   // std::hex, std::setw, std::setfill

// ─────────────────────────────────────────────────────────────────────────────
// jalal.cpp — Template Functions at the Memory Level
//
// Goal: show what the compiler actually produces when you write a function
// template, and how that maps to memory addresses and byte layouts.
//
// Key facts:
//   - A template function is not code.  It is a recipe.
//   - For each distinct type T the compiler sees at the call site it stamps out
//     a fully typed, fully compiled function — a separate blob of machine code.
//   - sizeof(T) drives everything: the stack frame size, the copy width, the
//     argument-passing ABI.
//
// Chromium connection:
//   - V8 uses template functions heavily for type-safe object casting
//     (v8::Object::Cast<T>), byte-level memory ops (ReadUnalignedValue<T>), and
//     garbage-collector write barriers (WriteBarrier::Marking<T>).
//   - Blink uses them for RefCounted<T>, Member<T>, and DOM attribute accessors.
// ─────────────────────────────────────────────────────────────────────────────


// ── Template function 1: print_bytes<T> ──────────────────────────────────────
// Reinterpret any value as an array of raw bytes and print them in hex.
// The compiler stamps out a different version for each T — the loop bound
// `sizeof(T)` is a compile-time constant baked into that version's machine code.
template<typename T>
void print_bytes(const char* label, const T& value) {
    const uint8_t* p = reinterpret_cast<const uint8_t*>(&value);
    std::cout << "  " << label
              << "  addr=" << static_cast<const void*>(p)
              << "  size=" << sizeof(T) << "B  bytes=[ ";
    for (std::size_t i = 0; i < sizeof(T); ++i)
        std::cout << std::hex << std::setw(2) << std::setfill('0')
                  << static_cast<int>(p[i]) << " ";
    std::cout << std::dec << "]\n";
}


// ── Template function 2: swap_vals<T> ────────────────────────────────────────
// Classic in-place swap.  The only thing that changes across instantiations is
// sizeof(T): the temporary variable tmp occupies sizeof(T) bytes on the stack.
template<typename T>
void swap_vals(T& a, T& b) {
    T tmp = a;   // copies sizeof(T) bytes onto the stack
    a = b;
    b = tmp;
}


// ── Template function 3: typed_max<T> ────────────────────────────────────────
// Returns the larger of two values.  With T=int the comparison is integer;
// with T=double it is a floating-point compare — different machine instructions,
// same source text.
template<typename T>
T typed_max(const T& a, const T& b) {
    return (a > b) ? a : b;
}


// ── A plain struct to use as a template argument ──────────────────────────────
struct Vec2 {
    float x, y;   // 4 + 4 = 8 bytes, no padding needed
};


// ─────────────────────────────────────────────────────────────────────────────
int main() {
    std::cout << "══════════════════════════════════════════════\n";
    std::cout << "jalal.cpp — Template Functions at the Memory Level\n";
    std::cout << "══════════════════════════════════════════════\n";

    // ── 1. print_bytes<T> — three instantiations, three sizeof values ──────────
    // The compiler generates three separate functions from the one template body.
    // Each bakes a different loop bound (1, 4, 8) into its machine code.
    std::cout << "\n── 1. print_bytes<T>: one template, three instantiations ──\n";

    char    c  = 'A';          // 1 byte  — ASCII 0x41
    int     i  = 305419896;    // 4 bytes — 0x12345678 in little-endian: 78 56 34 12
    double  d  = 1.0;          // 8 bytes — IEEE-754: 00 00 00 00 00 00 f0 3f

    print_bytes<char>  ("char   c='A'    ", c);
    print_bytes<int>   ("int    i=0x12345678", i);
    print_bytes<double>("double d=1.0    ", d);

    // ── 2. sizeof per instantiation ───────────────────────────────────────────
    // Each instantiation of swap_vals<T> allocates a `T tmp` on the stack.
    // sizeof tells you exactly how wide that temporary is.
    std::cout << "\n── 2. sizeof per template instantiation ──\n";
    std::cout << "  sizeof(char)   = " << sizeof(char)   << " byte  → swap_vals<char>   copies 1 byte\n";
    std::cout << "  sizeof(int)    = " << sizeof(int)    << " bytes → swap_vals<int>    copies 4 bytes\n";
    std::cout << "  sizeof(double) = " << sizeof(double) << " bytes → swap_vals<double> copies 8 bytes\n";
    std::cout << "  sizeof(Vec2)   = " << sizeof(Vec2)   << " bytes → swap_vals<Vec2>   copies 8 bytes\n";

    // ── 3. swap_vals<int> — watch addresses stay fixed, values move ───────────
    std::cout << "\n── 3. swap_vals<int> ──\n";
    int x = 10, y = 20;
    std::cout << "  before: x=" << x << " &x=" << &x
              << "   y=" << y << " &y=" << &y << "\n";
    swap_vals(x, y);
    std::cout << "  after:  x=" << x << " &x=" << &x
              << "   y=" << y << " &y=" << &y << "\n";
    std::cout << "  addresses unchanged — only the bytes at those addresses moved\n";

    // ── 4. swap_vals<Vec2> — same template, wider copy ────────────────────────
    std::cout << "\n── 4. swap_vals<Vec2> ──\n";
    Vec2 v1{1.0f, 2.0f}, v2{3.0f, 4.0f};
    std::cout << "  before: v1=(" << v1.x << "," << v1.y << ")  v2=(" << v2.x << "," << v2.y << ")\n";
    print_bytes<Vec2>("v1 bytes", v1);
    print_bytes<Vec2>("v2 bytes", v2);
    swap_vals(v1, v2);
    std::cout << "  after:  v1=(" << v1.x << "," << v1.y << ")  v2=(" << v2.x << "," << v2.y << ")\n";
    print_bytes<Vec2>("v1 bytes", v1);
    print_bytes<Vec2>("v2 bytes", v2);

    // ── 5. typed_max<T> — different comparison instructions per T ─────────────
    // typed_max<int>    compiles to an integer compare (CMP + CMOV on x86-64).
    // typed_max<double> compiles to a float compare  (UCOMISD on x86-64).
    // Same source, different machine code — the template is not polymorphism,
    // it is code generation.
    std::cout << "\n── 5. typed_max<T> ──\n";
    std::cout << "  typed_max<int>(3, 7)       = " << typed_max<int>(3, 7)       << "\n";
    std::cout << "  typed_max<double>(3.5, 2.1)= " << typed_max<double>(3.5, 2.1) << "\n";
    std::cout << "  typed_max<char>('z','a')   = " << typed_max<char>('z', 'a')   << "\n";

    // ── 6. Function pointers prove separate code blobs ────────────────────────
    // Each instantiation lives at its own address in the text segment.
    // Printing those addresses shows that the compiler really did emit three
    // distinct functions from the single template definition.
    std::cout << "\n── 6. Function pointers — each instantiation is a distinct address ──\n";
    void (*pb_char  )(const char*,   const char&)   = &print_bytes<char>;
    void (*pb_int   )(const char*,   const int&)    = &print_bytes<int>;
    void (*pb_double)(const char*,   const double&) = &print_bytes<double>;

    std::cout << "  &print_bytes<char>   = " << reinterpret_cast<void*>(pb_char)   << "\n";
    std::cout << "  &print_bytes<int>    = " << reinterpret_cast<void*>(pb_int)    << "\n";
    std::cout << "  &print_bytes<double> = " << reinterpret_cast<void*>(pb_double) << "\n";
    std::cout << "  All different — three separate machine-code blobs in .text\n";

    // ── 7. Template type deduction — no angle brackets needed ─────────────────
    // The compiler infers T from the argument type at the call site.
    // This is the same deduction engine that makes auto and decltype work.
    std::cout << "\n── 7. Type deduction — compiler infers T automatically ──\n";
    float  fa = 1.5f, fb = 9.9f;
    double da = 1.5,  db = 9.9;
    std::cout << "  swap_vals(fa, fb) → T deduced as float\n";
    swap_vals(fa, fb);
    std::cout << "    fa=" << fa << "  fb=" << fb << "\n";
    std::cout << "  swap_vals(da, db) → T deduced as double\n";
    swap_vals(da, db);
    std::cout << "    da=" << da << "  db=" << db << "\n";

    std::cout << "\n══════════════════════════════════════════════\n";
    std::cout << "Key takeaway:\n";
    std::cout << "  A template is a compile-time recipe.  The compiler stamps out\n";
    std::cout << "  a separate, fully-typed function for every distinct T it sees.\n";
    std::cout << "  sizeof(T) is baked in — no runtime type info, no overhead.\n";
    std::cout << "══════════════════════════════════════════════\n";

    return 0;
}
