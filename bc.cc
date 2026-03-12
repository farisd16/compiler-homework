#include <cstdio>
#include <unistd.h>
#include <stdlib.h>
#include <string_view>
#include <cctype>
#include <vector>
#include <optional>
#include <iostream>
#include <regex>
#include <cstdint>
#include <unordered_map>

using namespace std::literals;

struct SymbolInfo {
    enum StorageClass { UNKNOWN, AUTO, REGISTER, PARAMETER };

    StorageClass storage;
    int depth;
};

struct FunctionInfo {
    int num_of_params;
    bool is_explicitly_defined;
};

struct Token {
    enum Kind {
        AUTO, REGISTER, IF, ELSE, WHILE, RETURN,

        IDENT, NUMBER,

        LPAREN, RPAREN,
        LBRACE, RBRACE,
        LBRACKET, RBRACKET,
        COMMA, SEMICOLON, AT,

        ASSIGN, EQ, NEQ,
        LT, GT,
        PLUS, MINUS,
        AND, OR,
        BANG, TILDE, ADDRESSOF,

        _EOF, ERROR
    };

    std::string_view v; 
    Kind kind;
};

struct Node;
using NodePtr = Node*;

struct Node {
    enum Kind {
        WhileStmt, ExprStmt, DeclStmt, ReturnStmt, IfStmt,

        PROGRAM, BLOCK, FUNCTION, PARAMLIST,
        
        IDENT, NUMBER, SIZESPEC, ARGLIST,
        
        AUTO, REGISTER, IF, ELSE, WHILE,

        FUNCCALL, SUBSCRIPT,

        UMINUS, COMMA, LOGICALNOT, BITWISENOT, ADD, SUBTRACT, LT, GT, EQUAL, NOTEQUAL, LOGICALOR, LOGICALAND, ASSIGNMENT, ADDRESSOF
    };

    Kind kind;
    std::vector<NodePtr> children;
    std::optional<std::string> name;
    std::optional<int64_t> value;

    Node(Kind k, std::initializer_list<NodePtr> c) 
        : kind(k), children(c) {}

    Node(Kind k, std::string_view s) 
        : kind(k), name(std::string(s)) {}

    Node(Kind k, int64_t v) 
        : kind(k), value(v) {}
    
    Node(Kind k) : kind(k) {}
};

NodePtr makeNode(Node::Kind kind, std::initializer_list<NodePtr> children) {
    return new Node(kind, children);
}

NodePtr makeNode(Node::Kind kind, std::string_view name) {
    return new Node(kind, name);
}

NodePtr makeNode(Node::Kind kind, int64_t value) {
    return new Node(kind, value);
}

NodePtr makeNode(Node::Kind kind) {
    return new Node(kind);
}

size_t leading_spaces_acc = 0;
std::vector<Token> tokens;
NodePtr ast;

using SymbolTable = std::unordered_map<std::string, std::vector<SymbolInfo>>;
using FunctionTable = std::unordered_map<std::string, FunctionInfo>;

#define UNARY_PREC 13

SymbolInfo* findSymbol(const std::string& name, int current_depth, SymbolTable& table) {
    if (!table.count(name)) return nullptr;

    auto& declarations = table.at(name);

    for (auto it = declarations.rbegin(); it != declarations.rend(); it++) {
        if (it->depth <= current_depth) {
            return &(*it); 
        }
    }

    return nullptr;
}

std::optional<Token::Kind> isKeyword(std::string_view v) {
    if (v == "auto") return Token::AUTO;
    if (v == "register") return Token::REGISTER;
    if (v == "return") return Token::RETURN;
    if (v == "while") return Token::WHILE;
    if (v == "if") return Token::IF;
    if (v == "else") return Token::ELSE;
    return std::nullopt;
}

Token next(std::string_view v) {
    if (v.empty()) return Token{v, Token::_EOF};
    if (v.starts_with("//")) {
        size_t end_of_line = v.find("\n") != std::string::npos ? v.find("\n") : v.size() - 1;
        leading_spaces_acc += end_of_line;
        return next(v.substr(end_of_line));
    }
    if (v.starts_with("==")) return Token{"=="sv, Token::EQ};
    if (v.starts_with("!=")) return Token{"!="sv, Token::NEQ};
    if (v.starts_with("&&")) return Token{"&&"sv, Token::AND};
    if (v.starts_with("||")) return Token{"||"sv, Token::OR};
    if (v.starts_with("=")) return Token{"="sv, Token::ASSIGN};
    if (v.starts_with("+")) return Token{"+"sv, Token::PLUS};
    if (v.starts_with("-")) return Token{"-"sv, Token::MINUS};
    if (v.starts_with("<")) return Token{"<"sv, Token::LT};
    if (v.starts_with(">")) return Token{">"sv, Token::GT};
    if (v.starts_with("!")) return Token{"!"sv, Token::BANG};
    if (v.starts_with("~")) return Token{"~"sv, Token::TILDE};
    if (v.starts_with("&")) return Token{"&"sv, Token::ADDRESSOF};

    if (v.starts_with("(")) return Token{"("sv, Token::LPAREN};
    if (v.starts_with(")")) return Token{")"sv, Token::RPAREN};
    if (v.starts_with("{")) return Token{"{"sv, Token::LBRACE};
    if (v.starts_with("}")) return Token{"}"sv, Token::RBRACE};
    if (v.starts_with("{")) return Token{"{"sv, Token::LBRACE};
    if (v.starts_with("[")) return Token{"["sv, Token::LBRACKET};
    if (v.starts_with("]")) return Token{"]"sv, Token::RBRACKET};
    if (v.starts_with(",")) return Token{","sv, Token::COMMA};
    if (v.starts_with(";")) return Token{";"sv, Token::SEMICOLON};
    if (v.starts_with("@")) return Token{"@"sv, Token::AT};

    char c = v[0];
    if (c == ' ' || c == '\n' || c == '\t') {
        leading_spaces_acc++;
        return next(v.substr(1));
    } else if (std::isalpha(c) || c == '_') {
        size_t end_of_word = v.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_");
        
        if (end_of_word == std::string::npos) {
            end_of_word = v.size() - 1;
        }

        std::string_view first_word = v.substr(0, end_of_word);
        Token t = {first_word, Token::IDENT};

        if (auto kind = isKeyword(t.v)) return Token{t.v, *kind};

        return t;
    } else if (std::isdigit(c)) {
        size_t end_of_word = v.find_first_not_of("0123456789");

        if (end_of_word == std::string::npos) {
            end_of_word = v.size() - 1;
        }

        std::string_view first_word = v.substr(0, end_of_word);

        return Token{first_word, Token::NUMBER};
    } else {
        return Token{v.substr(0, 1), Token::ERROR};
    }
}

std::vector<Token> tokenize(std::string_view input) {
    std::vector<Token> tokens;
    Token t = next(input);

    do {
        if (t.kind == Token::ERROR) {
            std::cerr << "encountered error token\n";
            exit(EXIT_FAILURE);
        }

        tokens.push_back(t);
        input.remove_prefix(t.v.size() + leading_spaces_acc);
        leading_spaces_acc = 0;
        t = next(input);
    } while (t.kind != Token::_EOF);

    return tokens;
}

static void *exit_on_null(const char *msg, void *res) {
    return res ? res : (void *)(perror(msg), exit(EXIT_FAILURE), NULL);
}

char *readfile(const char *name) {
    FILE *f = (FILE *)exit_on_null(name, fopen(name, "r"));
    size_t size = 0x1000;

    if (!fseek(f, 0, SEEK_END)) {
        size = ftell(f);
        fseek(f, 0, SEEK_SET);
    }

    char *prog = (char *)exit_on_null(NULL, malloc(size + 1));
    size_t cur = 0;

    while (true) {
        cur += fread(prog + cur, 1, size - cur + 1, f);
        if (ferror(f) || feof(f)) {
            break;
        }
        size = cur + 0x1000;
        prog = (char *)exit_on_null(NULL, realloc(prog, size + 1));
    }

    fclose(f);
    prog[cur] = 0;

    return prog;
}

void consume(Token::Kind token_kind) {
    if (!tokens.empty()) {
        Token first_token = tokens.front();
        tokens.erase(tokens.begin());

        if (first_token.kind != token_kind) {
            std::cerr << "expected " << token_kind << " but encountered " << first_token.kind << "\n";
            exit(EXIT_FAILURE);   
        }
    }
}

Token peekToken() {
    if (tokens.empty()) return { ""sv, Token::_EOF }; 
    return tokens.front();
}

Token next() {
    if (tokens.empty()) return { ""sv, Token::_EOF };
    Token first_token = tokens.front();
    tokens.erase(tokens.begin());
    return first_token;
}

struct OpDesc {
    unsigned int prec;
    bool rassoc;
    Node::Kind node_kind;
};

OpDesc OPS_BINARY[Token::_EOF];
OpDesc OPS_UNARY[Token::_EOF];

void initializeOperatorTables() {
    OPS_BINARY[Token::LBRACKET] = {14, false, Node::SUBSCRIPT};
    OPS_BINARY[Token::PLUS]     = {11, false, Node::ADD};
    OPS_BINARY[Token::MINUS]    = {11, false, Node::SUBTRACT};
    OPS_BINARY[Token::LT]       = {9,  false, Node::LT};
    OPS_BINARY[Token::GT]       = {9,  false, Node::GT};
    OPS_BINARY[Token::EQ]       = {8,  false, Node::EQUAL};
    OPS_BINARY[Token::NEQ]      = {8,  false, Node::NOTEQUAL};
    OPS_BINARY[Token::AND]      = {4,  false, Node::LOGICALAND};
    OPS_BINARY[Token::OR]       = {3,  false, Node::LOGICALOR};
    OPS_BINARY[Token::ASSIGN]   = {1,  true,  Node::ASSIGNMENT};
    
    OPS_UNARY[Token::BANG]      = {13, true, Node::LOGICALNOT};
    OPS_UNARY[Token::TILDE]     = {13, true, Node::BITWISENOT};
    OPS_UNARY[Token::MINUS]     = {13, true, Node::UMINUS};
    OPS_UNARY[Token::ADDRESSOF] = {13, true, Node::ADDRESSOF};
}

NodePtr parseParamList() {
    NodePtr params = makeNode(Node::PARAMLIST);
    
    if (peekToken().kind == Token::RPAREN) {
        return params;
    }

    do {
        Token p = next();
        if (p.kind != Token::IDENT) {
            std::cerr << "Expected parameter name (identifier)\n";
            exit(EXIT_FAILURE);
        }
        params->children.push_back(makeNode(Node::IDENT, p.v));

        if (peekToken().kind == Token::COMMA) {
            consume(Token::COMMA);
        } else {
            break;
        }
    } while (true);

    return params;
}

NodePtr parseExpr(unsigned minPrec = 1);
NodePtr parseDeclStmt();
NodePtr parseWhileStmt();
NodePtr parseIfStmt();
NodePtr parseReturnStmt();

NodePtr parseBlock() {
    consume(Token::LBRACE);
    NodePtr block = makeNode(Node::BLOCK);

    while (peekToken().kind != Token::RBRACE) {
        block->children.push_back(parseDeclStmt());
    }

    consume(Token::RBRACE);
    return block;
}

NodePtr parseFunction() {
    Token func_name = next();
    if (func_name.kind != Token::IDENT) {
        std::cerr << "expected function name (identifier), but got " << func_name.kind << std::endl;;
        exit(EXIT_FAILURE);
    }

    consume(Token::LPAREN);
    NodePtr param_list = parseParamList();
    consume(Token::RPAREN);
    NodePtr body = parseBlock();

    NodePtr func_node = makeNode(Node::FUNCTION, {param_list, body});
    func_node->name = std::string(func_name.v); 
    
    return func_node;
}

NodePtr parseProgram() {
    NodePtr program = makeNode(Node::PROGRAM);
    
    while (peekToken().kind != Token::_EOF) {
        program->children.push_back(parseFunction());
    }
    
    return program;
}

NodePtr parseStmt() {
    switch (peekToken().kind) {
        case Token::WHILE:
            return parseWhileStmt();
        case Token::IF:
            return parseIfStmt();
        case Token::RETURN:
            return parseReturnStmt();
        case Token::LBRACE:
            return parseBlock();
        default: {
            NodePtr expr = parseExpr();
            consume(Token::SEMICOLON);
            return makeNode(Node::ExprStmt, {expr});
        }
    }
}

NodePtr parseDeclStmt() {
    Token t = peekToken();

    if (t.kind == Token::AUTO || t.kind == Token::REGISTER) {
        consume(t.kind);
        
        NodePtr type_node = makeNode(t.kind == Token::AUTO ? Node::AUTO : Node::REGISTER);

        Token var_name = next();
        if (var_name.kind != Token::IDENT) {
            std::cerr << "expected identifier in declaration\n";
            exit(EXIT_FAILURE);
        }
        NodePtr var_node = makeNode(Node::IDENT, var_name.v);

        consume(Token::ASSIGN);
        NodePtr expr = parseExpr();
        consume(Token::SEMICOLON);

        return makeNode(Node::DeclStmt, {type_node, var_node, expr});
    }

    return parseStmt();
}

NodePtr parseWhileStmt() {
    consume(Token::WHILE);
    consume(Token::LPAREN);
    NodePtr expr = parseExpr();
    consume(Token::RPAREN);
    NodePtr body = parseDeclStmt();
    return makeNode(Node::WhileStmt, {expr, body});
}

NodePtr parseIfStmt() {
    consume(Token::IF);
    consume(Token::LPAREN);
    NodePtr cond = parseExpr();
    consume(Token::RPAREN);
    NodePtr then_stmt = parseStmt();

    if (peekToken().kind == Token::ELSE) {
        consume(Token::ELSE);
        NodePtr else_stmt = parseStmt();
        return makeNode(Node::IfStmt, {cond, then_stmt, else_stmt});
    }

    return makeNode(Node::IfStmt, {cond, then_stmt});
}

NodePtr parseReturnStmt() {
    consume(Token::RETURN);

    if (peekToken().kind == Token::SEMICOLON) {
        consume(Token::SEMICOLON);
        return makeNode(Node::ReturnStmt);
    }

    NodePtr expr = parseExpr();
    consume(Token::SEMICOLON);
    return makeNode(Node::ReturnStmt, {expr});
}

NodePtr parseArgList() {
    NodePtr args = makeNode(Node::ARGLIST);

    if (peekToken().kind == Token::RPAREN) {
        return args;
    }

    do {
        args->children.push_back(parseExpr());

        if (peekToken().kind == Token::COMMA) {
            consume(Token::COMMA);
        } else {
            break;
        }
    } while (true);

    return args;
}

NodePtr parsePrimaryExpr() {
    switch (Token t = next(); t.kind) {
        case Token::IDENT:
            if (peekToken().kind == Token::LPAREN) {
                consume(Token::LPAREN);
                NodePtr args = parseArgList();
                consume(Token::RPAREN);
                return makeNode(Node::FUNCCALL, {makeNode(Node::IDENT, t.v), args});
            }
            return makeNode(Node::IDENT, t.v);
        case Token::NUMBER:
            return makeNode(Node::NUMBER, std::stoll(std::string(t.v)));
        case Token::LPAREN: {
            NodePtr expr = parseExpr();
            consume(Token::RPAREN);
            return expr;
        }
        case Token::MINUS:
        case Token::BANG:
        case Token::TILDE:
        case Token::ADDRESSOF: {
            auto op = OPS_UNARY[t.kind];
            NodePtr operand = parseExpr(op.prec);

            if (t.kind == Token::ADDRESSOF) {
                if (operand->kind != Node::IDENT && operand->kind != Node::SUBSCRIPT) {
                    std::cerr << "syntax error: operand of address-of operator (&) must be an identifier or subscript\n";
                    exit(EXIT_FAILURE);
                }
            }

            return makeNode(op.node_kind, {operand});
        }
        default:
            std::cerr << "Unexpected token in expression: " << t.v << "\n";
            exit(EXIT_FAILURE);
    }
}

NodePtr parseExpr(unsigned minPrec) {
    NodePtr lhs = parsePrimaryExpr();

    while (true) {
        Token t = peekToken();

        if (t.kind >= std::size(OPS_BINARY) || OPS_BINARY[t.kind].prec == 0) {
            break;
        }
        
        auto op = OPS_BINARY[t.kind];
        
        if (op.prec < minPrec) {
            break;
        }

        next();

        if (t.kind == Token::LBRACKET) {
            NodePtr index = parseExpr();
            
            NodePtr sizeSpec = nullptr;
            if (peekToken().kind == Token::AT) {
                consume(Token::AT);
                Token num = next();
                if (num.kind != Token::NUMBER) {
                    std::cerr << "expected size (number) after '@'\n";
                    exit(EXIT_FAILURE);
                }
                int64_t number = std::stoll(std::string(num.v));
                if (number != 1 && number != 2 && number != 4 && number != 8) {
                    std::cerr << "size specifier does not have allowed value (1, 2, 4, 8)\n";
                    exit(EXIT_FAILURE);
                }
                sizeSpec = makeNode(Node::SIZESPEC, number);
            }

            consume(Token::RBRACKET);
            
            if (sizeSpec) {
                lhs = makeNode(Node::SUBSCRIPT, {lhs, index, sizeSpec});
            } else {
                lhs = makeNode(Node::SUBSCRIPT, {lhs, index});
            }
        } else {
            if (t.kind == Token::ASSIGN) {
                if (lhs->kind != Node::IDENT && lhs->kind != Node::SUBSCRIPT) {
                    std::cerr << "syntax error: left-hand side of assignment must be an identifier or subscript\n";
                    exit(EXIT_FAILURE); 
                }
            }

            auto newPrec = op.rassoc ? op.prec : op.prec + 1;
            auto rhs = parseExpr(newPrec);
            lhs = makeNode(op.node_kind, {lhs, rhs});
        }
    }

    return lhs;
}

std::string_view nodeKindToStringView(Node::Kind node_kind) {
    switch (node_kind) {
        case Node::ADD:       return "+"sv;
        case Node::SUBTRACT:  return "-"sv;
        case Node::UMINUS:    return "u-"sv;
        case Node::SUBSCRIPT: return "[]"sv;
        case Node::LOGICALNOT:return "!"sv;
        case Node::BITWISENOT:return "~"sv;
        case Node::ADDRESSOF: return "&"sv;
        case Node::LT:        return "<"sv;
        case Node::GT:        return ">"sv;
        case Node::EQUAL:     return "=="sv;
        case Node::NOTEQUAL:  return "!="sv;
        case Node::LOGICALAND:return "&&"sv;
        case Node::LOGICALOR: return "||"sv;
        case Node::ASSIGNMENT:return "="sv;
        case Node::AUTO:      return "auto";
        case Node::REGISTER:  return "register";
        
        case Node::BLOCK:     return "block"sv;
        case Node::IfStmt:    return "if"sv;
        case Node::WhileStmt: return "while"sv;
        case Node::ReturnStmt:return "return"sv;
        case Node::DeclStmt:  return "decl"sv;
        case Node::ExprStmt:  return "expr-stmt"sv;
        
        case Node::PROGRAM:   return "program"sv;
        case Node::FUNCTION:  return "func"sv;
        case Node::PARAMLIST: return "paramlist"sv;
        case Node::ARGLIST:   return "arglist"sv;

        default: { std::cout << node_kind << std::endl; return "UNKNOWN"sv; }
    }
}

void printNode(NodePtr node) {
    if (!node) {
        std::cerr << "cant print null node\n";
        exit(EXIT_FAILURE);
    }

    switch (node->kind) {
        case Node::IDENT:
            std::cout << "\"" << node->name.value() << "\"";
            break;
        case Node::NUMBER:
        case Node::SIZESPEC:
            std::cout << node->value.value();
            break;
        default:
            std::cout << "(";
            std::cout << nodeKindToStringView(node->kind);

            if (node->kind == Node::FUNCTION) {
                std::cout << " \"" << node->name.value() << "\"";
            }

            for (NodePtr child : node->children) {
                std::cout << " ";
                printNode(child);
            }

            std::cout << ")";
            break;
    }
}

void printAst(NodePtr root) {
    printNode(root);
    std::cout << std::endl;
}

void checkNode(NodePtr node, FunctionTable& func_table, SymbolTable& symbol_table, int current_depth) {
    if (!node) return;

    switch (node->kind) {
        case Node::FUNCTION: {
            std::string func_name = node->name.value();
            NodePtr param_list = node->children[0];
            NodePtr body = node->children[1];
            int num_of_params = param_list->children.size();

            if (func_table.count(func_name)) {
                auto& info = func_table.at(func_name);
                if (info.is_explicitly_defined) {
                    std::cerr << "semantic error: duplicate function definition for '" << func_name << "'.\n";
                    exit(EXIT_FAILURE);
                }
                if (info.num_of_params != num_of_params) {
                    std::cerr << "semantic error: definition of function '" << func_name << "' has " << num_of_params 
                              << " parameters, but previous call implied " << info.num_of_params << ".\n";
                    exit(EXIT_FAILURE);
                }
                info.is_explicitly_defined = true;
            } else {
                func_table[func_name] = {num_of_params, true};
            }

            SymbolTable functionVarTable;
            int paramDepth = current_depth + 1;
            
            for (auto param : param_list->children) {
                std::string paramName = param->name.value();
                if (functionVarTable.count(paramName)) {
                     std::cerr << "semantic error: duplicate parameter name '" << paramName << "' in function '" << func_name << "'.\n";
                     exit(EXIT_FAILURE);
                }
                functionVarTable[paramName].push_back({SymbolInfo::PARAMETER, paramDepth});
            }
            
            checkNode(body, func_table, functionVarTable, paramDepth - 1);
            break;
        }
        case Node::FUNCCALL: {
            std::string func_name = node->children[0]->name.value();
            NodePtr argList = node->children[1];
            int callArity = argList->children.size();
            
            if (func_table.count(func_name)) {
                auto& info = func_table.at(func_name);
                if (info.num_of_params != callArity) {
                    std::cerr << "semantic error: call to function '" << func_name << "' has " << callArity 
                              << " arguments, but expected " << info.num_of_params << ".\n";
                    exit(EXIT_FAILURE);
                }
            } else {
                func_table[func_name] = {callArity, false};
            }
            
            for (auto arg : argList->children) {
                checkNode(arg, func_table, symbol_table, current_depth);
            }
            break;
        }
        case Node::BLOCK: {
            int newDepth = current_depth + 1;
            std::vector<std::string> symbolsInThisBlock;

            for (auto child : node->children) {
                checkNode(child, func_table, symbol_table, newDepth);
                if (child->kind == Node::DeclStmt) {
                    symbolsInThisBlock.push_back(child->children[1]->name.value());
                }
            }
            
            for (const auto& name : symbolsInThisBlock) {
                symbol_table.at(name).pop_back();
                if (symbol_table.at(name).empty()) {
                    symbol_table.erase(name);
                }
            }
            break;
        }
        case Node::DeclStmt: {
            NodePtr typeNode = node->children[0];
            NodePtr varNode = node->children[1];
            NodePtr exprNode = node->children[2];
            std::string varName = varNode->name.value();
            
            checkNode(exprNode, func_table, symbol_table, current_depth);
            
            if (symbol_table.count(varName) && !symbol_table.at(varName).empty() && symbol_table.at(varName).back().depth == current_depth) {
                std::cerr << "semantic error: redeclaration of variable '" << varName << "'.\n";
                exit(EXIT_FAILURE);
            }
            
            SymbolInfo::StorageClass sc = (typeNode->kind == Node::REGISTER) ? SymbolInfo::REGISTER : SymbolInfo::AUTO;
            symbol_table[varName].push_back({sc, current_depth});
            break;
        }
        case Node::IDENT: {
            SymbolInfo* info = findSymbol(node->name.value(), current_depth, symbol_table);
            if (info == nullptr) {
                std::cerr << "semantic error: use of undeclared identifier '" << node->name.value() << "'.\n";
                exit(EXIT_FAILURE);
            }
            break;
        }
        case Node::ADDRESSOF: {
            NodePtr operand = node->children[0];
            if (operand->kind == Node::IDENT) {
                SymbolInfo* info = findSymbol(operand->name.value(), current_depth, symbol_table);
                if (info) {
                    if (info->storage == SymbolInfo::REGISTER) {
                        std::cerr << "semantic error: cannot take address of register variable '" << operand->name.value() << "'.\n";
                        exit(EXIT_FAILURE);
                    }
                    if (info->storage == SymbolInfo::PARAMETER) {
                        std::cerr << "semantic error: cannot take address of parameter '" << operand->name.value() << "'.\n";
                        exit(EXIT_FAILURE);
                    }
                }
            }

            checkNode(operand, func_table, symbol_table, current_depth);
            break;
        }
        default:
            for (auto child : node->children) {
                checkNode(child, func_table, symbol_table, current_depth);
            }
            break;
    }
}

void performSemanticCheck() {
    FunctionTable func_table;
    SymbolTable symbol_table;
    
    checkNode(ast, func_table, symbol_table, 0);
}

size_t countAstNodes(NodePtr node) {
    if (!node) {
        return 0;
    }

    size_t count = 1; 

    for (NodePtr child : node->children) {
        count += countAstNodes(child);
    }

    return count;
}

int main(int argc, char *argv[]) {
    int opt;
    char *prog = NULL;
    bool print_ast = false;
    bool semantic_check_only = false;
    bool print_tokens_and_ast_size = false;

    while ((opt = getopt(argc, argv, "actp:")) != -1) {
        switch (opt) {
            case 'a':
                print_ast = true;
                break;
            case 'c':
                semantic_check_only = true;
                break;
            case 't':
                print_tokens_and_ast_size = true;
                break;
            case 'p':
                prog = optarg;
                break;
            default:
                fprintf(stderr, "Usage: %s (-a|-c) -t program_file\n", argv[0]);
                return 1;
        }
    }

    if (!(print_ast xor semantic_check_only)) {
        fprintf(stderr, "Usage: %s (-a|-c) -t program_file\n", argv[0]);
        return 1;
    }

    if (optind < argc) {
        prog = readfile(argv[optind]);
    }

    if (!prog) {
        fprintf(stderr, "%s: No program specified\n", argv[0]);
        return 1;
    }

    std::string_view input{prog};
    tokens = tokenize(input);

    initializeOperatorTables();
    ast = parseProgram();

    performSemanticCheck();

    if (semantic_check_only) {
        return 0;
    }

    if (print_tokens_and_ast_size) {
        for (Token token : tokens) {
            std::cout << "Token: " << token.v << " " << token.kind << "\n";
        }
    }

    if (print_tokens_and_ast_size) {
        std::cout << "Number of AST nodes: " << countAstNodes(ast) << std::endl;
    }

    printAst(ast);

    return 0;
}
