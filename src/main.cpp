//
// Created by Daniel Dumas on 27/05/26.
//

#include <iostream>
#include "iztadb.h"

// Test const in functions
void shout(std::string s) {
    std::cout << "shout: " << s << std::endl;
}

void shout2(std::string& s) {
    std::cout << "shout: " << s << std::endl;
    s = "HELLO";
}

int main() {
    std::cout << "Hello, I'm IztaDB!" << std::endl;
    // unordered_map_commands();

    std::string str1 = "hello";
    shout(str1);

    shout2(str1);
    std::cout << str1 << std::endl;

    return 0;
}
