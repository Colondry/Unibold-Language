#include <iostream>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>
#include <cctype>
#include <algorithm>
#include <thread>
#include <chrono>
#include <numeric>

#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#endif

// ==========================================
// 1. TOKENS & AST NODE
// ==========================================
enum class Kind { 
    Mode, PrintStr, Input, PrintCell, Clear, 
    Shift, LoopStart, LoopEnd, CondStart, Break, Continue, Exit,
    Dec, InsertChar, InsertNum, PrintCellChar, NewLine, MemSet_Stack, MemSet_Heap
};

struct Token {
    Kind kind;
    std::string str;
    int val = 0;
};

struct Node {
    Kind kind;
    std::string str;
    int val = 0;
    std::vector<Node> body;
};

// LEXER
class Lexer {
    std::string src;
    size_t pos = 0;

    bool match(const std::string& pat) {
        if (src.compare(pos, pat.length(), pat) == 0) {
            pos += pat.length();
            return true;
        }
        return false;
    }

    inline char currentChar() {
        return (pos < src.length()) ? src[pos] : '\0';
    }

    void skipCommentsAndSpace() {
        while (pos < src.length()) {
            if (std::isspace(src[pos])) {
                pos++;
            } else if (src[pos] == ';' || src.compare(pos, 2, "//") == 0) {
                while (pos < src.length() && src[pos] != '\n') pos++;
            } else break;
        }
    }

public:
    explicit Lexer(std::string source) : src(std::move(source)) {}

    std::vector<Token> tokenize() {
        std::vector<Token> tokens;
        std::string mode;
        
        while (pos < src.length()) {
            skipCommentsAndSpace();
            if (pos >= src.length()) break;

            if (match("!")) {
                if (match("s")) {
                    std::string raw;
                    if (pos < src.length() && src[pos] == '<') pos++;
                    while (pos < src.length() && src[pos] != '>' && !std::isspace(src[pos])) raw += src[pos++];
                    if (pos < src.length() && src[pos] == '>') pos++;
                    tokAdd(tokens, Kind::MemSet_Stack, raw);
                } else if (match("h")) {
                    std::string raw;
                    if (pos < src.length() && src[pos] == '<') pos++;
                    while (pos < src.length() && src[pos] != '>' && !std::isspace(src[pos])) raw += src[pos++];
                    if (pos < src.length() && src[pos] == '>') pos++;
                    tokAdd(tokens, Kind::MemSet_Heap, raw);
                }
            }
            else if (match("!* [") || match("![")) tokAdd(tokens, Kind::LoopStart);
            else if (match("] *!") || match("]")) tokAdd(tokens, Kind::LoopEnd);
            else if (match("$*")) {
                if (match("br")) {
                    tokAdd(tokens, Kind::Break);
                }
                else if (match("con")) {
                    tokAdd(tokens, Kind::Continue);
                }
            }
            else if (match("$<0x")) {
                std::string raw;
                while (pos < src.length() && src[pos] != '>' && !std::isspace(src[pos])) raw += src[pos++];
                if (pos < src.length() && src[pos] == '>') pos++;

                if (raw == "00") { mode = raw; tokAdd(tokens, Kind::Exit, "", 0); }
                else if (raw == "F") { mode = raw; tokAdd(tokens, Kind::Exit, "", 15); }
                else if (raw == "#") { mode = "1"; tokAdd(tokens, Kind::Mode, "1"); }
                else { mode = raw; tokAdd(tokens, Kind::Mode, raw); }
            }
            else if (match("$")) {
                if (match("\"")) {
                    std::string str;
                    while (pos < src.length() && src[pos] != '"') str += src[pos++];
                    if (pos < src.length()) pos++; // Skip closing quote

                    if (mode == "01") {
                        tokAdd(tokens, Kind::PrintStr, str);
                    } else if (mode == "02") {
                        tokAdd(tokens, Kind::Input, str);
                    }
                }
                else if (match("N")) {
                    tokAdd(tokens, Kind::NewLine);
                }
                else if (match("#")) { 
                    if (mode == "01") { 
                        if (match("'")) tokAdd(tokens, Kind::PrintCellChar);
                        else tokAdd(tokens, Kind::PrintCell);
                    }
                }
                else if (match("X")) { 
                    if (mode == "1") tokAdd(tokens, Kind::Clear); 
                }
                else if (currentChar() == '>') { 
                    if (mode == "1") {
                        int snum = 0;
                        while (pos < src.length() && src[pos] == '>') {
                            snum++;
                            pos++;
                        }
                        tokAdd(tokens, Kind::Shift, "", snum);
                    } else {
                        pos++;
                    }
                }
                else if (match("~")) {
                    if (mode == "1") {
                        if (match("'")) {
                            std::string chr;
                            while (pos < src.length() && src[pos] != '\'') chr += src[pos++];
                            if (pos < src.length()) pos++; // consume closing '

                            if (chr.empty()) {
                                tokAdd(tokens, Kind::InsertChar, "\\0");
                            } else {
                                tokAdd(tokens, Kind::InsertChar, chr);
                            }
                        } else {
                            std::string num;
                            while (pos < src.length() && src[pos] != '~' && !std::isspace(src[pos])) num += src[pos++];
                            if (pos < src.length() && src[pos] == '~') pos++; // consume closing ~

                            if (!isNumeric(num)) {
                                std::cerr << "Lexer error: ~...~ expects numeric literal, got \"" << num << "\"\n";
                            } else {
                                tokAdd(tokens, Kind::InsertNum, num);
                            }
                        }
                    }
                }
            }
            else if (match("?")) {
                std::string ch;
                if (pos < src.length() && src[pos] == '\'') { 
                    pos++; 
                    if (pos < src.length()) ch += src[pos++]; 
                    if (pos < src.length() && src[pos] == '\'') pos++; 
                }
                skipCommentsAndSpace();
                if (match("[")) tokAdd(tokens, Kind::CondStart, ch);
            }
            else {
                std::cerr << "Lexer warning: unrecognized character '" << src[pos]
                          << "' ignored at offset " << pos << "\n";
                pos++;
            }
        }
        return tokens;
    }

private:
    void tokAdd(std::vector<Token>& t, Kind k, std::string s = "", int v = 0) {
        t.push_back({k, s, v});
    }

    bool isNumeric(const std::string& str) {
        if (str.empty()) return false; 
        return std::all_of(str.begin(), str.end(), [](unsigned char c) {
            return std::isdigit(c);
        });
    }
};

// PARSER
class Parser {
    std::vector<Token> toks;
    size_t pos = 0;

public:
    explicit Parser(std::vector<Token> t) : toks(std::move(t)) {}

    std::vector<Node> parse() {
        std::vector<Node> ast;
        while (pos < toks.size()) {
            ast.push_back(parseNode());
        }
        return ast;
    }

private:
    Node parseNode() {
        Token t = toks[pos++];
        if (t.kind == Kind::LoopStart) {
            Node n{Kind::LoopStart, "", 0, {}};
            while (pos < toks.size() && toks[pos].kind != Kind::LoopEnd) {
                n.body.push_back(parseNode());
            }
            if (pos < toks.size()) pos++; // Consume LoopEnd
            return n;
        }
        if (t.kind == Kind::CondStart) {
            Node n{Kind::CondStart, t.str, 0, {}};
            while (pos < toks.size() && toks[pos].kind != Kind::LoopEnd) {
                n.body.push_back(parseNode());
            }
            if (pos < toks.size()) pos++; // Consume LoopEnd
            return n;
        }
        return {t.kind, t.str, t.val, {}};
    }
};

// Helper function to handle string/char escaping safely for generated C source
std::string escapeChar(const std::string& in) {
    if (in == "'") return "\\'";
    if (in == "\\") return "\\\\";
    if (in == "\n") return "\\n";
    return in;
}

// CODE GEN
void emitC(const Node& n, std::ostream& out, bool& usesHeap) {
    switch (n.kind) {
        case Kind::Mode:        out << "    // mode 0x" << n.str << "\n"; break;
        case Kind::PrintStr:    out << "    printf(\"" << n.str << "\");\n"; break;
        case Kind::Input:       out << "    printf(\"" << n.str << "\"); scanf(\"%\" SCNd64, &tape[ptr]);\n"; break;
        case Kind::PrintCell:   out << "    printf(\"%\" PRId64 \"\\n\", tape[ptr]);\n"; break;
        case Kind::PrintCellChar: out << "    printf(\"%c\", (char)tape[ptr]);\n"; break;
        case Kind::NewLine:     out << "    printf(\"\\n\");\n"; break;
        case Kind::Clear:       out << "    tape[ptr] = 0;\n"; break;
        case Kind::Shift:       out << "    ptr += " << n.val << ";\n"; break;
        case Kind::Break:       out << "    break;\n"; break;
        case Kind::Continue:    out << "    continue;\n"; break;
        case Kind::MemSet_Stack:out << "    int64_t tape[" << n.str << "] = {0};\n"; break;
        case Kind::MemSet_Heap:  
            out << "    int64_t *tape = (int64_t*)calloc(" << n.str << ", sizeof(int64_t));\n"; 
            usesHeap = true;
            break;
        case Kind::Exit:        out << "    return " << n.val << ";\n"; break;
        case Kind::InsertChar:  out << "    tape[ptr] = '" << escapeChar(n.str) << "';\n"; break;
        case Kind::InsertNum:   out << "    tape[ptr] = " << n.str << ";\n"; break;
        case Kind::LoopStart:
            out << "    while (1) {\n";
            for (const auto& child : n.body) emitC(child, out, usesHeap);
            out << "    }\n";
            break;
        case Kind::CondStart:
            out << "    if (tape[ptr] == '" << escapeChar(n.str) << "') {\n";
            for (const auto& child : n.body) emitC(child, out, usesHeap);
            out << "    }\n";
            break;
        default: break;
    }
}

void executeAndMeasure(const std::string& exePath) {
#ifdef _WIN32
    STARTUPINFOA si = { sizeof(si) };
    PROCESS_INFORMATION pi;

    if (!CreateProcessA(exePath.c_str(), NULL, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        std::cerr << "Failed to start process for profiling.\n";
        return;
    }

    std::vector<double> ramSamplesMB;
    PROCESS_MEMORY_COUNTERS pmc;

    while (WaitForSingleObject(pi.hProcess, 15) == WAIT_TIMEOUT) {
        if (GetProcessMemoryInfo(pi.hProcess, &pmc, sizeof(pmc))) {
            double currentRAM = pmc.WorkingSetSize / (1024.0 * 1024.0);
            ramSamplesMB.push_back(currentRAM);
        }
    }

    FILETIME ftCreate, ftExit, ftKernel, ftUser;
    GetProcessTimes(pi.hProcess, &ftCreate, &ftExit, &ftKernel, &ftUser);
    
    uint64_t kTime = ((uint64_t)ftKernel.dwHighDateTime << 32) | ftKernel.dwLowDateTime;
    uint64_t uTime = ((uint64_t)ftUser.dwHighDateTime << 32) | ftUser.dwLowDateTime;
    double totalCpuSeconds = (kTime + uTime) / 10000000.0;

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    if (!ramSamplesMB.empty()) {
        auto [minIt, maxIt] = std::minmax_element(ramSamplesMB.begin(), ramSamplesMB.end());
        double avgRam = std::accumulate(ramSamplesMB.begin(), ramSamplesMB.end(), 0.0) / ramSamplesMB.size();

        std::cout << "\n--- Resource Report ---" << std::endl;
        std::cout << "RAM Peak : " << *maxIt << " MB\n";
        std::cout << "RAM Avg  : " << avgRam << " MB\n";
        std::cout << "RAM Min  : " << *minIt << " MB\n";
        std::cout << "Total CPU Time Spent: " << totalCpuSeconds << " seconds\n";
    }
#else
    std::cout << "\nProfiling is currently supported on Windows builds.\n";
#endif
}

int main(int argc, char* argv[]) {
    bool isDebug = false;
    if (argc < 2) {
        std::cerr << "Error: Provide source file argument.\n";
        return 1;
    }

    for (int i = 1; i < argc; ++i) {
        if (std::string_view(argv[i]) == "-wdebug") {
            isDebug = true;
            break;
        }
    }

    std::ifstream file(argv[1]);
    if (!file.is_open()) {
        std::cerr << "Error: Could not open file '" << argv[1] << "'\n";
        return 1;
    }

    std::string src((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

    Lexer lexer(src);
    Parser parser(lexer.tokenize());
    auto ast = parser.parse();

    std::ofstream out("out.c");
    out << "#include <stdio.h>\n"
        << "#include <stdint.h>\n"
        << "#include <inttypes.h>\n"
        << "#include <stdlib.h>\n\n"
        << "int main() {\n"
        << "    int ptr = 0;\n\n";

    bool usesHeap = false;
    for (const auto& node : ast) {
        emitC(node, out, usesHeap);
    }

    if (usesHeap) {
        out << "\n    free(tape);\n";
    }
    out << "    return 0;\n";
    out << "}\n";
    out.close();

    std::cout << "Successfully generated out.c!\n";

#ifdef _WIN32
    const char* compileCmd = "gcc -mconsole -w -o out.exe out.c";
    const char* runCmd     = "out.exe";
#else
    const char* compileCmd = "gcc -o out out.c";
    const char* runCmd     = "./out";
#endif

    if (std::system(compileCmd) == 0) {
        std::system(runCmd);
    } else {
        std::cerr << "Compilation failed.\n";
    }

    if (isDebug) {
        executeAndMeasure("out.exe");
    }
    
    return 0;
}