//
// Created by Daniel Dumas on 27/05/26.
//

#include <iostream>
#include "iztadb.h"
#include "log_writer.h"

int main() {
    std::cout << "Hello, I'm IztaDB!" << std::endl;

    IztaDB izta_db1 = IztaDB();

    LogWriter log_writer = LogWriter();

    std::cout << "WAL write path: "<< log_writer.get_write_path() << std::endl;

    return 0;
}
