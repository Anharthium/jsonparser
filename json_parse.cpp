#include <vector>
#include <string>
#include <fstream>
#include <cctype>
#include <iostream>
#include <unordered_map>
#include <unordered_set>
#include "json_parse.h"


#define IS_HEX_CHAR(c) (c >= '0' && c <= '9' || c >= 'a' && c <= 'f' || c >= 'A' && c <= 'F') 

// States for DFA to parse JSON number
enum class parse_json_num_state {
    init_state,
    accepted_minus,
    accepted_exactly_one_non_zero_digit,
    accepted_exactly_one_zero_digit,
    accept_all_digits,
    accept_fraction,
    accepted_one_digit_of_fraction,
    accepted_exponent_char,
    accept_all_exponent_digits,
};

// States for DFA to parse JSON string
enum class parse_json_string_state {
    init_state,
    accepted_start_quote,
    accepted_single_backslash,
    accept_unicode_chararacter_sequence,
    accepted_end_quote,
};

// States for parsing a json list [...]
enum class parse_json_list_state {
    accept_list_value_or_end_bracket,
    accept_list_value,
    accept_comma_or_end_bracket,
};

// States for parsing a json object {...}
enum class parse_json_obj_state {
    accept_key_or_end_brace,
    accept_key,
    accept_colon,
    accept_comma_or_end_brace,
    accept_value,
};


std::unordered_map<char, token_type> char_to_token_type_dict = {
    {l_brace, L_BRACE},
    {r_brace, R_BRACE},
    {l_bracket, L_BRACKET},
    {r_bracket, R_BRACKET},
    {comma, COMMA},
    {colon, COLON},
    {'t', KEYW_TRUE},
    {'f', KEYW_FALSE},
    {'n', KEYW_NULL},
    // Add more if necessary
};

std::unordered_set<char> valid_escape_char = {
    'a', 'b', 'f', 'n', 'r', 't', 'v', '\'', '\"', '?', '\\'
};

int parse_json_number(char init_char, std::ifstream &inf, std::vector<token_struct> &token_list, int & line, int & col);
void count_lines_and_col(char c, int & line, int & col, bool char_consumed=true);

// Small utility function to count number of lines and col when char c is consumed/put back from/into input stream
void count_lines_and_col(char c, int & line, int & col, bool char_consumed) {
    static int old_col = 1; // used to reset when a character is put back into input stream
    if (c == '\n') {
        if (char_consumed) {
            old_col = col; // Store
            col = 1;
            ++line;
        }
        else {
            col = old_col;
            --line;
        }
    }
    else {
        if (char_consumed) {
            ++col;
        }
        else {
            --col;
        }
    }
}

// Accumulate and Accept/Reject Json string using a DFA. Final state: accepted_end_quote.
int parse_json_string(char init_char, std::ifstream &inf, std::vector<token_struct> &token_list, int & line, int & col) {
    char c = init_char;
    char temp_c = c, count = 0;
    std::string json_string = "";
    token_struct tk {STRING, json_string, line, col};
    parse_json_string_state curr_state = parse_json_string_state::init_state;
    count_lines_and_col(c, line, col, false); // Reset done so that this function inside the do while loop works correctly.

    do
    {
        std::string hex_digits = "";
        count_lines_and_col(c, line, col);
        switch(curr_state) {
            case parse_json_string_state::init_state:
                if (c == d_quote) {
                    curr_state = parse_json_string_state::accepted_start_quote;
                }
                else {
                    goto print_error_and_return;
                }
                break;
            case parse_json_string_state::accepted_start_quote:
                if (c == d_quote) {
                    curr_state = parse_json_string_state::accepted_end_quote;
                }
                else if (c == '\\') {
                    curr_state = parse_json_string_state::accepted_single_backslash;
                }
                else if (c >= 0 && c <= 31) { // Can't accept control characters without escaping
                    goto print_error_and_return;
                }
                else {
                    // Accept any other character. State remains same
                }
                break;
            case parse_json_string_state::accepted_single_backslash:
                if (valid_escape_char.find(c) != valid_escape_char.end()) {
                    curr_state = parse_json_string_state::accepted_start_quote;
                }
                else if (c == 'u') {
                    curr_state = parse_json_string_state::accept_unicode_chararacter_sequence;
                }
                else {
                    goto print_error_and_return;
                }
                break;
            case parse_json_string_state::accept_unicode_chararacter_sequence:
                // consume the next three hex charcters indicating unicode char sequence
                temp_c = c;
                count = 0;
                do {
                    count_lines_and_col(temp_c, line, col);
                    if (!IS_HEX_CHAR(temp_c)) {
                        std::cerr << "Error. Not a hex digit ==> " << temp_c << std::endl;
                        return -1;
                    }
                    hex_digits += temp_c;
                    ++count;
                    if (count == 4) break;
                } while (inf.get(temp_c));
                
                if (count != 4) {
                    std::cerr << "More hex digits expected after '\\u'" << std::endl;
                    return -1;
                }
                curr_state = parse_json_string_state::accepted_start_quote; // Go back to accepting any characters
                break;
            default:
                std::cerr << "Unrecognized lex string state: " << static_cast<int>(curr_state) << std::endl;
                break;
        }

        json_string += c;
        json_string += hex_digits;

        if (curr_state == parse_json_string_state::accepted_end_quote) {
            // String end. Break loop. No need to consume any more characters
            break;
        }
    
    } while (inf.get(c));

    // Case when we run out of characters from input stream before we reach final state.
    tk.tk_value = json_string;
    token_list.push_back(tk);
    return 0; // success


print_error_and_return:
    std::cerr << "Error in parsing json string State: " << static_cast<int>(curr_state) <<  " char '" << c << "' Line " 
        << line << " col " << col << std::endl;
    return -1; // error
}

// Accumulate and Accept/Reject Json number using a DFA.
// Final states have push_char_back_to_istream_and_end_parse as error handling while non-final states 
// have print_error_and_return as its error handling
int parse_json_number(char init_char, std::ifstream &inf, std::vector<token_struct> &token_list, int & line, int & col) {
    
    std::string json_num = ""; // accumulated json number to be added to token_list at the end
    char c = init_char;
    parse_json_num_state curr_state = parse_json_num_state::init_state;
    token_struct tk = {NUMBER, json_num, line, col};
    count_lines_and_col(c, line, col, false); // Reset done so that this function inside the do while loop works correctly.
    do {
        count_lines_and_col(c, line, col);
        switch(curr_state) {
            case parse_json_num_state::init_state:
                if (c == minus) {
                    curr_state = parse_json_num_state::accepted_minus;
                }
                else if (c >= '1' && c <= '9') {
                    curr_state = parse_json_num_state::accepted_exactly_one_non_zero_digit;
                }
                else if (c == '0') {
                    curr_state = parse_json_num_state::accepted_exactly_one_zero_digit;
                }
                else {
                    goto print_error_and_return;
                }
                break;
            case parse_json_num_state::accepted_exactly_one_non_zero_digit:
                if (c >= '0' && c <= '9') {
                    curr_state = parse_json_num_state::accept_all_digits;
                }
                else {
                    goto push_char_back_to_istream_and_end_parse;
                }
                break;
            case parse_json_num_state::accepted_minus:
                if (c == '0') {
                    curr_state = parse_json_num_state::accepted_exactly_one_zero_digit;
                }
                else if (c >= '1' && c <= '9') {
                    curr_state = parse_json_num_state::accept_all_digits;
                }
                else {
                    goto print_error_and_return;
                }
                break;
            case parse_json_num_state::accepted_exactly_one_zero_digit:
                if (c == decimal) {
                    curr_state = parse_json_num_state::accept_fraction;
                }
                else if (c >= '0' && c <= '9') 
                { 
                    goto print_error_and_return;
                }
                else {
                    goto push_char_back_to_istream_and_end_parse;
                }
                break;
            case parse_json_num_state::accept_all_digits:
                if (c >= '0' && c <= '9') {
                    curr_state = parse_json_num_state::accept_all_digits;
                }
                else if (c == decimal) {
                    curr_state = parse_json_num_state::accept_fraction;
                }
                else {
                    goto push_char_back_to_istream_and_end_parse;
                }
                break;
            case parse_json_num_state::accept_fraction:
                if (c >= '0' && c <= '9') {
                    curr_state = parse_json_num_state::accepted_one_digit_of_fraction;
                }
                else {
                    goto print_error_and_return;
                }
                break;
            case parse_json_num_state::accepted_one_digit_of_fraction:
                if (c >= '0' && c <= '9') {
                    curr_state = parse_json_num_state::accepted_one_digit_of_fraction;
                }
                else if (c == 'e' || c == 'E') {
                    curr_state = parse_json_num_state::accepted_exponent_char;
                }
                else {
                    goto push_char_back_to_istream_and_end_parse;
                }
                break;
            case parse_json_num_state::accepted_exponent_char:
                if (c == '-' || c == '+' || c >= '0' && c <= '9') {
                    curr_state = parse_json_num_state::accept_all_exponent_digits;
                }
                else {
                    goto print_error_and_return;
                }
                break;
            case parse_json_num_state::accept_all_exponent_digits:
                if (c >= '0' && c <= '9') {
                    curr_state = parse_json_num_state::accept_all_exponent_digits;
                }
                else {
                    goto push_char_back_to_istream_and_end_parse;
                }
                break;
            default:
                std::cerr << "Unrecognized lex number state: " << static_cast<int>(curr_state) << std::endl;
                break;
        }
        json_num += c;
    } while (inf.get(c));

    // Case when we run out of characters from input stream
    tk.tk_value = json_num;
    token_list.push_back(tk);
    return 0; // success

print_error_and_return:
    std::cerr << "Error in parsing json number State: " << static_cast<int>(curr_state) <<  " char '" << c << "' Line " 
        << line << " col " << col << std::endl;
    return -1; // error

push_char_back_to_istream_and_end_parse:
    //std::cout << "Pushback char to ifsteam: " << c << std::endl;
    inf.putback(c);
    tk.tk_value = json_num;
    count_lines_and_col(c, line, col, false); // Reset line count
    token_list.push_back(tk);
    return 0; // success
}
 
// Divide the input into different tokens. Return -1 if there's an error else 0 for success.
int lex(std::ifstream &inf, std::vector<token_struct> &token_list) {

    std::unordered_map <char, std::string> m_str_dict = {
                {'t', "true"},
                {'f', "false"},
                {'n', "null"},
    };

    int line = 1, col = 0; // Keep track of line and column number of input stream.
    char c;
    while (inf.get(c)) {
        
        count_lines_and_col(c, line, col);
        
        if (c == l_brace || c == r_brace || c == l_bracket || c == r_bracket || c == comma || c == colon) {
            std::string temp_s {c};
            token_struct tk {char_to_token_type_dict[c], temp_s, line, col};
            token_list.push_back(tk);
            //std::cout << temp_s;
        }
        else if (std::isspace(c)) {
            // throw away
        }
        else if (c == d_quote) {
            // Start of a json string. Keep iterating till we find matching end quote or end of input.
            if (parse_json_string(c, inf, token_list, line, col) != 0) {
                return -1; // error
            }
        }
        else if (c == 't' || c == 'f' || c == 'n') {
            // keyword = true or false or null
            char temp_c;
            std::string m_str = m_str_dict[c];
            std::string s_read {c};
            token_struct tk {char_to_token_type_dict[c], s_read, line, col};
            int i = 0;
            // read exactly as many characters as m_str characters and see if it matches
            while (inf.get(temp_c)) {

                count_lines_and_col(temp_c, line, col);

                s_read += temp_c;
                ++i;
                if (i == m_str.size()-1) {
                    break;
                }
            }
            if (s_read != m_str) {
                std::cerr << "Lex: Unexpected keyword ==> '" << s_read << "'" << "at line " << line << " col " << col << std::endl;
                return -1; // error
            }
            tk.tk_value = s_read;
            token_list.push_back(tk);
        }
        else if (c >= '0' && c <= '9' || c == minus) {
            // start json number
            if (parse_json_number(c, inf, token_list, line, col) != 0) {
                return -1; // error
            }
        }
        else {
            std::cerr << "Lex: Unexpected char '" << c << "'" << std::endl;
            return -1; // error
        }
    }

    return 0; // Success
}


// Implementation of recursive descent parser.
// Keep parsing a recursive part of json grammar till we can't divide it any further.
// Note that each token by itself is valid according to json grammar. 

// Json list starts with '[' and ends with ']'. Different values are separated by ','. Calling this function means 
// we have already seen the starting '['
int parse_json_list(std::vector<token_struct> &token_list, int *tk_index_ptr) {
    
    parse_json_list_state curr_state = parse_json_list_state::accept_list_value_or_end_bracket;
    while (*tk_index_ptr < token_list.size()) {
        token_struct &tk = token_list[*tk_index_ptr];
        (*tk_index_ptr)++;
        switch(curr_state) {
            case parse_json_list_state::accept_list_value_or_end_bracket:
                if (tk.tk_type == L_BRACKET) {
                    // start of another json list
                    if (parse_json_list(token_list, tk_index_ptr) != 0) {
                        return -1; // Parse fail
                    }
                    curr_state = parse_json_list_state::accept_comma_or_end_bracket;
                }
                else if (tk.tk_type == R_BRACKET) {
                    // End of this json list. End of parse
                    return 0; // Parse successful
                }
                else if (tk.tk_type == L_BRACE) {
                    // start of json object
                    if (parse_json_object(token_list, tk_index_ptr) != 0) {
                        return -1;
                    }
                    curr_state = parse_json_list_state::accept_comma_or_end_bracket;
                }
                else if (tk.tk_type == KEYW_TRUE || tk.tk_type == KEYW_FALSE || tk.tk_type == KEYW_NULL 
                    || tk.tk_type == STRING || tk.tk_type == NUMBER) {
                        // valid json list values. Accept it.
                        curr_state = parse_json_list_state::accept_comma_or_end_bracket;
                }
                else {
                    std::cerr << tk.line << ":" << tk.col << "Json list parsing error: Unexpected token ==> " << tk.tk_value;
                    return -1;
                }
                break;
            case parse_json_list_state::accept_list_value:
                if (tk.tk_type == L_BRACKET) {
                    // start of another json list
                    if (parse_json_list(token_list, tk_index_ptr) != 0) {
                        return -1; // Parse fail
                    }
                    curr_state = parse_json_list_state::accept_comma_or_end_bracket;
                }
                else if (tk.tk_type == L_BRACE) {
                    // start of json object
                    if (parse_json_object(token_list, tk_index_ptr) != 0) {
                        return -1;
                    }
                    curr_state = parse_json_list_state::accept_comma_or_end_bracket;
                }
                else if (tk.tk_type == KEYW_TRUE || tk.tk_type == KEYW_FALSE || tk.tk_type == KEYW_NULL 
                    || tk.tk_type == STRING || tk.tk_type == NUMBER) {
                        // valid json list values. Accept it.
                        curr_state = parse_json_list_state::accept_comma_or_end_bracket;
                }
                else {
                    std::cerr << tk.line << ":" << tk.col << " Json list parsing error: Unexpected token ==> " << tk.tk_value;
                    return -1;
                }
                break;
            case parse_json_list_state::accept_comma_or_end_bracket:
                if (tk.tk_type == COMMA) {
                    curr_state = parse_json_list_state::accept_list_value;
                }
                else if (tk.tk_type == R_BRACKET) {
                    return 0; // Json list end and hence parse successful.
                }
                else {
                    std::cerr << tk.line << ":" << tk.col << " Json list parsing error: Unexpected token ==> " << tk.tk_value;
                    return -1;
                }
                break;
        }
    }

    // If control reaches this point, it means we have exhausted the token_list and still not seen closing bracket.
    return -1;
}

// Json object starts with '{' and ends with '}'. Contains different key value pairs separated by ":". Different key-values are separated by ','. 
// Calling this function means we have already seen the starting '{'. Keys must be unique in a json object.
int parse_json_object(std::vector<token_struct> &token_list, int *tk_index_ptr) {
    
    parse_json_obj_state curr_state = parse_json_obj_state::accept_key_or_end_brace;
    std::unordered_set<std::string> json_keys; // to keep keys unique.
    
    while (*tk_index_ptr < token_list.size()) {
        token_struct &tk = token_list[*tk_index_ptr];
        (*tk_index_ptr)++;
        switch(curr_state) {
            case parse_json_obj_state::accept_key_or_end_brace:
                if (tk.tk_type == STRING) {
                    if (json_keys.find(tk.tk_value) == json_keys.end()) {
                        json_keys.insert(tk.tk_value);
                        curr_state = parse_json_obj_state::accept_colon;
                    }
                    else {
                        std::cerr << "Duplicate key not allowed ==> " << tk.tk_value << "line " << tk.line << "col " << tk.col;
                        return -1;
                    }
                }
                else if (tk.tk_type == R_BRACE) {
                    return 0; // Parse successful
                }
                else {
                    std::cerr << "Expected either a string or end brace '}' ==> " << tk.tk_value << "line " << tk.line << "col " << tk.col;
                    return -1;
                }
                break;
            case parse_json_obj_state::accept_key:
                if (tk.tk_type == STRING) {
                    if (json_keys.find(tk.tk_value) == json_keys.end()) {
                        json_keys.insert(tk.tk_value);
                        curr_state = parse_json_obj_state::accept_colon;
                    }
                    else {
                        std::cerr << "Duplicate key not allowed ==> " << tk.tk_value << "line " << tk.line << "col " << tk.col;
                        return -1;
                    }
                }
                else {
                    std::cerr << "Only strings allowed as keys ==> " << tk.tk_value << "line " << tk.line << "col " << tk.col;
                    return -1;
                }
                break;
            case parse_json_obj_state::accept_colon:
                if (tk.tk_type == COLON) {
                    curr_state = parse_json_obj_state::accept_value;
                }
                else {
                    std::cerr << "Expected colon. Found something else ==> " << tk.tk_value << "line " << tk.line << "col " << tk.col;
                    return -1;
                }
                break;
            case parse_json_obj_state::accept_value:
                if (tk.tk_type == L_BRACE) {
                    // Start of another json object
                    if (parse_json_object(token_list, tk_index_ptr) != 0) {
                        return -1;
                    }
                    curr_state = parse_json_obj_state::accept_comma_or_end_brace;
                }
                else if (tk.tk_type == R_BRACE) {
                    // End of json object
                    return 0; // successful parsing
                }
                else if (tk.tk_type == L_BRACKET) {
                    // Start of json list
                    if (parse_json_list(token_list, tk_index_ptr) != 0) {
                        return -1;
                    }
                    curr_state = parse_json_obj_state::accept_comma_or_end_brace;
                }
                else if (tk.tk_type == KEYW_TRUE || tk.tk_type == KEYW_FALSE || tk.tk_type == KEYW_NULL 
                    || tk.tk_type == STRING || tk.tk_type == NUMBER) {
                        // valid json obj values. Accept it.
                    curr_state = parse_json_obj_state::accept_comma_or_end_brace;
                }
                else {
                    std::cerr << "Unexpected token ==> " << tk.tk_value << " line " << tk.line << " col " << tk.col;
                    return -1; 
                }
                break;
            case parse_json_obj_state::accept_comma_or_end_brace:
                if (tk.tk_type == COMMA) {
                    curr_state = parse_json_obj_state::accept_key;
                }
                else if (tk.tk_type == R_BRACE) {
                    return 0; // json obj end.
                }
                else {
                    std::cerr << "Unexpected comma ==> " << tk.tk_value << " line " << tk.line << " col " << tk.col;
                    return -1; 
                }
                break;
        }
    }

    // If control reaches this point, it means we have exhausted the token_list and still not seen closing brace.
    return -1;
}