#include <iostream>
#include <string>
#include <vector>
#include <cstdint>   // uint8_t
#include <iomanip>   // std::setw, std::left, std::hex

// ─────────────────────────────────────────────────────────────────────────────
// Section 12 — Compiler Pipeline
//
// Competence: trace "int result = 1 + 1;" from source text to raw machine bytes.
//
// The five stages every C++ compiler runs:
//
//   Source text
//       │
//   [1] Lexer      — characters  →  stream of tokens
//       │
//   [2] Parser     — tokens      →  Abstract Syntax Tree (AST)
//       │
//   [3] IR gen     — AST         →  three-address intermediate representation
//       │
//   [4] Code gen   — IR          →  target assembly (x86-64 here)
//       │
//   [5] Assembler  — assembly    →  raw machine bytes
//
// Chromium note: This is what g++ does to your source before any byte reaches
// the CPU.  V8 does the same thing at runtime for JavaScript — Ignition
// produces bytecode (the IR stage), TurboFan produces machine code (the
// assembly + bytes stage).  The pipeline is identical; only the input language
// and output ISA differ.
// ─────────────────────────────────────────────────────────────────────────────

// ═════════════════════════════════════════════════════════════════════════════
// Data structures — one per stage
// ═════════════════════════════════════════════════════════════════════════════

// Stage 1
struct Token {
    std::string type;    // "keyword", "identifier", "operator", "literal", "punctuation"
    std::string value;   // the actual text
};

// Stage 2
struct ASTNode {
    std::string kind;    // "VarDecl", "BinaryOp", "Literal", "Var"
    std::string value;   // e.g. "result", "+", "1"
    ASTNode*    left;
    ASTNode*    right;
};

// Stage 3
struct IRInstruction {
    std::string op;    // "load_const", "add", "store"
    std::string dest;  // destination temp or variable name
    std::string src1;  // first source operand
    std::string src2;  // second source operand (empty if unused)
};

// Stage 4
struct AsmLine {
    std::string mnemonic;  // "mov", "add"
    std::string op1;       // first operand
    std::string op2;       // second operand
    std::string comment;   // human note
};

// Stage 5 — one annotated byte
struct EncodedByte {
    uint8_t     val;
    std::string comment;
};

// ═════════════════════════════════════════════════════════════════════════════
// Stage functions
// ═════════════════════════════════════════════════════════════════════════════

// ── Stage 1: Lexer ────────────────────────────────────────────────────────────
// A real lexer scans one character at a time and matches patterns.
// We hard-code the token stream for "int result = 1 + 1;" to keep the focus
// on the pipeline structure rather than string-scanning mechanics.
std::vector<Token> lex(const std::string& /*src*/) {
    return {
        { "keyword",     "int"    },
        { "identifier",  "result" },
        { "operator",    "="      },
        { "literal",     "1"      },
        { "operator",    "+"      },
        { "literal",     "1"      },
        { "punctuation", ";"      },
    };
}

// ── Stage 2: Parser / AST builder ────────────────────────────────────────────
// A real parser reads the token stream and applies grammar rules.
// We build the tree directly to expose its shape.
//
// Tree for "int result = 1 + 1":
//
//   VarDecl("result")          ← declares an int called result
//   └── BinaryOp("+")          ← initialiser is an addition expression
//       ├── Literal("1")       ← left operand
//       └── Literal("1")       ← right operand
static ASTNode* make_node(std::string kind, std::string value,
                           ASTNode* left = nullptr, ASTNode* right = nullptr) {
    return new ASTNode{ std::move(kind), std::move(value), left, right };
}

ASTNode* build_ast() {
    ASTNode* lit1  = make_node("Literal",  "1");
    ASTNode* lit2  = make_node("Literal",  "1");
    ASTNode* binop = make_node("BinaryOp", "+", lit1, lit2);
    ASTNode* decl  = make_node("VarDecl",  "result", nullptr, binop);
    return decl;
}

void print_ast(const ASTNode* node, int depth = 0) {
    if (!node) return;
    std::cout << std::string(static_cast<std::size_t>(depth) * 4, ' ')
              << node->kind << "(\"" << node->value << "\")"
              << "  @ " << static_cast<const void*>(node) << "\n";
    print_ast(node->left,  depth + 1);
    print_ast(node->right, depth + 1);
}

void free_ast(ASTNode* node) {
    if (!node) return;
    free_ast(node->left);
    free_ast(node->right);
    delete node;
}

// ── Stage 3: IR generation ────────────────────────────────────────────────────
// Three-address code (TAC): every instruction has at most one operator and
// uses temporaries (t1, t2 …) to hold intermediate values.
// This is the form GCC uses internally (GIMPLE) and LLVM IR uses.
std::vector<IRInstruction> generate_ir(const ASTNode* decl) {
    // decl->right is the BinaryOp node; its children are the two Literals.
    const ASTNode* binop = decl->right;
    return {
        { "load_const", "t1",        binop->left->value,  ""   },
        { "load_const", "t2",        binop->right->value, ""   },
        { "add",        "t3",        "t1",                "t2" },
        { "store",      decl->value, "t3",                ""   },
    };
}

// ── Stage 4: Assembly generation ─────────────────────────────────────────────
// Map IR instructions to x86-64 register-machine instructions.
// Convention: result lives in EAX between operations; final store to [rbp-4]
// which is where the compiler assigns 'result' in the local stack frame.
std::vector<AsmLine> generate_asm(const std::vector<IRInstruction>& ir) {
    (void)ir;  // our mapping is fixed for this input; ir shown for structure
    return {
        { "mov", "eax",     "1",   "load first operand (t1) into EAX"           },
        { "add", "eax",     "1",   "add second operand (t2) to EAX → t3 in EAX" },
        { "mov", "[rbp-4]", "eax", "store EAX to 'result' stack slot at RBP-4"  },
    };
}

// ── Stage 5: Machine-byte encoding ───────────────────────────────────────────
// Real x86-64 encodings. Each field of the encoding is annotated:
//
//  mov eax, 1      → B8 01 00 00 00
//    B8          opcode for MOV r32, imm32 (B8 + reg id; EAX = register 0)
//    01 00 00 00 immediate 32-bit value 1 in little-endian order
//
//  add eax, 1      → 83 C0 01
//    83          opcode for ADD r/m32, imm8 (sign-extended immediate)
//    C0          ModRM byte: mod=11 (register), /0 (ADD), rm=000 (EAX)
//                  bits: [11][000][000] = 1100 0000 = 0xC0
//    01          imm8 = 1
//
//  mov [rbp-4], eax → 89 45 FC
//    89          opcode for MOV r/m32, r32
//    45          ModRM byte: mod=01 (register + disp8), reg=000 (EAX=source),
//                            rm=101 (RBP)
//                  bits: [01][000][101] = 0100 0101 = 0x45
//    FC          disp8 = 0xFC = -4 in two's complement → effective address RBP-4
//
std::vector<std::vector<EncodedByte>> get_encodings() {
    return {
        {   // mov eax, 1
            { 0xB8, "opcode: MOV r32,imm32  (B8 + register-id; EAX = reg 0)" },
            { 0x01, "imm32 byte 0 — value 1, little-endian"                  },
            { 0x00, "imm32 byte 1 — 0"                                        },
            { 0x00, "imm32 byte 2 — 0"                                        },
            { 0x00, "imm32 byte 3 — 0"                                        },
        },
        {   // add eax, 1
            { 0x83, "opcode: ADD r/m32, imm8 (sign-extended)"                 },
            { 0xC0, "ModRM 0xC0: mod=11(reg) /0=ADD rm=000(EAX)  [11 000 000]" },
            { 0x01, "imm8 — value 1"                                          },
        },
        {   // mov [rbp-4], eax
            { 0x89, "opcode: MOV r/m32, r32"                                  },
            { 0x45, "ModRM 0x45: mod=01(disp8) reg=000(EAX) rm=101(RBP)  [01 000 101]" },
            { 0xFC, "disp8 0xFC — two's complement of -4; address = RBP - 4"  },
        },
    };
}

// ═════════════════════════════════════════════════════════════════════════════
int main() {
    const std::string source = "int result = 1 + 1;";

    std::cout << "══════════════════════════════════════════════\n";
    std::cout << "Section 12 — Compiler Pipeline\n";
    std::cout << "══════════════════════════════════════════════\n";
    std::cout << "\n  Input source: \"" << source << "\"\n";
    std::cout << "\n  sizeof(Token)         = " << sizeof(Token)         << " bytes\n";
    std::cout << "  sizeof(ASTNode)       = " << sizeof(ASTNode)       << " bytes\n";
    std::cout << "  sizeof(IRInstruction) = " << sizeof(IRInstruction) << " bytes\n";
    std::cout << "  sizeof(AsmLine)       = " << sizeof(AsmLine)       << " bytes\n";

    // ── Stage 1: Lex ─────────────────────────────────────────────────────────
    std::cout << "\n──────────────────────────────────────────────\n";
    std::cout << "Stage 1 — Lexer  (source text → tokens)\n";
    std::cout << "──────────────────────────────────────────────\n";

    auto tokens = lex(source);
    std::cout << "  " << tokens.size() << " tokens:\n\n";
    for (std::size_t i = 0; i < tokens.size(); ++i) {
        std::cout << "  [" << i << "]  "
                  << std::left << std::setw(12) << tokens[i].type
                  << "  \"" << tokens[i].value << "\"\n";
    }

    // ── Stage 2: Parse / AST ──────────────────────────────────────────────────
    std::cout << "\n──────────────────────────────────────────────\n";
    std::cout << "Stage 2 — Parser  (tokens → AST)\n";
    std::cout << "──────────────────────────────────────────────\n";

    ASTNode* ast = build_ast();
    std::cout << "\n  AST (each node shows kind, value, and heap address):\n\n";
    print_ast(ast, 1);

    // ── Stage 3: IR ───────────────────────────────────────────────────────────
    std::cout << "\n──────────────────────────────────────────────\n";
    std::cout << "Stage 3 — IR Generation  (AST → three-address code)\n";
    std::cout << "──────────────────────────────────────────────\n";

    auto ir = generate_ir(ast);
    std::cout << "\n  " << ir.size() << " IR instructions:\n\n";
    for (const auto& ins : ir) {
        std::cout << "  " << std::left << std::setw(6) << ins.dest << " = "
                  << ins.op;
        if (!ins.src1.empty()) std::cout << "  " << ins.src1;
        if (!ins.src2.empty()) std::cout << ", " << ins.src2;
        std::cout << "\n";
    }

    // ── Stage 4: Assembly ─────────────────────────────────────────────────────
    std::cout << "\n──────────────────────────────────────────────\n";
    std::cout << "Stage 4 — Code Generation  (IR → x86-64 assembly)\n";
    std::cout << "──────────────────────────────────────────────\n";

    auto asm_lines = generate_asm(ir);
    std::cout << "\n  " << asm_lines.size() << " instructions:\n\n";
    for (const auto& line : asm_lines) {
        std::cout << "  " << std::left << std::setw(4) << line.mnemonic
                  << "  " << std::setw(10) << line.op1
                  << (line.op2.empty() ? "" : ", " + line.op2);
        std::cout << std::right << "   ; " << line.comment << "\n";
    }

    // ── Stage 5: Machine bytes ────────────────────────────────────────────────
    std::cout << "\n──────────────────────────────────────────────\n";
    std::cout << "Stage 5 — Assembler  (assembly → raw x86-64 bytes)\n";
    std::cout << "──────────────────────────────────────────────\n\n";

    auto encodings = get_encodings();
    std::size_t total_bytes = 0;

    for (std::size_t i = 0; i < asm_lines.size(); ++i) {
        const auto& line = asm_lines[i];
        const auto& enc  = encodings[i];

        // Print the assembly line and all its hex bytes on one row
        std::cout << "  " << line.mnemonic << " " << line.op1;
        if (!line.op2.empty()) std::cout << ", " << line.op2;
        std::cout << "\n";

        // Hex byte summary
        std::cout << "    bytes: ";
        for (const auto& b : enc)
            std::cout << std::uppercase << std::hex
                      << std::setw(2) << std::setfill('0') << static_cast<int>(b.val)
                      << " " << std::dec << std::setfill(' ');
        std::cout << " (" << enc.size() << " bytes)\n";

        // Per-byte annotation
        for (const auto& b : enc) {
            std::cout << "    "
                      << std::uppercase << std::hex
                      << std::setw(2) << std::setfill('0') << static_cast<int>(b.val)
                      << std::dec << std::setfill(' ')
                      << "  →  " << b.comment << "\n";
        }
        total_bytes += enc.size();
        std::cout << "\n";
    }

    // Full byte stream
    std::cout << "  Complete machine-code byte stream (" << total_bytes << " bytes total):\n";
    std::cout << "    ";
    for (const auto& enc : encodings)
        for (const auto& b : enc)
            std::cout << std::uppercase << std::hex
                      << std::setw(2) << std::setfill('0') << static_cast<int>(b.val)
                      << " " << std::dec << std::setfill(' ');
    std::cout << "\n";

    std::cout << "\n  This byte sequence is what the OS loader copies into memory\n";
    std::cout << "  and what the CPU's fetch-decode-execute pipeline processes.\n";

    free_ast(ast);
    return 0;
}
