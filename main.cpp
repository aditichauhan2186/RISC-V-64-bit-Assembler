
//RISC V Assembler
#include <bits/stdc++.h>
using namespace std;
using u32 = uint32_t;
using u64 = uint64_t;
using i32 = int32_t;
using u8  = uint8_t;

//Utilities 
static string trim(const string &s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == string::npos) return "";
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}
static string lower(const string &s) {
    string r = s;
    transform(r.begin(), r.end(), r.begin(), [](unsigned char c){ return tolower(c); });
    return r;
}
static string stripComment(const string &line) {
    size_t p = line.find('#');
    if (p != string::npos) return line.substr(0, p);
    p = line.find("//");
    if (p != string::npos) return line.substr(0, p);
    return line;
}
static vector<string> tokenize(const string &line) {
    string s = stripComment(line);
    s = trim(s);
    vector<string> out;
    string cur;
    for (size_t i = 0; i < s.size(); ++i) {
        char c = s[i];
        if (isspace((unsigned char)c)) {
            if (!cur.empty()) { 
                while (!cur.empty() && cur.back() == ',') cur.pop_back();
                if (!cur.empty()) out.push_back(cur);
                cur.clear();
            }
        } else cur.push_back(c);
    }
    if (!cur.empty()) {
        while (!cur.empty() && cur.back() == ',') cur.pop_back();
        if (!cur.empty()) out.push_back(cur);
    }
    return out;
}
static bool isLabelToken(const string &t) { return !t.empty() && t.back() == ':'; }
static int regToNum(const string &r) {
    string s = r;
    s.erase(remove_if(s.begin(), s.end(), [](char c){ return isspace((unsigned char)c) || c==','; }), s.end());
    if (s.empty()) return 0;
    if (s[0]=='x' || s[0]=='X' || s[0]=='r' || s[0]=='R') s = s.substr(1);
    try { return stoi(s); } catch(...) { return 0; }
}
static i32 parseImm(const string &t) {
    string s = t;
    s.erase(remove(s.begin(), s.end(), ','), s.end());
    if (s.size()>2 && s[0]=='0' && (s[1]=='x' || s[1]=='X')) return (i32)strtol(s.c_str(), nullptr, 16);
    try { return stoi(s); } catch(...) { return 0; }
}
static string toHex32(u32 v) { char buf[12]; sprintf(buf, "0x%08X", v); return string(buf); }
static string addrHex(u32 a)    { char buf[20]; sprintf(buf, "0x%X", a); return string(buf); }

//Symbol table
struct SymbolTable {
    unordered_map<string,u32> table;
    void add(const string &lbl, u32 addr) { table[lbl] = addr; }
    bool has(const string &lbl) const { return table.find(lbl) != table.end(); }
    u32 get(const string &lbl) const {
        auto it = table.find(lbl);
        if (it == table.end()) throw runtime_error("Undefined label: " + lbl);
        return it->second;
    }
};

//Data entry
struct DataEntry {
    u32 addr;
    vector<uint8_t> bytes;
    string comment;
};

//Encoders(R/I/S/SB/U/UJ)
static u32 encodeR(u8 funct7, u8 rs2, u8 rs1, u8 funct3, u8 rd, u8 opcode) {
    return ((u32)funct7 << 25) | ((u32)rs2 << 20) | ((u32)rs1 << 15) | ((u32)funct3 << 12) | ((u32)rd << 7) | (u32)opcode;
}
static u32 encodeI(i32 imm, u8 rs1, u8 funct3, u8 rd, u8 opcode) {
    u32 imm12 = (u32)imm & 0xFFF;
    return (imm12 << 20) | ((u32)rs1 << 15) | ((u32)funct3 << 12) | ((u32)rd << 7) | (u32)opcode;
}
static u32 encodeS(i32 imm, u8 rs2, u8 rs1, u8 funct3, u8 opcode) {
    u32 imm12 = (u32)imm & 0xFFF;
    u32 imm11_5 = (imm12 >> 5) & 0x7F;
    u32 imm4_0  = imm12 & 0x1F;
    return (imm11_5 << 25) | ((u32)rs2 << 20) | ((u32)rs1 << 15) | ((u32)funct3 << 12) | (imm4_0 << 7) | (u32)opcode;
}
static u32 encodeSB(i32 imm, u8 rs2, u8 rs1, u8 funct3, u8 opcode) {
    u32 imm13 = (u32)imm & 0x1FFF;
    u32 imm12 = (imm13 >> 12) & 0x1;
    u32 imm10_5 = (imm13 >> 5) & 0x3F;
    u32 imm4_1 = (imm13 >> 1) & 0xF;
    u32 imm11 = (imm13 >> 11) & 0x1;
    return (imm12 << 31) | (imm10_5 << 25) | ((u32)rs2 << 20) | ((u32)rs1 << 15) | ((u32)funct3 << 12) | (imm4_1 << 8) | (imm11 << 7) | (u32)opcode;
}
static u32 encodeU(i32 imm, u8 rd, u8 opcode) {
    u32 imm20 = (u32)imm & 0xFFFFF000;
    return imm20 | ((u32)rd << 7) | (u32)opcode;
}
static u32 encodeUJ(i32 imm, u8 rd, u8 opcode) {
    u32 imm21 = (u32)imm & 0x1FFFFF;
    u32 bit20 = (imm21 >> 20) & 1;
    u32 bits10_1 = (imm21 >> 1) & 0x3FF;
    u32 bit11 = (imm21 >> 11) & 1;
    u32 bits19_12 = (imm21 >> 12) & 0xFF;
    return (bit20 << 31) | (bits10_1 << 21) | (bit11 << 20) | (bits19_12 << 12) | ((u32)rd << 7) | (u32)opcode;
}

//Op table
struct OpInfo { u8 opcode; u8 funct3; u8 funct7; string fmt; };
static unordered_map<string,OpInfo> buildOpMap() {
    unordered_map<string,OpInfo> m;
    auto a=[&](string name,u8 op,u8 f3,u8 f7,string fmt){ m[name]={op,f3,f7,fmt}; };
    // R format
    a("add",0x33,0x0,0x00,"R"); a("sub",0x33,0x0,0x20,"R"); a("and",0x33,0x7,0x00,"R");
    a("or",0x33,0x6,0x00,"R"); a("sll",0x33,0x1,0x00,"R"); a("slt",0x33,0x2,0x00,"R");
    a("sra",0x33,0x5,0x20,"R"); a("srl",0x33,0x5,0x00,"R"); a("xor",0x33,0x4,0x00,"R");
    // M ext
    a("mul",0x33,0x0,0x01,"R"); a("div",0x33,0x4,0x01,"R"); a("rem",0x33,0x6,0x01,"R");
    // W variants
    a("addw",0x3B,0x0,0x00,"R"); a("subw",0x3B,0x0,0x20,"R");
    a("sllw",0x3B,0x1,0x00,"R"); a("srlw",0x3B,0x5,0x00,"R"); a("sraw",0x3B,0x5,0x20,"R");
    a("mulw",0x3B,0x0,0x01,"R"); a("divw",0x3B,0x4,0x01,"R"); a("remw",0x3B,0x6,0x01,"R");
    // I format arithmetic
    a("addi",0x13,0x0,0x00,"I"); a("addiw",0x1B,0x0,0x00,"I"); a("andi",0x13,0x7,0x00,"I"); a("ori",0x13,0x6,0x00,"I");
    // loads 
    a("lb",0x03,0x0,0x00,"I"); a("lh",0x03,0x1,0x00,"I"); a("lw",0x03,0x2,0x00,"I"); a("ld",0x03,0x3,0x00,"I");
    // jalr
    a("jalr",0x67,0x0,0x00,"I");
    // S-type stores
    a("sb",0x23,0x0,0x00,"S"); a("sh",0x23,0x1,0x00,"S"); a("sw",0x23,0x2,0x00,"S"); a("sd",0x23,0x3,0x00,"S");
    // Branches
    a("beq",0x63,0x0,0x00,"SB"); a("bne",0x63,0x1,0x00,"SB"); a("bge",0x63,0x5,0x00,"SB"); a("blt",0x63,0x4,0x00,"SB");
    // U
    a("lui",0x37,0x0,0x00,"U"); a("auipc",0x17,0x0,0x00,"U");
    // UJ
    a("jal",0x6F,0x0,0x00,"UJ");
    return m;
}
static unordered_map<string,OpInfo> opmap = buildOpMap();

//Main two-pass assembler
int main(int argc, char** argv) {
    string inname = "input.asm";
    string outname = "output.mc";
    if (argc >= 2) inname = argv[1];

    ifstream fin(inname);
    if (!fin.is_open()) { cerr << "Error: cannot open " << inname << endl; return 1; }

    const u32 TEXT_BASE = 0x00000000u;
    const u32 DATA_BASE = 0x10000000u;

    vector<string> lines;
    string raw;
    while (getline(fin, raw)) lines.push_back(raw);
    fin.close();

    SymbolTable sym;
    bool inText=false, inData=false;
    u32 pc = TEXT_BASE, dataPtr = 0;
    for (size_t i=0;i<lines.size();++i) {
        string line = trim(stripComment(lines[i]));
        if (line.empty()) continue;
        auto toks = tokenize(lines[i]);
        if (toks.empty()) continue;
        if (isLabelToken(toks[0])) {
            string lbl = toks[0].substr(0, toks[0].size()-1);
            if (inText) sym.add(lbl, pc);
            else if (inData) sym.add(lbl, DATA_BASE + dataPtr);
            toks.erase(toks.begin());
            if (toks.empty()) continue;
        }
        string op = lower(toks[0]);
        if (op == ".text") { inText=true; inData=false; continue; }
        if (op == ".data") { inData=true; inText=false; continue; }

        if (inText) {
            pc += 4; 
        } else if (inData) {
            if (op == ".byte") dataPtr += (u32)max(0, (int)(toks.size()-1));
            else if (op == ".half") dataPtr += 2*(u32)max(0, (int)(toks.size()-1));
            else if (op == ".word") dataPtr += 4*(u32)max(0, (int)(toks.size()-1));
            else if (op == ".dword") dataPtr += 8*(u32)max(0, (int)(toks.size()-1));
            else if (op == ".asciz") {
                string s; for (size_t k=1;k<toks.size();++k) { if (k>1) s+=' '; s+=toks[k]; }
                if (!s.empty() && s.front()=='"') s.erase(0,1);
                if (!s.empty() && s.back()=='"') s.pop_back();
                dataPtr += (u32)(s.size()+1);
            } else {
            }
        }
    }

    ofstream fout(outname);
    if (!fout.is_open()) { cerr << "Error: cannot open " << outname << " for writing\n"; return 1; }
    inText=false; inData=false;
    pc = TEXT_BASE; dataPtr = 0;
    vector<DataEntry> dataEntries;

    for (size_t i=0;i<lines.size();++i) {
        auto toks = tokenize(lines[i]);
        if (toks.empty()) continue;
        if (isLabelToken(toks[0])) { toks.erase(toks.begin()); if (toks.empty()) continue; }
        if (toks.empty()) continue;

        string op = lower(toks[0]);
        if (op == ".text") { inText=true; inData=false; continue; }
        if (op == ".data") { inData=true; inText=false; continue; }

        if (inText) {
            try {
                if (opmap.find(op) == opmap.end()) {
                    fout << addrHex(pc) << " 0x00000000 , unknown: " << toks[0] << "\n";
                    pc += 4; continue;
                }
                OpInfo oi = opmap[op];
                u32 word = 0;
                string asmcomment;
                if (oi.fmt == "R") {
                    if (toks.size() < 4) throw runtime_error("R requires rd, rs1, rs2");
                    int rd = regToNum(toks[1]);
                    int rs1 = regToNum(toks[2]);
                    int rs2 = regToNum(toks[3]);
                    word = encodeR((u8)oi.funct7, (u8)rs2, (u8)rs1, (u8)oi.funct3, (u8)rd, (u8)oi.opcode);
                    asmcomment = toks[0] + " " + toks[1] + "," + toks[2] + "," + toks[3];
                } else if (oi.fmt == "I") {
                    if ((oi.opcode == 0x03) || op=="jalr") {
                        if (toks.size() >= 3 && toks[2].find('(') != string::npos) {
                            int rd = regToNum(toks[1]);
                            string tok = toks[2];
                            size_t p = tok.find('(');
                            string immStr = tok.substr(0,p);
                            string rs1s = tok.substr(p+1, tok.find(')') - p - 1);
                            int rs1 = regToNum(rs1s);
                            int imm = parseImm(immStr);
                            word = encodeI(imm, (u8)rs1, (u8)oi.funct3, (u8)rd, (u8)oi.opcode);
                            asmcomment = toks[0] + " " + toks[1] + "," + toks[2];
                        } else {
                            if (toks.size() < 4) throw runtime_error("I-format needs rd, rs1, imm or rd, imm(rs1)");
                            int rd = regToNum(toks[1]);
                            int rs1 = regToNum(toks[2]);
                            int imm = parseImm(toks[3]);
                            word = encodeI(imm, (u8)rs1, (u8)oi.funct3, (u8)rd, (u8)oi.opcode);
                            asmcomment = toks[0] + " " + toks[1] + "," + toks[2] + "," + toks[3];
                        }
                    } else {
                        if (toks.size() < 4) throw runtime_error("I-format arithmetic needs rd, rs1, imm");
                        int rd = regToNum(toks[1]);
                        int rs1 = regToNum(toks[2]);
                        int imm = parseImm(toks[3]);
                        word = encodeI(imm, (u8)rs1, (u8)oi.funct3, (u8)rd, (u8)oi.opcode);
                        asmcomment = toks[0] + " " + toks[1] + "," + toks[2] + "," + toks[3];
                    }
                } else if (oi.fmt == "S") {
                    if (toks.size() < 3) throw runtime_error("S needs rs2, imm(rs1)");
                    int rs2 = regToNum(toks[1]);
                    string tok = toks[2];
                    if (tok.find('(') == string::npos) throw runtime_error("S expects imm(rs1)");
                    size_t p = tok.find('(');
                    string immStr = tok.substr(0,p);
                    string rs1s = tok.substr(p+1, tok.find(')') - p - 1);
                    int rs1 = regToNum(rs1s);
                    int imm = parseImm(immStr);
                    word = encodeS(imm, (u8)rs2, (u8)rs1, (u8)oi.funct3, (u8)oi.opcode);
                    asmcomment = toks[0] + " " + toks[1] + "," + toks[2];
                } else if (oi.fmt == "SB") {
                    if (toks.size() < 4) throw runtime_error("SB needs rs1, rs2, label");
                    int rs1 = regToNum(toks[1]);
                    int rs2 = regToNum(toks[2]);
                    string lbl = toks[3];
                    if (!sym.has(lbl)) throw runtime_error("Undefined label: " + lbl);
                    u32 tgt = sym.get(lbl);
                    i32 offset = (i32)tgt - (i32)pc;
                    word = encodeSB(offset, (u8)rs2, (u8)rs1, (u8)oi.funct3, (u8)oi.opcode);
                    asmcomment = toks[0] + " " + toks[1] + "," + toks[2] + "," + toks[3];
                } else if (oi.fmt == "U") {
                    if (toks.size() < 3) throw runtime_error("U needs rd, imm20");
                    int rd = regToNum(toks[1]);
                    i32 imm = parseImm(toks[2]);
                    word = encodeU((i32)(imm << 12), (u8)rd, (u8)oi.opcode);
                    asmcomment = toks[0] + " " + toks[1] + "," + toks[2];
                } else if (oi.fmt == "UJ") {
                    if (toks.size() < 3) throw runtime_error("UJ needs rd, label");
                    int rd = regToNum(toks[1]);
                    string lbl = toks[2];
                    if (!sym.has(lbl)) throw runtime_error("Undefined label: " + lbl);
                    u32 tgt = sym.get(lbl);
                    i32 offset = (i32)tgt - (i32)pc;
                    word = encodeUJ(offset, (u8)rd, (u8)oi.opcode);
                    asmcomment = toks[0] + " " + toks[1] + "," + toks[2];
                } else {
                    throw runtime_error("Unknown format");
                }

                fout << addrHex(pc) << " " << toHex32(word) << " , " << asmcomment;
                fout << " # " << bitset<7>(oi.opcode) << "-" << bitset<3>(oi.funct3) << "-" << bitset<7>(oi.funct7) << "\n";
            } catch (const exception &e) {
                fout << addrHex(pc) << " 0x00000000 , error: " << e.what() << "\n";
            }
            pc += 4;
        } else if (inData) {
            if (op == ".byte") {
                for (size_t j=1;j<toks.size();++j) {
                    int v = parseImm(toks[j]);
                    DataEntry d; d.addr = DATA_BASE + dataPtr; d.bytes = { (uint8_t)(v & 0xFF) }; d.comment = toks[j];
                    dataEntries.push_back(d); dataPtr += 1;
                }
            } else if (op == ".half") {
                for (size_t j=1;j<toks.size();++j) {
                    int v = parseImm(toks[j]);
                    DataEntry d; d.addr = DATA_BASE + dataPtr;
                    d.bytes = { (uint8_t)(v & 0xFF), (uint8_t)((v>>8)&0xFF) }; d.comment = toks[j];
                    dataEntries.push_back(d); dataPtr += 2;
                }
            } else if (op == ".word") {
                for (size_t j=1;j<toks.size();++j) {
                    int v = parseImm(toks[j]);
                    DataEntry d; d.addr = DATA_BASE + dataPtr;
                    d.bytes = { (uint8_t)(v & 0xFF), (uint8_t)((v>>8)&0xFF), (uint8_t)((v>>16)&0xFF), (uint8_t)((v>>24)&0xFF) };
                    d.comment = toks[j];
                    dataEntries.push_back(d); dataPtr += 4;
                }
            } else if (op == ".dword") {
                for (size_t j=1;j<toks.size();++j) {
                    long long v = (long long)parseImm(toks[j]);
                    DataEntry d; d.addr = DATA_BASE + dataPtr;
                    for (int b=0;b<8;++b) d.bytes.push_back((uint8_t)((v >> (8*b)) & 0xFF));
                    d.comment = toks[j];
                    dataEntries.push_back(d); dataPtr += 8;
                }
            } else if (op == ".asciz") {
                string s;
                for (size_t j=1;j<toks.size();++j) { if (j>1) s += " "; s += toks[j]; }
                if (!s.empty() && s.front()=='"') s.erase(0,1);
                if (!s.empty() && s.back()=='"') s.pop_back();
                DataEntry d; d.addr = DATA_BASE + dataPtr;
                for (char c : s) d.bytes.push_back((uint8_t)c);
                d.bytes.push_back(0);
                d.comment = s;
                dataEntries.push_back(d); dataPtr += (u32)(s.size()+1);
            } else {
            }
        } else {
        }
    }

    for (auto &d : dataEntries) {
        fout << addrHex(d.addr) << " ";
        if (d.bytes.size() <= 8) {
            u64 v = 0;
            for (size_t b=0;b<d.bytes.size();++b) v |= ((u64)d.bytes[b]) << (8*b);
            char buf[40]; sprintf(buf, "0x%0*llX", (int)(d.bytes.size()*2), (unsigned long long)v);
            fout << buf;
        } else {
            fout << "\"";
            for (auto &bb : d.bytes) {
                if (bb==0) fout << "\\0"; else fout << (char)bb;
            }
            fout << "\"";
        }
        fout << " , " << d.comment << "\n";
    }

    fout.close();
    cout << "Assembly to machine code conversion complete!" << endl;
    return 0;
}