// Tests the OutputManager fallback state machine end-to-end using a fake
// PlatformOutput. No X11 / Cocoa dependencies — runs on any platform.

#include "output/output.h"
#include "output/platform_output.h"

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

using namespace autowhisper;

namespace {

struct Call {
    enum Kind { INJECT, COPY, PASTE, RETURN_KEY };
    Kind kind;
    std::string text;
};

class FakePlatform : public PlatformOutput {
public:
    std::vector<Call> calls;
    bool inject_ok = true;
    bool copy_ok = true;
    bool paste_ok = true;
    bool return_ok = true;
    bool post_events_allowed = true;
    bool clipboard_allowed = true;

    bool inject(const std::string& text) override {
        calls.push_back({Call::INJECT, text});
        return inject_ok;
    }
    bool copy_to_clipboard(const std::string& text) override {
        calls.push_back({Call::COPY, text});
        return copy_ok;
    }
    bool send_paste() override {
        calls.push_back({Call::PASTE, ""});
        return paste_ok;
    }
    bool send_return_key() override {
        calls.push_back({Call::RETURN_KEY, ""});
        return return_ok;
    }
    bool can_post_events() const override { return post_events_allowed; }
    bool can_copy_to_clipboard() const override { return clipboard_allowed; }
    std::string name() const override { return "fake"; }
};

OutputConfig make_config(const std::string& method = "inject",
                         bool auto_paste = true,
                         const std::string& ending = "none",
                         bool also_copy = false) {
    OutputConfig c;
    c.method = method;
    c.auto_paste = auto_paste;
    c.paste_delay = 0.0f;
    c.ending_action = ending;
    c.also_copy_to_clipboard = also_copy;
    return c;
}

} // namespace

TEST_CASE("empty text is a no-op") {
    auto fake = std::make_unique<FakePlatform>();
    auto* raw = fake.get();
    OutputManager om(make_config(), std::move(fake));
    om.initialize();
    REQUIRE(om.inject("") == true);
    REQUIRE(raw->calls.empty());
    REQUIRE(om.last_outcome() == OutputManager::Outcome::NOOP_EMPTY);
}

TEST_CASE("direct inject succeeds when post_events is allowed") {
    auto fake = std::make_unique<FakePlatform>();
    auto* raw = fake.get();
    OutputManager om(make_config(), std::move(fake));
    om.initialize();
    REQUIRE(om.inject("hello") == true);
    REQUIRE(raw->calls.size() == 1);
    REQUIRE(raw->calls[0].kind == Call::INJECT);
    REQUIRE(raw->calls[0].text == "hello");
    REQUIRE(om.last_outcome() == OutputManager::Outcome::INJECTED);
}

TEST_CASE("also_copy_to_clipboard copies first, then injects") {
    auto fake = std::make_unique<FakePlatform>();
    auto* raw = fake.get();
    OutputManager om(make_config("inject", true, "none", /*also_copy=*/true),
                     std::move(fake));
    om.initialize();
    REQUIRE(om.inject("hi") == true);
    REQUIRE(raw->calls.size() == 2);
    REQUIRE(raw->calls[0].kind == Call::COPY);
    REQUIRE(raw->calls[1].kind == Call::INJECT);
}

TEST_CASE("falls back to clipboard when inject fails") {
    auto fake = std::make_unique<FakePlatform>();
    fake->inject_ok = false;
    auto* raw = fake.get();
    OutputManager om(make_config("inject", true), std::move(fake));
    om.initialize();
    REQUIRE(om.inject("hey") == true);
    // inject attempted once, then copy + paste.
    REQUIRE(raw->calls.size() == 3);
    REQUIRE(raw->calls[0].kind == Call::INJECT);
    REQUIRE(raw->calls[1].kind == Call::COPY);
    REQUIRE(raw->calls[2].kind == Call::PASTE);
    REQUIRE(om.last_outcome() == OutputManager::Outcome::CLIPBOARD_PASTED);
}

TEST_CASE("degraded mode: post_events denied skips inject and paste") {
    auto fake = std::make_unique<FakePlatform>();
    fake->post_events_allowed = false;  // codex #4 scenario
    auto* raw = fake.get();
    OutputManager om(make_config("inject", true), std::move(fake));
    om.initialize();
    REQUIRE(om.inject("text") == true);
    // Only copy. No inject, no paste (both would fail).
    REQUIRE(raw->calls.size() == 1);
    REQUIRE(raw->calls[0].kind == Call::COPY);
    REQUIRE(om.last_outcome() == OutputManager::Outcome::CLIPBOARD_ONLY);
}

TEST_CASE("degraded mode: clipboard copy fails is a hard failure") {
    auto fake = std::make_unique<FakePlatform>();
    fake->post_events_allowed = false;
    fake->copy_ok = false;
    auto* raw = fake.get();
    OutputManager om(make_config(), std::move(fake));
    om.initialize();
    REQUIRE(om.inject("x") == false);
    REQUIRE(raw->calls.size() == 1);
    REQUIRE(raw->calls[0].kind == Call::COPY);
    REQUIRE(om.last_outcome() == OutputManager::Outcome::FAILED);
}

TEST_CASE("method=clipboard skips direct inject") {
    auto fake = std::make_unique<FakePlatform>();
    auto* raw = fake.get();
    OutputManager om(make_config("clipboard"), std::move(fake));
    om.initialize();
    REQUIRE(om.inject("c") == true);
    REQUIRE(raw->calls.size() == 2);
    REQUIRE(raw->calls[0].kind == Call::COPY);
    REQUIRE(raw->calls[1].kind == Call::PASTE);
}

TEST_CASE("ending_action=return_key appends a return after inject") {
    auto fake = std::make_unique<FakePlatform>();
    auto* raw = fake.get();
    OutputManager om(make_config("inject", true, "return_key"), std::move(fake));
    om.initialize();
    REQUIRE(om.inject("ok") == true);
    REQUIRE(raw->calls.size() == 2);
    REQUIRE(raw->calls[0].kind == Call::INJECT);
    REQUIRE(raw->calls[1].kind == Call::RETURN_KEY);
}

TEST_CASE("ending_action=newline appends to text, not as return key") {
    auto fake = std::make_unique<FakePlatform>();
    auto* raw = fake.get();
    OutputManager om(make_config("inject", true, "newline"), std::move(fake));
    om.initialize();
    REQUIRE(om.inject("ok") == true);
    // Only INJECT; the \n was appended to the text string.
    REQUIRE(raw->calls.size() == 1);
    REQUIRE(raw->calls[0].kind == Call::INJECT);
    REQUIRE(raw->calls[0].text == "ok\n");
}
