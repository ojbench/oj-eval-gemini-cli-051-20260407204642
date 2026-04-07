#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <set>
#include <cctype>
#include <fstream>
#include <sstream>
#include <algorithm>

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

struct Instruction {
    int opcode; // -1 for ., 0 for msubleq, 1 for rsubleq, 2 for ldorst
    vector<AST*> items;
    int orig_addr;
    int orig_size;
    bool can_optimize;
    int new_addr;
};

vector<Instruction> instructions;
map<string, int> label_map_orig;
map<string, int> label_map_new;
int current_addr = 0;

void parse_program() {
    next_token();
    while (current_token.type != T_EOF) {
        while (current_token.type == T_LABEL_DEF) {
            label_map_orig[current_token.str] = current_addr;
            next_token();
        }
        if (current_token.type == T_EOF) break;
        
        if (current_token.type == T_DOT) {
            next_token();
            Instruction inst;
            inst.opcode = -1;
            inst.orig_addr = current_addr;
            while (current_token.type != T_SEMI && current_token.type != T_EOF) {
                while (current_token.type == T_LABEL_DEF) {
                    label_map_orig[current_token.str] = current_addr;
                    next_token();
                }
                if (current_token.type == T_SEMI || current_token.type == T_EOF) break;
                AST* ast = parse_expression();
                inst.items.push_back(ast);
                current_addr++;
            }
            inst.orig_size = inst.items.size();
            instructions.push_back(inst);
            if (current_token.type == T_SEMI) next_token();
        } else if (current_token.type == T_MSUBLEQ || current_token.type == T_RSUBLEQ || current_token.type == T_LDORST) {
            Instruction inst;
            if (current_token.type == T_MSUBLEQ) inst.opcode = 0;
            if (current_token.type == T_RSUBLEQ) inst.opcode = 1;
            if (current_token.type == T_LDORST) inst.opcode = 2;
            inst.orig_addr = current_addr;
            current_addr++;
            next_token();
            
            vector<AST*> parsed_items;
            while (current_token.type != T_SEMI && current_token.type != T_EOF) {
                while (current_token.type == T_LABEL_DEF) {
                    label_map_orig[current_token.str] = current_addr;
                    next_token();
                }
                if (current_token.type == T_SEMI || current_token.type == T_EOF) break;
                AST* ast = parse_expression();
                parsed_items.push_back(ast);
                current_addr++;
            }
            
            if (parsed_items.size() == 0) {
                inst.items.push_back(new AST(AST_QUESTION)); current_addr++;
                inst.items.push_back(new AST(AST_QUESTION)); current_addr++;
                inst.items.push_back(new AST(AST_QUESTION)); current_addr++;
            } else if (parsed_items.size() == 1) {
                inst.items.push_back(parsed_items[0]);
                inst.items.push_back(parsed_items[0]); current_addr++;
                inst.items.push_back(new AST(AST_QUESTION)); current_addr++;
            } else if (parsed_items.size() == 2) {
                inst.items.push_back(parsed_items[0]);
                inst.items.push_back(parsed_items[1]);
                inst.items.push_back(new AST(AST_QUESTION)); current_addr++;
            } else {
                for (int i = 0; i < parsed_items.size(); i++) {
                    inst.items.push_back(parsed_items[i]);
                }
            }
            inst.orig_size = 1 + inst.items.size();
            instructions.push_back(inst);
            if (current_token.type == T_SEMI) next_token();
        } else {
            next_token();
        }
    }
}

int eval_ast(AST* node, int eval_addr, bool use_orig) {
    if (!node) return 0;
    if (node->type == AST_NUM) return node->val;
    if (node->type == AST_ID) {
        if (use_orig) return label_map_orig[node->name];
        else return label_map_new[node->name];
    }
    if (node->type == AST_QUESTION) return eval_addr + 1;
    if (node->type == AST_PLUS) return eval_ast(node->left, eval_addr, use_orig) + eval_ast(node->right, eval_addr, use_orig);
    if (node->type == AST_MINUS) return eval_ast(node->left, eval_addr, use_orig) - eval_ast(node->right, eval_addr, use_orig);
    if (node->type == AST_NEG) return -eval_ast(node->left, eval_addr, use_orig);
    return 0;
}

bool contains_question(AST* node) {
    if (!node) return false;
    if (node->type == AST_QUESTION) return true;
    return contains_question(node->left) || contains_question(node->right);
}

int main() {
    ostringstream oss;
    oss << cin.rdbuf();
    input_text = oss.str();
    
    parse_program();
    
    vector<pair<int, int>> unoptimizable_ranges;
    set<int> target_orig_addrs;
    
    for (auto& inst : instructions) {
        int start_idx = (inst.opcode == -1) ? 0 : 1;
        for (int i = 0; i < inst.items.size(); i++) {
            if (i == 2 && inst.opcode != -1 && inst.items[i]->type == AST_QUESTION) continue;
            if (contains_question(inst.items[i])) {
                int eval_addr = inst.orig_addr + start_idx + i;
                int target = eval_ast(inst.items[i], eval_addr, true);
                target_orig_addrs.insert(target);
                int src = inst.orig_addr;
                unoptimizable_ranges.push_back({min(src, target), max(src, target)});
            }
        }
    }
    
    for (auto& inst : instructions) {
        inst.can_optimize = true;
        if (inst.opcode != 0) inst.can_optimize = false;
        
        bool has_complex_question = false;
        for (int i = 0; i < inst.items.size(); i++) {
            if (i == 2 && inst.items[i]->type == AST_QUESTION) continue;
            if (contains_question(inst.items[i])) has_complex_question = true;
        }
        if (has_complex_question) inst.can_optimize = false;
        
        for (int i = 0; i < inst.orig_size; i++) {
            if (target_orig_addrs.count(inst.orig_addr + i)) {
                inst.can_optimize = false;
            }
        }
        
        for (auto& range : unoptimizable_ranges) {
            if (inst.orig_addr >= range.first && inst.orig_addr <= range.second) {
                inst.can_optimize = false;
            }
        }
        
        if (inst.can_optimize) {
            int a_val = eval_ast(inst.items[0], inst.orig_addr + 1, true);
            int b_val = eval_ast(inst.items[1], inst.orig_addr + 2, true);
            if (a_val < 0 || b_val < 0) inst.can_optimize = false;
        }
    }
    
    int current_new_addr = 0;
    map<int, int> orig_to_new_addr;
    for (auto& inst : instructions) {
        inst.new_addr = current_new_addr;
        if (inst.can_optimize) {
            for (int i = 0; i < inst.orig_size; i++) {
                orig_to_new_addr[inst.orig_addr + i] = inst.new_addr + i;
            }
            current_new_addr += 28;
        } else {
            for (int i = 0; i < inst.orig_size; i++) {
                orig_to_new_addr[inst.orig_addr + i] = inst.new_addr + i;
            }
            current_new_addr += inst.orig_size;
        }
    }
    
    for (auto& kv : label_map_orig) {
        if (orig_to_new_addr.count(kv.second)) {
            label_map_new[kv.first] = orig_to_new_addr[kv.second];
        } else {
            label_map_new[kv.first] = current_new_addr;
        }
    }
    
    vector<int> output;
    for (auto& inst : instructions) {
        if (inst.can_optimize) {
            int X = inst.new_addr;
            int a_val = eval_ast(inst.items[0], X + 1, false);
            int b_val = eval_ast(inst.items[1], X + 5, false);
            int c_val;
            if (inst.items[2]->type == AST_QUESTION) {
                c_val = X + 28;
            } else {
                c_val = eval_ast(inst.items[2], X + 24, false);
            }
            
            output.push_back(2); output.push_back(a_val); output.push_back(1); output.push_back(0);
            output.push_back(2); output.push_back(b_val); output.push_back(2); output.push_back(0);
            output.push_back(1); output.push_back(1); output.push_back(2); output.push_back(X + 20);
            output.push_back(2); output.push_back(b_val); output.push_back(2); output.push_back(1);
            output.push_back(1); output.push_back(3); output.push_back(3); output.push_back(X + 28);
            output.push_back(2); output.push_back(b_val); output.push_back(2); output.push_back(1);
            output.push_back(1); output.push_back(3); output.push_back(3); output.push_back(c_val);
        } else {
            if (inst.opcode != -1) {
                output.push_back(inst.opcode);
                for (int i = 0; i < inst.items.size(); i++) {
                    output.push_back(eval_ast(inst.items[i], inst.new_addr + 1 + i, false));
                }
            } else {
                for (int i = 0; i < inst.items.size(); i++) {
                    output.push_back(eval_ast(inst.items[i], inst.new_addr + i, false));
                }
            }
        }
    }
    
    for (int i = 0; i < output.size(); i++) {
        cout << output[i] << (i + 1 == output.size() ? "" : " ");
    }
    cout << endl;
    
    return 0;
}