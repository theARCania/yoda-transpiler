#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// --- Tokenizer Section ---

// Enum for all possible token types
typedef enum {
    TOKEN_KEYWORD,
    TOKEN_IDENTIFIER,
    TOKEN_NUMBER,
    TOKEN_LPAREN,
    TOKEN_RPAREN,
    TOKEN_LBRACE,
    TOKEN_RBRACE,
    TOKEN_EQUALS,
    TOKEN_SEMICOLON,
    TOKEN_COMMA,
    TOKEN_PREPROCESSOR,
    TOKEN_EOF,
    TOKEN_UNKNOWN
} TokenType;

// Keywords
const char* keywords[] = {"int", "void", "char", "for", "while", "if", "else", "return"};
const int num_keywords = sizeof(keywords) / sizeof(char*);

// Token struct to hold type and the actual text (lexeme)
typedef struct {
    TokenType type;
    char* lexeme;
} Token;

// A dynamic array to store tokens
typedef struct {
    Token* tokens;
    int count;
    int capacity;
} TokenList;

void add_token(TokenList* list, TokenType type, const char* lexeme, int len) {
    if (list->count >= list->capacity) {
        list->capacity = list->capacity == 0 ? 8 : list->capacity * 2;
        list->tokens = realloc(list->tokens, list->capacity * sizeof(Token));
    }
    char* token_lexeme = malloc(len + 1);
    strncpy(token_lexeme, lexeme, len);
    token_lexeme[len] = '\0';
    list->tokens[list->count++] = (Token){type, token_lexeme};
}

int is_keyword(const char* str) {
    for (int i = 0; i < num_keywords; i++) {
        if (strcmp(keywords[i], str) == 0) {
            return 1;
        }
    }
    return 0;
}

TokenList tokenize(const char* source) {
    TokenList token_list = {NULL, 0, 0};
    const char* current = source;

    while (*current != '\0') {
        if (isspace(*current)) {
            current++;
            continue;
        }
        if (*current == '/' && *(current+1) == '/') { // Basic comment support
            while (*current != '\n' && *current != '\0') current++;
            continue;
        }
        
        if (*current == '#') {
            const char* start = current;
            while (*current != '\n' && *current != '\0') current++;
            add_token(&token_list, TOKEN_PREPROCESSOR, start, current - start);
            continue;
        }

        // Handle operators: ==, !=, >=, <=, >, <
        // This block must come BEFORE the single-character checks to correctly handle multi-character tokens.
        if (strchr("><=!", *current)) {
            const char* start = current;
            // Check for two-character operators first
            if ((*start == '>' || *start == '<' || *start == '!' || *start == '=') && *(start + 1) == '=') {
                add_token(&token_list, TOKEN_IDENTIFIER, start, 2);
                current += 2;
                continue;
            }
            // Check for single-character operators
            if (*start == '>' || *start == '<') {
                add_token(&token_list, TOKEN_IDENTIFIER, start, 1);
                current++;
                continue;
            }
            // A single '=' is handled later as an assignment. A single '!' is an error.
        }

        if (*current == '(') { add_token(&token_list, TOKEN_LPAREN, current++, 1); continue; }
        if (*current == ')') { add_token(&token_list, TOKEN_RPAREN, current++, 1); continue; }
        if (*current == '{') { add_token(&token_list, TOKEN_LBRACE, current++, 1); continue; }
        if (*current == '}') { add_token(&token_list, TOKEN_RBRACE, current++, 1); continue; }
        if (*current == '=') { add_token(&token_list, TOKEN_EQUALS, current++, 1); continue; }
        if (*current == ';') { add_token(&token_list, TOKEN_SEMICOLON, current++, 1); continue; }
        if (*current == ',') { add_token(&token_list, TOKEN_COMMA, current++, 1); continue; }
        
        if (isdigit(*current)) {
            const char* start = current;
            while (isdigit(*current)) current++;
            add_token(&token_list, TOKEN_NUMBER, start, current - start);
            continue;
        }

        if (isalpha(*current) || *current == '_') {
            const char* start = current;
            while (isalnum(*current) || *current == '_') current++;
            int len = current - start;
            char* temp = malloc(len + 1);
            if (temp == NULL) { fprintf(stderr, "Memory allocation failed.\n"); exit(1); }
            strncpy(temp, start, len);
            temp[len] = '\0';
            if (is_keyword(temp)) {
                add_token(&token_list, TOKEN_KEYWORD, start, len);
            } else {
                add_token(&token_list, TOKEN_IDENTIFIER, start, len);
            }
            free(temp);
            continue;
        }
        
        // Correctly parse string literals
        if (*current == '"') {
            const char* start = current;
            current++; // Move past the opening quote
            while(*current != '"' && *current != '\0') {
                 if (*current == '\\' && *(current+1) != '\0') current++; // Skip escaped char
                 current++;
            }
            if (*current == '"') current++; // Move past the closing quote
            add_token(&token_list, TOKEN_IDENTIFIER, start, current - start);
            continue;
        }

        printf("Tokenizer Error: Unknown character '%c'\n", *current);
        add_token(&token_list, TOKEN_UNKNOWN, current++, 1);
    }
    
    add_token(&token_list, TOKEN_EOF, "EOF", 3);
    return token_list;
}

void free_tokens(TokenList* list) {
    for (int i = 0; i < list->count; i++) {
        free(list->tokens[i].lexeme);
    }
    free(list->tokens);
}

// --- Parser Section ---

typedef struct {
    TokenList tokens;
    int current_token_pos;
    char* output;
    int output_capacity;
    int output_size;
} Parser;

// Forward declarations
int parse_statement(Parser* p);
int parse_for_loop(Parser* p);
int parse_while_loop(Parser* p);
int parse_if_statement(Parser* p);
int parse_variable_declaration(Parser* p);
int parse_function_declaration(Parser* p);
int parse_reversed_function_call(Parser* p);

void append_output(Parser* p, const char* str) {
    int len = strlen(str);
    while (p->output_size + len + 1 > p->output_capacity) {
        p->output_capacity = p->output_capacity == 0 ? 256 : p->output_capacity * 2;
        p->output = realloc(p->output, p->output_capacity);
    }
    strcat(p->output, str);
    p->output_size += len;
}

Token current_token(Parser* p) { return p->tokens.tokens[p->current_token_pos]; }
Token peek_at(Parser* p, int offset) {
    if (p->current_token_pos + offset >= p->tokens.count) return p->tokens.tokens[p->tokens.count - 1];
    return p->tokens.tokens[p->current_token_pos + offset];
}
Token advance(Parser* p) {
    if (p->current_token_pos < p->tokens.count - 1) p->current_token_pos++;
    return p->tokens.tokens[p->current_token_pos - 1];
}
int match(Parser* p, TokenType type) { return current_token(p).type == type; }
int consume(Parser* p, TokenType type, const char* error_message) {
    if (match(p, type)) {
        advance(p);
        return 1;
    }
    printf("Parser Error: %s. Got '%s' instead.\n", error_message, current_token(p).lexeme);
    return 0;
}

void slurp_tokens_until(Parser *p, TokenType end_type, char* buffer, int buffer_size) {
    buffer[0] = '\0';
    while (!match(p, end_type) && !match(p, TOKEN_EOF)) {
        if (strlen(buffer) > 0) strncat(buffer, " ", buffer_size - strlen(buffer) - 1);
        strncat(buffer, current_token(p).lexeme, buffer_size - strlen(buffer) - 1);
        advance(p);
    }
}

// Helper to find the offset after a matching parenthesis block
int get_offset_after_paren(Parser* p) {
    if (!match(p, TOKEN_LPAREN)) return 0;
    int paren_level = 1;
    int offset = 1;
    while(paren_level > 0 && (p->current_token_pos + offset) < p->tokens.count) {
        Token t = peek_at(p, offset);
        if (t.type == TOKEN_LPAREN) paren_level++;
        if (t.type == TOKEN_RPAREN) paren_level--;
        offset++;
    }
    return offset;
}

int parse_reversed_function_call(Parser* p) {
    char args_buffer[1024] = {0};
    if (!consume(p, TOKEN_LPAREN, "Expected '(' for function call")) return 0;
    
    int start_token_pos = p->current_token_pos;
    int paren_level = 1;
    while(paren_level > 0 && !match(p, TOKEN_EOF)) {
        if (match(p, TOKEN_LPAREN)) paren_level++;
        if (match(p, TOKEN_RPAREN)) paren_level--;
        if (paren_level > 0) advance(p);
    }
    int end_token_pos = p->current_token_pos;

    for (int i = start_token_pos; i < end_token_pos; i++) {
        strncat(args_buffer, p->tokens.tokens[i].lexeme, sizeof(args_buffer) - strlen(args_buffer) - 1);
        if (i < end_token_pos - 1 && peek_at(p, i - p->current_token_pos + 1).type != TOKEN_COMMA) {
             strncat(args_buffer, " ", sizeof(args_buffer) - strlen(args_buffer) - 1);
        }
    }

    if (!consume(p, TOKEN_RPAREN, "Expected ')' to end function call arguments")) return 0;
    Token func_name = current_token(p);
    if (!consume(p, TOKEN_IDENTIFIER, "Expected function name")) return 0;
    if (!consume(p, TOKEN_SEMICOLON, "Expected ';' after function call")) return 0;
    
    char final_call[2048];
    sprintf(final_call, "    %s(%s);\n", func_name.lexeme, args_buffer);
    append_output(p, final_call);
    return 1;
}

int parse_for_loop(Parser* p) {
    char condition[256] = {0};
    if (!consume(p, TOKEN_LPAREN, "Expected '(' before for loop condition")) return 0;
    slurp_tokens_until(p, TOKEN_RPAREN, condition, sizeof(condition));
    if (!consume(p, TOKEN_RPAREN, "Expected ')' after for loop condition")) return 0;
    if (!consume(p, TOKEN_KEYWORD, "Expected 'for' keyword after condition")) return 0;
    
    char temp_buffer[512];
    sprintf(temp_buffer, "    for (%s) {\n", condition);
    append_output(p, temp_buffer);
    
    if (!consume(p, TOKEN_LBRACE, "Expected '{' before for loop body")) return 0;
    while(!match(p, TOKEN_RBRACE) && !match(p, TOKEN_EOF)) {
        if (!parse_statement(p)) return 0;
    }
    if (!consume(p, TOKEN_RBRACE, "Expected '}' after for loop body")) return 0;
    append_output(p, "    }\n");
    return 1;
}

int parse_while_loop(Parser* p) {
    char condition[256] = {0};
    if (!consume(p, TOKEN_LPAREN, "Expected '(' before while loop condition")) return 0;
    slurp_tokens_until(p, TOKEN_RPAREN, condition, sizeof(condition));
    if (!consume(p, TOKEN_RPAREN, "Expected ')' after while loop condition")) return 0;
    if (!consume(p, TOKEN_KEYWORD, "Expected 'while' keyword after condition")) return 0;

    char temp_buffer[512];
    sprintf(temp_buffer, "    while (%s) {\n", condition);
    append_output(p, temp_buffer);

    if (!consume(p, TOKEN_LBRACE, "Expected '{' before while loop body")) return 0;
    while(!match(p, TOKEN_RBRACE) && !match(p, TOKEN_EOF)) {
        if (!parse_statement(p)) return 0;
    }
    if (!consume(p, TOKEN_RBRACE, "Expected '}' after while loop body")) return 0;
    append_output(p, "    }\n");
    return 1;
}

int parse_if_statement(Parser* p) {
    char condition[256] = {0};
    int has_else = 0;

    if (!consume(p, TOKEN_LPAREN, "Expected '(' before if condition")) return 0;
    slurp_tokens_until(p, TOKEN_RPAREN, condition, sizeof(condition));
    if (!consume(p, TOKEN_RPAREN, "Expected ')' after if condition")) return 0;
    if (!consume(p, TOKEN_KEYWORD, "Expected 'if' keyword after condition")) return 0;

    char temp_buffer[512];
    sprintf(temp_buffer, "    if (%s) {\n", condition);
    append_output(p, temp_buffer);

    if (!consume(p, TOKEN_LBRACE, "Expected '{' before if body")) return 0;
    while(!match(p, TOKEN_RBRACE) && !match(p, TOKEN_EOF)) {
        if (!parse_statement(p)) return 0;
    }
    if (!consume(p, TOKEN_RBRACE, "Expected '}' after if body")) return 0;
    append_output(p, "    }\n");
    
    if (match(p, TOKEN_KEYWORD) && strcmp(current_token(p).lexeme, "else") == 0) {
        has_else = 1;
        advance(p); // consume 'else'
        append_output(p, "    else {\n");
        if (!consume(p, TOKEN_LBRACE, "Expected '{' before else body")) return 0;
        while(!match(p, TOKEN_RBRACE) && !match(p, TOKEN_EOF)) {
            if (!parse_statement(p)) return 0;
        }
        if (!consume(p, TOKEN_RBRACE, "Expected '}' after else body")) return 0;
        append_output(p, "    }\n");
    }
    return 1;
}

int parse_variable_declaration(Parser* p) {
    Token value = advance(p); // consume the number
    if (!consume(p, TOKEN_EQUALS, "Expected '=' after value in declaration")) return 0;
    Token name = current_token(p);
    if (!consume(p, TOKEN_IDENTIFIER, "Expected identifier name for variable")) return 0;
    Token type = current_token(p);
    if (!consume(p, TOKEN_KEYWORD, "Expected type keyword for variable")) return 0;
    if (!consume(p, TOKEN_SEMICOLON, "Expected ';' after variable declaration")) return 0;

    char temp_buffer[512];
    sprintf(temp_buffer, "    %s %s = %s;\n", type.lexeme, name.lexeme, value.lexeme);
    append_output(p, temp_buffer);
    return 1;
}

int parse_statement(Parser* p) {
    if (match(p, TOKEN_NUMBER)) {
        return parse_variable_declaration(p);
    }
    
    if (match(p, TOKEN_LPAREN)) {
        int offset = get_offset_after_paren(p);
        Token token_after_paren = peek_at(p, offset);

        if (token_after_paren.type == TOKEN_KEYWORD) {
            if (strcmp(token_after_paren.lexeme, "for") == 0) return parse_for_loop(p);
            if (strcmp(token_after_paren.lexeme, "while") == 0) return parse_while_loop(p);
            if (strcmp(token_after_paren.lexeme, "if") == 0) return parse_if_statement(p);
        }

        if (token_after_paren.type == TOKEN_IDENTIFIER) {
            if (peek_at(p, offset + 1).type == TOKEN_SEMICOLON) {
                return parse_reversed_function_call(p);
            }
        }
    }
    
    if (match(p, TOKEN_KEYWORD) || match(p, TOKEN_IDENTIFIER)) {
        char line[1024];
        slurp_tokens_until(p, TOKEN_SEMICOLON, line, sizeof(line));
        if (consume(p, TOKEN_SEMICOLON, "Expected ';' after statement")) {
            append_output(p, "    ");
            append_output(p, line);
            append_output(p, ";\n");
            return 1;
        }
        return 0;
    }

    printf("Parser Error: Unrecognized statement starting with '%s'\n", current_token(p).lexeme);
    return 0;
}

int parse_function_declaration(Parser* p) {
    char args[1024] = {0};
    if (!consume(p, TOKEN_LPAREN, "Expected '(' before function arguments")) return 0;
    
    while(!match(p, TOKEN_RPAREN) && !match(p, TOKEN_EOF)) {
        if (strlen(args) > 0) strcat(args, ", ");
        Token arg_name = current_token(p);
        if(!consume(p, TOKEN_IDENTIFIER, "Expected argument name")) return 0;
        Token arg_type = current_token(p);
        if(!consume(p, TOKEN_KEYWORD, "Expected argument type")) return 0;
        
        strcat(args, arg_type.lexeme);
        strcat(args, " ");
        strcat(args, arg_name.lexeme);

        if (match(p, TOKEN_COMMA)) advance(p);
        else if (!match(p, TOKEN_RPAREN)) { printf("Parser Error: Expected ',' or ')' in argument list.\n"); return 0; }
    }
    if (!consume(p, TOKEN_RPAREN, "Expected ')' after function arguments")) return 0;

    Token func_name = current_token(p);
    if (!consume(p, TOKEN_IDENTIFIER, "Expected function name")) return 0;
    Token return_type = current_token(p);
    if (!consume(p, TOKEN_KEYWORD, "Expected function return type")) return 0;

    char func_header[2048];
    sprintf(func_header, "%s %s(%s) {\n", return_type.lexeme, func_name.lexeme, args);
    append_output(p, func_header);

    if (!consume(p, TOKEN_LBRACE, "Expected '{' before function body")) return 0;
    while (!match(p, TOKEN_RBRACE) && !match(p, TOKEN_EOF)) {
        if (!parse_statement(p)) return 0;
    }
    if (!consume(p, TOKEN_RBRACE, "Expected '}' after function body")) return 0;
    append_output(p, "}\n\n");
    return 1;
}

char* parse(TokenList tokens) {
    Parser p = {tokens, 0, NULL, 0, 0};
    p.output = malloc(1); p.output[0] = '\0';

    while(!match(&p, TOKEN_EOF)) {
        if (match(&p, TOKEN_PREPROCESSOR)) {
            append_output(&p, current_token(&p).lexeme);
            append_output(&p, "\n");
            advance(&p);
        } else if (match(&p, TOKEN_LPAREN)) {
            if(!parse_function_declaration(&p)) {
                free(p.output);
                return NULL;
            }
        } else {
             printf("Parser Error: Only preprocessor directives or function definitions allowed at top level. Found '%s'.\n", current_token(&p).lexeme);
             free(p.output);
             return NULL;
        }
    }
    return p.output;
}

// --- Main Driver ---

char* read_file(const char* path) {
    FILE* file = fopen(path, "rb");
    if (!file) { fprintf(stderr, "Could not open file \"%s\".\n", path); exit(74); }
    fseek(file, 0L, SEEK_END);
    size_t fileSize = ftell(file);
    rewind(file);
    char* buffer = (char*)malloc(fileSize + 1);
    if (!buffer) { fprintf(stderr, "Not enough memory to read \"%s\".\n", path); exit(74); }
    size_t bytesRead = fread(buffer, sizeof(char), fileSize, file);
    if (bytesRead < fileSize) { fprintf(stderr, "Could not read file \"%s\".\n", path); exit(74); }
    buffer[bytesRead] = '\0';
    fclose(file);
    return buffer;
}

int main(int argc, char* argv[]) {
    if (argc != 2) { printf("Usage: %s <filename.ydc>\n", argv[0]); return 1; }
    const char* source_file = argv[1];
    char* source_code = read_file(source_file);

    printf("--- Tokenizing ---\n");
    TokenList tokens = tokenize(source_code);
    
    printf("\n--- Parsing & Transpiling ---\n");
    char* c_code = parse(tokens);
    if (!c_code) {
        printf("Failed to transpile due to parsing errors.\n");
        free(source_code);
        free_tokens(&tokens);
        return 1;
    }
    printf("Transpiled C code:\n---\n%s---\n", c_code);
    
    printf("\n--- Compiling with GCC ---\n");
    FILE* out_file = fopen("output.c", "w");
    if (!out_file) { printf("Error: could not create output.c\n"); return 1; }
    fprintf(out_file, "%s", c_code);
    fclose(out_file);
    
    int result = system("gcc -o output output.c");
    if (result == 0) {
        printf("\nSuccess! Compiled to './output' executable.\n");
    } else {
        printf("\nGCC compilation failed.\n");
    }

    free(source_code);
    free(c_code);
    free_tokens(&tokens);
    return 0;
}

