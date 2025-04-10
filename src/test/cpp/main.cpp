#include <clocale>

#include <gtest/gtest.h>

#include "ulight/ulight.h"

int main(int argc, char** argv)
{
    std::setlocale(LC_ALL, ".UTF8");
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
