#pragma once

#include <string>
#include <vector>

namespace autowhisper::bench {

// Lowercases, strips punctuation (keeping intra-word apostrophes), and
// splits on whitespace — the usual ASR scoring normalization.
std::vector<std::string> normalize_words(const std::string& text);

// Word error rate: word-level Levenshtein distance between hypothesis and
// reference, divided by reference length. Returns 0 when both are empty,
// 1 when the reference is empty but the hypothesis is not.
double word_error_rate(const std::string& reference, const std::string& hypothesis);

}  // namespace autowhisper::bench
