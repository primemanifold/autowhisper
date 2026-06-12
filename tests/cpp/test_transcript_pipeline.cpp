#include <catch2/catch_test_macros.hpp>

#include "pipeline/transcript_pipeline.h"

using namespace autowhisper;

namespace {

FormattingConfig all_on(std::vector<std::string> dictionary = {}) {
    FormattingConfig cfg;
    cfg.remove_fillers = true;
    cfg.spoken_commands = true;
    cfg.dictionary = std::move(dictionary);
    return cfg;
}

std::string run(const std::string& text, FormattingConfig cfg = all_on(),
                const std::string& language = "en") {
    return TranscriptPipeline(cfg, language).process(text);
}

}  // namespace

// ---- filler removal ----

TEST_CASE("fillers are removed with their trailing comma", "[pipeline]") {
    CHECK(run("Um, I think we should go.") == "I think we should go.");
    CHECK(run("I think, uh, we should go.") == "I think, we should go.");
    CHECK(run("So, um, yeah.") == "So, yeah.");
}

TEST_CASE("mid-sentence fillers keep one separating space", "[pipeline]") {
    CHECK(run("I think um we should go.") == "I think we should go.");
    CHECK(run("It was uh uh really good.") == "It was really good.");
}

TEST_CASE("filler removal is case-insensitive and capitalizes sentence starts", "[pipeline]") {
    CHECK(run("Uh, the meeting moved.") == "The meeting moved.");
    CHECK(run("Right. Um, the meeting moved.") == "Right. The meeting moved.");
}

TEST_CASE("words containing filler letters survive", "[pipeline]") {
    CHECK(run("The umbrella is uhlstrom's.") == "The umbrella is uhlstrom's.");
    CHECK(run("Set the column maximum.") == "Set the column maximum.");
}

TEST_CASE("fillers stay when disabled or non-English", "[pipeline]") {
    auto cfg = all_on();
    cfg.remove_fillers = false;
    CHECK(run("Um, hello.", cfg) == "Um, hello.");
    CHECK(run("Um, hallo.", all_on(), "de") == "Um, hallo.");
    // auto-detected language assumes the English list still applies
    CHECK(run("Um, hello.", all_on(), "auto") == "Hello.");
}

// ---- spoken commands ----

TEST_CASE("standalone new line and new paragraph become breaks", "[pipeline]") {
    CHECK(run("First point, new line, second point.") == "First point,\nSecond point.");
    CHECK(run("Hello. New paragraph. The next topic.") == "Hello.\n\nThe next topic.");
    CHECK(run("Dear team, new paragraph, the launch is ready.") ==
          "Dear team,\n\nThe launch is ready.");
}

TEST_CASE("new line inside ordinary prose is preserved", "[pipeline]") {
    CHECK(run("We shipped a new line of products.") == "We shipped a new line of products.");
    CHECK(run("The new paragraph styles look great.") ==
          "The new paragraph styles look great.");
}

TEST_CASE("spoken commands can be disabled", "[pipeline]") {
    auto cfg = all_on();
    cfg.spoken_commands = false;
    CHECK(run("First, new line, second.", cfg) == "First, new line, second.");
}

// ---- dictionary ----

TEST_CASE("dictionary replaces whole phrases case-insensitively", "[pipeline]") {
    auto cfg = all_on({"auto whisper => AutoWhisper"});
    CHECK(run("I use auto whisper daily.", cfg) == "I use AutoWhisper daily.");
    CHECK(run("Auto Whisper is fast.", cfg) == "AutoWhisper is fast.");
}

TEST_CASE("dictionary preserves leading capitalization for lowercase targets", "[pipeline]") {
    auto cfg = all_on({"acme corp => acme corporation"});
    CHECK(run("Acme corp called.", cfg) == "Acme corporation called.");
    CHECK(run("We met acme corp.", cfg) == "We met acme corporation.");
}

TEST_CASE("dictionary does not replace inside larger words", "[pipeline]") {
    auto cfg = all_on({"cat => dog"});
    CHECK(run("The catalog lists a cat.", cfg) == "The catalog lists a dog.");
}

TEST_CASE("malformed dictionary entries are ignored", "[pipeline]") {
    auto cfg = all_on({"no separator here", " => empty lhs", "good => fine"});
    CHECK(run("This is good.", cfg) == "This is fine.");

    TranscriptPipeline::DictionaryEntry entry;
    CHECK_FALSE(TranscriptPipeline::parse_dictionary_entry("no separator", entry));
    CHECK_FALSE(TranscriptPipeline::parse_dictionary_entry(" => x", entry));
    REQUIRE(TranscriptPipeline::parse_dictionary_entry("a => b", entry));
    CHECK(entry.from == "a");
    CHECK(entry.to == "b");
}

TEST_CASE("dictionary entry with empty replacement deletes the phrase", "[pipeline]") {
    auto cfg = all_on({"you know =>"});
    CHECK(run("It was, you know, fine.", cfg) == "It was, fine.");
}

// ---- normalization ----

TEST_CASE("normalize collapses spaces and repairs punctuation spacing", "[pipeline]") {
    CHECK(TranscriptPipeline::normalize("hello   world .") == "Hello world.");
    CHECK(TranscriptPipeline::normalize("  leading and trailing  ") == "Leading and trailing");
}

TEST_CASE("normalize capitalizes after sentence enders and line breaks", "[pipeline]") {
    CHECK(TranscriptPipeline::normalize("one. two! three? four") == "One. Two! Three? Four");
    CHECK(TranscriptPipeline::normalize("list:\nfirst\nsecond") == "List:\nFirst\nSecond");
}

// ---- combined ----

TEST_CASE("stages compose over a realistic utterance", "[pipeline]") {
    auto cfg = all_on({"auto whisper => AutoWhisper"});
    CHECK(run("Um, so auto whisper, uh, works offline. New line. It's, um, fast too.", cfg) ==
          "So AutoWhisper, works offline.\nIt's, fast too.");
}

TEST_CASE("pipeline leaves clean text untouched", "[pipeline]") {
    const std::string clean = "And so my fellow Americans, ask not what your country can do "
                              "for you, ask what you can do for your country.";
    CHECK(run(clean) == clean);
}

TEST_CASE("pipeline handles empty and whitespace input", "[pipeline]") {
    CHECK(run("") == "");
    CHECK(run("   ") == "");
    CHECK(run("um") == "");
    CHECK(run("Um, uh.") == ".");
}
