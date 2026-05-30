//
// Created by Daniel Dumas on 29/05/26.
//

#include <unordered_map>
#include <iostream>

int unordered_map_commands(){
    // Build an unordered map, add three records.
    std::unordered_map<std::string, std::string> kvstore = {
        {"key1", "value1"},
        {"key2", "value2"},
        {"key3", "value3"},
    };

    // Insert Records.
    kvstore["key4"] = "value4";
    kvstore["key5"] = "value5";
    kvstore["key6"] = "value6";

    // Print a record.
    std::cout << "key2" << ": " << kvstore["key2"] << std::endl;

    // Safer to avoid "looking for a value and accidentally injecting it."
    std::cout << "key3" << ": " << kvstore.at("key3") << std::endl;

    // Iterate through each value.
    std::cout << "Iterate through each value" << std::endl;
    for (auto& [key, value] : kvstore) {
        std::cout << key << ": " << value << "\n";
    }

}