#pragma once

#include "MetasequoiaImeEngine/core/data_path.h"

#include <filesystem>
#include <functional>
#include <stdexcept>
#include <string>
#include <vector>

namespace test
{
// Every dictionary/engine API takes its paths as UTF-8. path::string() converts through the ANSI
// code page instead, which corrupts a non-ASCII path and throws outright on a code page that cannot
// represent the characters -- and the fixtures below all live under the temp directory, which sits
// inside the user profile. Tests must therefore convert with this, never with path::string().
inline std::string Utf8(const std::filesystem::path &path)
{
    return metasequoia::path_to_utf8(path);
}

struct TestCase
{
    std::string name;
    std::function<void()> fn;
};

std::vector<TestCase> &registry();

struct Registrar
{
    Registrar(const char *name, std::function<void()> fn);
};
} // namespace test

#define TEST_CASE(name)                                                                                                \
    static void name();                                                                                                \
    static test::Registrar name##_registrar(#name, name);                                                              \
    static void name()

#define REQUIRE(expr)                                                                                                  \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(expr))                                                                                                   \
        {                                                                                                              \
            throw std::runtime_error("Requirement failed: " #expr);                                                    \
        }                                                                                                              \
    } while (false)

#define REQUIRE_EQ(lhs, rhs)                                                                                           \
    do                                                                                                                 \
    {                                                                                                                  \
        const auto &_lhs = (lhs);                                                                                      \
        const auto &_rhs = (rhs);                                                                                      \
        if (!(_lhs == _rhs))                                                                                           \
        {                                                                                                              \
            throw std::runtime_error("Requirement failed: " #lhs " == " #rhs);                                         \
        }                                                                                                              \
    } while (false)
