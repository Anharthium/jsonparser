#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include "json_parse.h"

std::vector<std::string> test_files_list = {
    "step1/invalid.json",
    "step1/valid.json",
    "step2/invalid.json",
    "step2/invalid2.json",
    "step2/valid.json",
    "step2/valid2.json",
    "step3/invalid.json",
    "step3/valid.json",
    "step4/invalid.json",
    "step4/valid.json",
    "step4/valid2.json",
};

void run_test(std::string filename) {
    std::cout << "Running test on file: " << filename << std::endl;
    std::vector<token_struct> token_list;
    std::ifstream inf {filename};
    if (lex(inf, token_list) != 0) {
        std::cout << " ==> Invalid" << std::endl;
        return;
    }

    if (token_list.size() == 0) {
        std::cout << " ==> Invalid" << std::endl;
        return;
    }

    if (token_list[0].tk_type != L_BRACE) {
        std::cout << " ==> Invalid" << std::endl;
        return;
    }

    int tk_list_counter = 1;
    if (parse_json_object(token_list, &tk_list_counter) == 0) {
        std::cout << " ==> Valid" << std::endl;
    }
    else {
        std::cout << " ==> Invalid" << std::endl;
    }
}

int main() {
    for (auto filename: test_files_list) {
        filename =  "tests/" + filename;
        run_test(filename);
    }

    return 0;
}