#pragma once

#include <nlohmann/json_fwd.hpp>

#include <optional>
#include <string_view>
#include <vector>

namespace autowhisper::schema {

enum class Type {
    String,
    Int,
    Float,
    Bool,
    StringArray,
    Enum,
};

struct KeyDef {
    std::string_view section;
    std::string_view key;
    Type type;
    // Non-empty only when type == Enum.
    std::vector<std::string_view> enum_values;
    // Inclusive bounds on numeric types. nullopt = unbounded on that side.
    std::optional<double> min_numeric;
    std::optional<double> max_numeric;
    std::string_view description;
    // Defaults are NOT stored here — they live in Config{} member initializers
    // and are exposed to the UI via GET /api/defaults. See design doc.
};

// The full schema in declaration order. Stable across runs.
const std::vector<KeyDef>& all();

// Returns nullptr if (section, key) is not in the schema.
const KeyDef* find(std::string_view section, std::string_view key);

// Stable schema version for settings/config migrations.
int version();

bool is_known_section(std::string_view section);
bool is_known_key(std::string_view section, std::string_view key);

// Serialize the schema for the UI.
// Shape: { "<section>": [ { "key": "...", "type": "enum", "enum_values": [...],
//                           "min_numeric": <num or null>, "max_numeric": <num or null>,
//                           "description": "..." }, ... ], ... }
nlohmann::json to_json();

}  // namespace autowhisper::schema
