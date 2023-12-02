#include <iostream>
#include <fstream>
#include <string>
#include <cstring>
#include <vector>
#include "json_parse.h"

int main(int argc, char *argv[]) {
    std::string filename = "";
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "-h") == 0 || std::strcmp(argv[i], "--help") == 0) {
            std::cout << "Usage: jsonparser <filename>\n\tReturns 0 if the file is valid json file else 1\n"; 
        }
        else {
            filename = argv[i];
            break;
        }
    }
    if (filename == "") {
        return -1;
    }
    
    std::vector<token_struct> token_list;
    std::ifstream inf {filename};
    if (lex(inf, token_list) != 0) {
        return -1;
    }
    /*
    for (int i = 0; i < token_list.size(); ++i) {
        std::cout << token_list[i].tk_value << "|";
    }*/
    if (token_list.size() == 0) {
        std::cout << "Invalid. Json must start with '{' and end with '}'" << std::endl;
        return -1;
    }
    if (token_list[0].tk_type != L_BRACE) {
        std::cerr << "Expected Left brace at start. Found something else ==> " << token_list[0].tk_value << std::endl;
        return -1;
    }

    int tk_list_counter = 1;
    if (parse_json_object(token_list, &tk_list_counter) == 0) {
        std::cout << "valid json" << std::endl;
    }

    return 0;
}