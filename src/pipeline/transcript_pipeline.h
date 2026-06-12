#pragma once

#include "config/config.h"

#include <string>
#include <vector>

namespace autowhisper {

// Deterministic post-ASR cleanup between WhisperInference and OutputManager
// (Tier 1 of the intelligence layer; see the production plan, W4). All
// stages are rule-based and run offline:
//
//   1. dictionary  — user "spoken => written" replacements, whole-word,
//                    case-insensitive, capitalization-preserving
//   2. fillers     — drop standalone hesitation words (um, uh, ...) and
//                    their trailing comma (English only)
//   3. commands    — "new line" / "new paragraph" clauses become real
//                    line breaks
//   4. normalize   — whitespace/punctuation spacing and sentence
//                    capitalization repair after the edits above
class TranscriptPipeline {
public:
    TranscriptPipeline(const FormattingConfig& config, std::string language);

    std::string process(const std::string& text) const;

    // Individual stages, exposed for testing and reuse.
    std::string apply_dictionary(const std::string& text) const;
    std::string remove_fillers(const std::string& text) const;
    std::string apply_spoken_commands(const std::string& text) const;
    static std::string normalize(const std::string& text);

    struct DictionaryEntry {
        std::string from;
        std::string to;
    };

    // Parses "spoken => written". Returns false (and leaves `out` untouched)
    // for entries without a separator or with an empty "from" side.
    static bool parse_dictionary_entry(const std::string& raw, DictionaryEntry& out);

private:
    FormattingConfig config_;
    std::string language_;
    std::vector<DictionaryEntry> dictionary_;
};

} // namespace autowhisper
