#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <cctype>
#include <fstream>
#include <sstream>

using namespace std;

enum TokenType {
    T_LABEL_DEF, T_DOT, T_MSUBLEQ, T_RSUBLEQ, T_LDORST, T_SEMI, 
    T_PLUS, T_MINUS, T_LPAREN, T_RPAREN, T_QUESTION, T_NUMBER, T_ID, T_EOF
};

struct Token {
    TokenType type;
    string str;
    int val;
};

string input_text;
int pos_idx = 0;
Token current_token;

void next_token() {
    while (pos_idx < input_text.length() && isspace(input_text[pos_idx])) {
        pos_idx++;
    }
    if (pos_idx >= input_text.length()) {
        current_token = {T_EOF, "", 0};
        return;
    }
    
    if (input_text[pos_idx] == '/' && pos_idx + 1 < input_text.length() && input_text[pos_idx+1] == '/') {
        while (pos_idx < input_text.length() && input_text[pos_idx] != '\n') pos_idx++;
        return next_token();
    }
    if (input_text[pos_idx] == '/' && pos_idx + 1 < input_text.length() && input_text[pos_idx+1] == '*') {
        pos_idx += 2;
        while (pos_idx + 1 < input_text.length() && !(input_text[pos_idx] == '*' && input_text[pos_idx+1] == '/')) {
            pos_idx++;
        }
        pos_idx += 2;
        return next_token();
    }

    char c = input_text[pos_idx];
    if (c == '.') { pos_idx++; current_token = {T_DOT, ".", 0}; return; }
    if (c == ';') { pos_idx++; current_token = {T_SEMI, ";", 0}; return; }
    if (c == '+') { pos_idx++; current_token = {T_PLUS, "+", 0}; return; }
    if (c == '-') { pos_idx++; current_token = {T_MINUS, "-", 0}; return; }
    if (c == '(') { pos_idx++; current_token = {T_LPAREN, "(", 0}; return; }
    if (c == ')') { pos_idx++; current_token = {T_RPAREN, ")", 0}; return; }
    if (c == '?') { pos_idx++; current_token = {T_QUESTION, "?", 0}; return; }
    
    if (isdigit(c)) {
        int val = 0;
        while (pos_idx < input_text.length() && isdigit(input_text[pos_idx])) {
            val = val * 10 + (input_text[pos_idx] - '0');
            pos_idx++;
        }
        current_token = {T_NUMBER, "", val};
        return;
    }
    
    if (isalpha(c) || c == '_') {
        string s = "";
        while (pos_idx < input_text.length() && (isalnum(input_text[pos_idx]) || input_text[pos_idx] == '_')) {
            s += input_text[pos_idx];
            pos_idx++;
        }
        int temp_idx = pos_idx;
        while (temp_idx < input_text.length() && isspace(input_text[temp_idx])) temp_idx++;
        if (temp_idx < input_text.length() && input_text[temp_idx] == ':') {
            pos_idx = temp_idx + 1;
            current_token = {T_LABEL_DEF, s, 0};
            return;
        }
        if (s == "msubleq") { current_token = {T_MSUBLEQ, s, 0}; return; }
        if (s == "rsubleq") { current_token = {T_RSUBLEQ, s, 0}; return; }
        if (s == "ldorst") { current_token = {T_LDORST, s, 0}; return; }
        current_token = {T_ID, s, 0};
        return;
    }
    
    pos_idx++;
    next_token();
}

enum ASTType { AST_NUM, AST_ID, AST_QUESTION, AST_PLUS, AST_MINUS, AST_NEG };

struct AST {
    ASTType type;
    int val;
    string name;
    AST* left;
    AST* right;
    
    AST(ASTType t, int v=0, string n="") : type(t), val(v), name(n), left(nullptr), right(nullptr) {}
    AST(ASTType t, AST* l, AST* r=nullptr) : type(t), val(0), name(""), left(l), right(r) {}
};

AST* parse_expression();
AST* parse_term();

AST* parse_term() {
    if (current_token.type == T_MINUS) {
        next_token();
        return new AST(AST_NEG, parse_term());
    } else if (current_token.type == T_LPAREN) {
        next_token();
        AST* node = parse_expression();
        if (current_token.type == T_RPAREN) next_token();
        return node;
    } else if (current_token.type == T_QUESTION) {
        next_token();
        return new AST(AST_QUESTION);
    } else if (current_token.type == T_NUMBER) {
        int v = current_token.val;
        next_token();
        return new AST(AST_NUM, v);
    } else if (current_token.type == T_ID) {
        string n = current_token.str;
        next_token();
        return new AST(AST_ID, 0, n);
    }
    next_token();
    return new AST(AST_NUM, 0);
}

AST* parse_expression() {
    AST* node = parse_term();
    while (current_token.type == T_PLUS || current_token.type == T_MINUS) {
        TokenType op = current_token.type;
        next_token();
        AST* right = parse_term();
        if (op == T_PLUS) node = new AST(AST_PLUS, node, right);
        else node = new AST(AST_MINUS, node, right);
    }
    return node;
}

struct MemoryCell {
    bool is_opcode;
    int opcode_val;
    AST* ast;
    int addr;
    int eval_addr;
};

vector<MemoryCell> mem;
map<string, int> label_map;
int current_addr = 0;

void parse_program() {
    next_token();
    while (current_token.type != T_EOF) {
        while (current_token.type == T_LABEL_DEF) {
            label_map[current_token.str] = current_addr;
            next_token();
        }
        if (current_token.type == T_EOF) break;
        
        if (current_token.type == T_DOT) {
            next_token();
            while (current_token.type != T_SEMI && current_token.type != T_EOF) {
                while (current_token.type == T_LABEL_DEF) {
                    label_map[current_token.str] = current_addr;
                    next_token();
                }
                if (current_token.type == T_SEMI || current_token.type == T_EOF) break;
                AST* ast = parse_expression();
                mem.push_back({false, 0, ast, current_addr, current_addr});
                current_addr++;
            }
            if (current_token.type == T_SEMI) next_token();
        } else if (current_token.type == T_MSUBLEQ || current_token.type == T_RSUBLEQ || current_token.type == T_LDORST) {
            int op_val = 0;
            if (current_token.type == T_MSUBLEQ) op_val = 0;
            if (current_token.type == T_RSUBLEQ) op_val = 1;
            if (current_token.type == T_LDORST) op_val = 2;
            
            mem.push_back({true, op_val, nullptr, current_addr, current_addr});
            current_addr++;
            next_token();
            
            vector<AST*> items;
            vector<int> item_addrs;
            while (current_token.type != T_SEMI && current_token.type != T_EOF) {
                while (current_token.type == T_LABEL_DEF) {
                    label_map[current_token.str] = current_addr;
                    next_token();
                }
                if (current_token.type == T_SEMI || current_token.type == T_EOF) break;
                AST* ast = parse_expression();
                items.push_back(ast);
                item_addrs.push_back(current_addr);
                current_addr++;
            }
            
            if (items.size() == 0) {
                mem.push_back({false, 0, new AST(AST_QUESTION), current_addr, current_addr}); current_addr++;
                mem.push_back({false, 0, new AST(AST_QUESTION), current_addr, current_addr}); current_addr++;
                mem.push_back({false, 0, new AST(AST_QUESTION), current_addr, current_addr}); current_addr++;
            } else if (items.size() == 1) {
                mem.push_back({false, 0, items[0], item_addrs[0], item_addrs[0]});
                mem.push_back({false, 0, items[0], current_addr, item_addrs[0]}); current_addr++;
                mem.push_back({false, 0, new AST(AST_QUESTION), current_addr, current_addr}); current_addr++;
            } else if (items.size() == 2) {
                mem.push_back({false, 0, items[0], item_addrs[0], item_addrs[0]});
                mem.push_back({false, 0, items[1], item_addrs[1], item_addrs[1]});
                mem.push_back({false, 0, new AST(AST_QUESTION), current_addr, current_addr}); current_addr++;
            } else {
                for (int i = 0; i < items.size(); i++) {
                    mem.push_back({false, 0, items[i], item_addrs[i], item_addrs[i]});
                }
            }
            if (current_token.type == T_SEMI) next_token();
        } else {
            next_token();
        }
    }
}

int eval_ast(AST* node, int eval_addr) {
    if (!node) return 0;
    if (node->type == AST_NUM) return node->val;
    if (node->type == AST_ID) {
        if (label_map.count(node->name)) return label_map[node->name];
        else return 0;
    }
    if (node->type == AST_QUESTION) return eval_addr + 1;
    if (node->type == AST_PLUS) return eval_ast(node->left, eval_addr) + eval_ast(node->right, eval_addr);
    if (node->type == AST_MINUS) return eval_ast(node->left, eval_addr) - eval_ast(node->right, eval_addr);
    if (node->type == AST_NEG) return -eval_ast(node->left, eval_addr);
    return 0;
}

int main() {
    ostringstream oss;
    oss << cin.rdbuf();
    input_text = oss.str();
    
    parse_program();
    
    for (int i = 0; i < mem.size(); i++) {
        if (mem[i].is_opcode) {
            cout << mem[i].opcode_val << " ";
        } else {
            cout << eval_ast(mem[i].ast, mem[i].eval_addr) << " ";
        }
    }
    cout << endl;
    
    return 0;
}
