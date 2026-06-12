#include "pipeline/transcript_pipeline.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstring>

namespace autowhisper {

namespace {

bool is_word_char(char c) {
    return std::isalnum(static_cast<unsigned char>(c)) || c == '\'' || c == '-';
}

std::string to_lower(const std::string& s) {
    std::string out = s;
    std::transform(out.begin(), out.end(), out.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return out;
}

// Case-insensitive whole-word/phrase search starting at or after `from_pos`.
// Returns std::string::npos when not found.
size_t find_phrase(const std::string& haystack, const std::string& lower_needle,
                   size_t from_pos) {
    if (lower_needle.empty()) return std::string::npos;
    const std::string lower_hay = to_lower(haystack);
    size_t pos = from_pos;
    while ((pos = lower_hay.find(lower_needle, pos)) != std::string::npos) {
        const bool start_ok = pos == 0 || !is_word_char(haystack[pos - 1]);
        const size_t end = pos + lower_needle.size();
        const bool end_ok = end >= haystack.size() || !is_word_char(haystack[end]);
        if (start_ok && end_ok) return pos;
        pos += 1;
    }
    return std::string::npos;
}

// English hesitation sounds whisper transcribes as standalone words. Kept
// deliberately conservative: real words ("a", "like", "so") never qualify.
constexpr std::array<const char*, 6> kEnglishFillers = {
    "um", "uh", "uhm", "erm", "hmm", "mhm",
};

// Removes [begin, end) as a "spoken hole": swallows one trailing comma or
// semicolon plus surrounding whitespace, and re-glues with a single space
// when a word would otherwise butt against the previous character. Returns
// the position right after the hole.
size_t erase_word_hole(std::string& out, size_t begin, size_t end) {
    if (end < out.size() && (out[end] == ',' || out[end] == ';')) {
        end++;
    }
    while (end < out.size() && std::isspace(static_cast<unsigned char>(out[end]))) {
        end++;
    }
    while (begin > 0 && std::isspace(static_cast<unsigned char>(out[begin - 1]))) {
        begin--;
    }

    std::string glue;
    const bool word_after = end < out.size() && is_word_char(out[end]);
    if (word_after && begin > 0 && out[begin - 1] != '\n') glue = " ";

    out.replace(begin, end - begin, glue);
    return begin + glue.size();
}

}  // namespace

TranscriptPipeline::TranscriptPipeline(const FormattingConfig& config, std::string language)
    : config_(config), language_(std::move(language)) {
    for (const auto& raw : config_.dictionary) {
        DictionaryEntry entry;
        if (parse_dictionary_entry(raw, entry)) {
            dictionary_.push_back(std::move(entry));
        } else {
            spdlog::warn("Ignoring malformed dictionary entry (want 'spoken => written'): {}",
                         raw);
        }
    }
}

bool TranscriptPipeline::parse_dictionary_entry(const std::string& raw, DictionaryEntry& out) {
    const auto sep = raw.find("=>");
    if (sep == std::string::npos) return false;

    auto trim = [](std::string s) {
        while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) s.erase(s.begin());
        while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) s.pop_back();
        return s;
    };

    std::string from = trim(raw.substr(0, sep));
    std::string to = trim(raw.substr(sep + 2));
    if (from.empty()) return false;

    out.from = std::move(from);
    out.to = std::move(to);
    return true;
}

std::string TranscriptPipeline::process(const std::string& text) const {
    std::string out = text;
    if (!dictionary_.empty()) out = apply_dictionary(out);
    if (config_.remove_fillers) out = remove_fillers(out);
    if (config_.spoken_commands) out = apply_spoken_commands(out);
    return normalize(out);
}

std::string TranscriptPipeline::apply_dictionary(const std::string& text) const {
    std::string out = text;
    for (const auto& entry : dictionary_) {
        const std::string lower_from = to_lower(entry.from);
        size_t pos = 0;
        while ((pos = find_phrase(out, lower_from, pos)) != std::string::npos) {
            if (entry.to.empty()) {
                // Deletion entry: remove the phrase like a filler.
                pos = erase_word_hole(out, pos, pos + entry.from.size());
                continue;
            }
            std::string replacement = entry.to;
            // Preserve a leading capital when the user wrote the target in
            // lowercase ("teh => the" still fixes "Teh").
            if (std::islower(static_cast<unsigned char>(entry.to[0])) &&
                std::isupper(static_cast<unsigned char>(out[pos]))) {
                replacement[0] =
                    static_cast<char>(std::toupper(static_cast<unsigned char>(replacement[0])));
            }
            out.replace(pos, entry.from.size(), replacement);
            pos += replacement.size();
        }
    }
    return out;
}

std::string TranscriptPipeline::remove_fillers(const std::string& text) const {
    if (language_ != "en" && language_ != "auto" && !language_.empty()) {
        // Filler lists are language-specific; only English ships today.
        return text;
    }

    std::string out = text;
    for (const char* filler : kEnglishFillers) {
        const std::string lower_filler = filler;
        size_t pos = 0;
        while ((pos = find_phrase(out, lower_filler, pos)) != std::string::npos) {
            pos = erase_word_hole(out, pos, pos + lower_filler.size());
        }
    }
    return out;
}

std::string TranscriptPipeline::apply_spoken_commands(const std::string& text) const {
    struct Command {
        const char* phrase;
        const char* replacement;
    };
    // Order matters: the longer phrase must match before its prefix.
    constexpr std::array<Command, 3> commands = {{
        {"new paragraph", "\n\n"},
        {"new line", "\n"},
        {"newline", "\n"},
    }};

    std::string out = text;
    for (const auto& cmd : commands) {
        const std::string lower_phrase = cmd.phrase;
        size_t pos = 0;
        while ((pos = find_phrase(out, lower_phrase, pos)) != std::string::npos) {
            size_t begin = pos;
            size_t end = pos + lower_phrase.size();

            // Only treat the phrase as a command when it forms its own
            // clause: bounded by punctuation or the ends of the utterance.
            // "we shipped a new line of products" stays untouched.
            size_t left = begin;
            while (left > 0 && std::isspace(static_cast<unsigned char>(out[left - 1]))) left--;
            const bool left_boundary =
                left == 0 || out[left - 1] == ',' || out[left - 1] == '.' ||
                out[left - 1] == ';' || out[left - 1] == ':' || out[left - 1] == '\n';

            size_t right = end;
            while (right < out.size() &&
                   std::isspace(static_cast<unsigned char>(out[right]))) {
                right++;
            }
            const bool right_boundary =
                right >= out.size() || out[right] == ',' || out[right] == '.' ||
                out[right] == ';' || out[right] == ':' || out[right] == '\n' ||
                std::isupper(static_cast<unsigned char>(out[right]));

            if (!(left_boundary && right_boundary)) {
                pos = end;
                continue;
            }

            // The break replaces the command and its trailing delimiter;
            // punctuation on the left ("First point, new line ...") is the
            // user's and stays put.
            size_t erase_begin = left;
            size_t erase_end = right;
            if (erase_end < out.size() &&
                (out[erase_end] == ',' || out[erase_end] == '.' ||
                 out[erase_end] == ';' || out[erase_end] == ':')) {
                erase_end++;
                while (erase_end < out.size() &&
                       std::isspace(static_cast<unsigned char>(out[erase_end]))) {
                    erase_end++;
                }
            }

            out.replace(erase_begin, erase_end - erase_begin, cmd.replacement);
            pos = erase_begin + std::strlen(cmd.replacement);
        }
    }
    return out;
}

std::string TranscriptPipeline::normalize(const std::string& text) {
    std::string out;
    out.reserve(text.size());

    // Collapse runs of spaces/tabs (newlines survive) and drop spaces
    // before closing punctuation left behind by earlier stages.
    for (size_t i = 0; i < text.size(); i++) {
        char c = text[i];
        if (c == ' ' || c == '\t') {
            size_t j = i;
            while (j < text.size() && (text[j] == ' ' || text[j] == '\t')) j++;
            const bool at_edge = out.empty() || out.back() == '\n';
            const bool before_punct =
                j < text.size() && (text[j] == ',' || text[j] == '.' || text[j] == '!' ||
                                    text[j] == '?' || text[j] == ';' || text[j] == ':' ||
                                    text[j] == '\n');
            if (!at_edge && !before_punct) out.push_back(' ');
            i = j - 1;
            continue;
        }
        out.push_back(c);
    }

    // Trim trailing whitespace.
    while (!out.empty() && std::isspace(static_cast<unsigned char>(out.back()))) {
        out.pop_back();
    }

    // Repair sentence capitalization after the holes punched above: the
    // first letter of the text and the first letter after . ! ? or a line
    // break become uppercase.
    bool capitalize_next = true;
    for (char& c : out) {
        if (std::isalpha(static_cast<unsigned char>(c))) {
            if (capitalize_next) {
                c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
            }
            capitalize_next = false;
        } else if (c == '.' || c == '!' || c == '?' || c == '\n') {
            capitalize_next = true;
        } else if (!std::isspace(static_cast<unsigned char>(c)) && c != '"' && c != '\'') {
            capitalize_next = false;
        }
    }

    return out;
}

} // namespace autowhisper
