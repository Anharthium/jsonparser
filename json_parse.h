#include <string>
#include <vector>
#include <unordered_map>

using std::string;

const char l_brace = '{';
const char r_brace = '}';
const char l_bracket = '[';
const char r_bracket = ']';
const char comma = ',';
const char d_quote = '"';
const char colon = ':';
const char minus = '-';
const char decimal = '.';

typedef enum {
    L_BRACE,
    R_BRACE,
    L_BRACKET,
    R_BRACKET,
    COMMA,
    COLON,
    KEYW_TRUE,
    KEYW_FALSE,
    KEYW_NULL,
    STRING,
    NUMBER,
    // Add more tokens above
    TOKEN_TYPE_END
} token_type;
 
struct token_struct {
    token_type tk_type; 
    std::string tk_value;
    // Line and column number of start of token character from the original input
    int line;
    int col;

    //token_struct(token_type type, string value, int line, int col): 
    //    tk_type {type}, tk_value {value}, line {line}, col {col} {}
};


int lex(std::ifstream &inf, std::vector<token_struct> &token_list);
int parse_json_list(std::vector<token_struct> &token_list, int *tk_index_ptr);
int parse_json_object(std::vector<token_struct> &token_list, int *tk_index_ptr);