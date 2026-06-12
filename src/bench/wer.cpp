#include "bench/wer.h"

#include <algorithm>
#include <cctype>

namespace autowhisper::bench {

std::vector<std::string> normalize_words(const std::string& text) {
    std::vector<std::string> words;
    std::string current;

    auto flush = [&]() {
        // Apostrophes are kept only between letters ("don't"); trim any that
        // ended up at the edges.
        while (!current.empty() && current.front() == '\'') current.erase(current.begin());
        while (!current.empty() && current.back() == '\'') current.pop_back();
        if (!current.empty()) words.push_back(current);
        current.clear();
    };

    for (unsigned char c : text) {
        if (std::isalnum(c)) {
            current.push_back(static_cast<char>(std::tolower(c)));
        } else if (c == '\'') {
            current.push_back('\'');
        } else if (c >= 0x80) {
            // Pass non-ASCII bytes through untouched so multilingual text
            // still compares; no case folding is attempted.
            current.push_back(static_cast<char>(c));
        } else {
            flush();
        }
    }
    flush();
    return words;
}

double word_error_rate(const std::string& reference, const std::string& hypothesis) {
    const auto ref = normalize_words(reference);
    const auto hyp = normalize_words(hypothesis);

    if (ref.empty()) return hyp.empty() ? 0.0 : 1.0;

    // Levenshtein with two rolling rows.
    std::vector<size_t> prev(hyp.size() + 1);
    std::vector<size_t> curr(hyp.size() + 1);
    for (size_t j = 0; j <= hyp.size(); j++) prev[j] = j;

    for (size_t i = 1; i <= ref.size(); i++) {
        curr[0] = i;
        for (size_t j = 1; j <= hyp.size(); j++) {
            size_t substitution = prev[j - 1] + (ref[i - 1] == hyp[j - 1] ? 0 : 1);
            size_t deletion = prev[j] + 1;
            size_t insertion = curr[j - 1] + 1;
            curr[j] = std::min({substitution, deletion, insertion});
        }
        std::swap(prev, curr);
    }

    return static_cast<double>(prev[hyp.size()]) / static_cast<double>(ref.size());
}

}  // namespace autowhisper::bench
