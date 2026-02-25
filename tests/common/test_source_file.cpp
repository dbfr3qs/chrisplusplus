#include <gtest/gtest.h>
#include "common/source_file.h"
#include <fstream>
#include <cstdio>

using namespace chris;

class SourceFileTest : public ::testing::Test {
protected:
    void SetUp() override {
        testFilePath_ = "test_temp.chr";
    }

    void TearDown() override {
        std::remove(testFilePath_.c_str());
    }

    void writeTestFile(const std::string& content) {
        std::ofstream out(testFilePath_);
        out << content;
        out.close();
    }

    std::string testFilePath_;
};

TEST_F(SourceFileTest, LoadExistingFile) {
    writeTestFile("func main() {\n    print(\"hello\");\n}\n");
    SourceFile sf(testFilePath_);
    EXPECT_TRUE(sf.load());
    EXPECT_EQ(sf.path(), testFilePath_);
    EXPECT_EQ(sf.lineCount(), 4u); // 3 lines + trailing newline creates 4th offset
}

TEST_F(SourceFileTest, LoadNonExistentFile) {
    SourceFile sf("nonexistent.chr");
    EXPECT_FALSE(sf.load());
}

TEST_F(SourceFileTest, GetLine) {
    writeTestFile("line one\nline two\nline three\n");
    SourceFile sf(testFilePath_);
    EXPECT_TRUE(sf.load());

    EXPECT_EQ(sf.getLine(1), "line one");
    EXPECT_EQ(sf.getLine(2), "line two");
    EXPECT_EQ(sf.getLine(3), "line three");
}

TEST_F(SourceFileTest, GetLineOutOfRange) {
    writeTestFile("single line\n");
    SourceFile sf(testFilePath_);
    EXPECT_TRUE(sf.load());

    EXPECT_EQ(sf.getLine(0), "");
    EXPECT_EQ(sf.getLine(999), "");
}

TEST_F(SourceFileTest, EmptyFile) {
    writeTestFile("");
    SourceFile sf(testFilePath_);
    EXPECT_TRUE(sf.load());
    EXPECT_EQ(sf.content(), "");
    EXPECT_EQ(sf.lineCount(), 1u); // Even empty file has one "line"
}
