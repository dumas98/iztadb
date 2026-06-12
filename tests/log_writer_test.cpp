//
// Created by Daniel Dumas on 11/06/26.
//

#include <gtest/gtest.h>
#include "log_writer.h"

using namespace std;

/**
 * @brief Test fixture for MemTable.
 * Builds a MemTable instance before each test.
 *
 */
class LogWriterTest : public testing::Test {
protected:
    LogWriter log_writer;

    // Before Each.
    void SetUp() override {
        log_writer = LogWriter();
        std::filesystem::create_directories("./test_data/wal");
    }

    // After Each.
    void TearDown() override {
        // Dropped directories.
        std::filesystem::remove_all("./test_data/wal");
    }
};

// Verifies Put works correctly when trying to retrieve a previously inserted value.
TEST_F(LogWriterTest, PutAndGet) {
    log_writer.get_write_path();
    mem_table.put("lastname1", "Dumas");
    EXPECT_EQ(mem_table.get("name1"), "Daniel");
    EXPECT_EQ(mem_table.get("lastname1"), "Dumas");
}

