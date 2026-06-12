#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "bench/wer.h"

using namespace autowhisper::bench;
using Catch::Approx;

TEST_CASE("normalize_words lowercases and strips punctuation", "[wer]") {
    auto words = normalize_words("And so, my fellow Americans!");
    REQUIRE(words == std::vector<std::string>{"and", "so", "my", "fellow", "americans"});
}

TEST_CASE("normalize_words keeps intra-word apostrophes", "[wer]") {
    auto words = normalize_words("Don't stop 'quoted'");
    REQUIRE(words == std::vector<std::string>{"don't", "stop", "quoted"});
}

TEST_CASE("normalize_words handles empty and whitespace input", "[wer]") {
    CHECK(normalize_words("").empty());
    CHECK(normalize_words("  \t\n .,!? ").empty());
}

TEST_CASE("word_error_rate is zero for identical text", "[wer]") {
    CHECK(word_error_rate("hello world", "hello world") == 0.0);
    CHECK(word_error_rate("Hello, World!", "hello world") == 0.0);
}

TEST_CASE("word_error_rate counts substitutions", "[wer]") {
    CHECK(word_error_rate("the cat sat", "the dog sat") == Approx(1.0 / 3.0));
}

TEST_CASE("word_error_rate counts insertions and deletions", "[wer]") {
    CHECK(word_error_rate("the cat sat", "the cat") == Approx(1.0 / 3.0));
    CHECK(word_error_rate("the cat", "the big cat") == Approx(1.0 / 2.0));
}

TEST_CASE("word_error_rate can exceed one", "[wer]") {
    CHECK(word_error_rate("yes", "no no no") == Approx(3.0));
}

TEST_CASE("word_error_rate edge cases", "[wer]") {
    CHECK(word_error_rate("", "") == 0.0);
    CHECK(word_error_rate("", "something") == 1.0);
    CHECK(word_error_rate("something", "") == Approx(1.0));
}
