#include "tray/tray.h"
#include "audio/audio.h"
#include "config/config.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <fstream>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

namespace autowhisper {

// Model/compute/device/language option lists
static const std::vector<std::pair<std::string, std::string>> MODEL_SIZES = {
    {"tiny.en", "Tiny (English) - Fastest"},
    {"base.en", "Base (English)"},
    {"small.en", "Small (English)"},
    {"distil-small.en", "Distil Small (English)"},
    {"medium.en", "Medium (English)"},
    {"distil-medium.en", "Distil Medium (English)"},
    {"distil-large-v3", "Distil Large v3 - Best"},
    {"large-v3", "Large v3"},
};

static const std::vector<std::pair<std::string, std::string>> COMPUTE_TYPES = {
    {"float16", "Float16 (Default)"},
    {"bfloat16", "BFloat16 (RTX 50xx)"},
    {"int8", "Int8 (Smallest)"},
    {"int8_float16", "Int8+Float16"},
    {"float32", "Float32 (Slowest)"},
};

static const std::vector<std::pair<std::string, std::string>> DEVICES = {
    {"cuda", "CUDA (GPU)"},
    {"cpu", "CPU"},
    {"auto", "Auto"},
};

static const std::vector<std::pair<std::string, std::string>> LANGUAGES = {
    {"en", "English"},
    {"auto", "Auto-detect"},
    {"es", "Spanish"},
    {"fr", "French"},
    {"de", "German"},
    {"ja", "Japanese"},
    {"zh", "Chinese"},
};

static const std::vector<std::pair<std::string, std::string>> ENDING_ACTIONS = {
    {"none", "None"},
    {"newline", "Newline character"},
    {"return_key", "Return keypress (submit)"},
};

// Normalize X11 key names to our internal format
static std::string normalize_gdk_key(const std::string& keyname) {
    if (keyname == "Shift_L" || keyname == "Shift_R") return "shift";
    if (keyname == "Control_L" || keyname == "Control_R") return "ctrl";
    if (keyname == "Alt_L" || keyname == "Alt_R") return "alt";
    if (keyname == "Super_L" || keyname == "Super_R") return "super";
    if (keyname == "Meta_L" || keyname == "Meta_R") return "super";
    if (keyname == "Escape") return "esc";
    if (keyname == "Return") return "enter";
    if (keyname == "space") return "space";
    if (keyname == "Tab") return "tab";

    // Function keys
    if (keyname.size() > 1 && keyname[0] == 'F' && isdigit(keyname[1])) {
        std::string lower = keyname;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
        return lower;
    }

    // Single char
    if (keyname.size() == 1) {
        return std::string(1, static_cast<char>(tolower(keyname[0])));
    }

    return "";
}

// ========== HotkeyRow ==========
struct HotkeyRowData {
    GtkWidget* box;
    GtkWidget* button;
    GtkWidget* remove_btn;
    std::string hotkey;
    std::set<std::string> keys_pressed;
    bool capturing = false;
    std::function<void(HotkeyRowData*)> on_remove;
};

static void hotkey_row_update_label(HotkeyRowData* row) {
    if (row->capturing) {
        gtk_button_set_label(GTK_BUTTON(row->button), "Press keys...");
    } else if (!row->hotkey.empty()) {
        gtk_button_set_label(GTK_BUTTON(row->button), display_hotkey(row->hotkey).c_str());
    } else {
        gtk_button_set_label(GTK_BUTTON(row->button), "(click to set)");
    }
}

static HotkeyRowData* create_hotkey_row(const std::string& hotkey,
                                          std::function<void(HotkeyRowData*)> on_remove,
                                          bool removable) {
    auto* row = new HotkeyRowData();
    row->hotkey = hotkey;
    row->on_remove = std::move(on_remove);

    row->box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);

    row->button = gtk_button_new();
    gtk_widget_set_size_request(row->button, 180, 36);
    hotkey_row_update_label(row);
    g_signal_connect(row->button, "clicked",
        G_CALLBACK(+[](GtkButton*, gpointer data) {
            auto* r = static_cast<HotkeyRowData*>(data);
            r->capturing = true;
            r->keys_pressed.clear();
            hotkey_row_update_label(r);
        }), row);
    gtk_box_pack_start(GTK_BOX(row->box), row->button, TRUE, TRUE, 0);

    row->remove_btn = gtk_button_new_with_label("\xe2\x9c\x95"); // ✕
    gtk_widget_set_size_request(row->remove_btn, 36, 36);
    gtk_widget_set_sensitive(row->remove_btn, removable ? TRUE : FALSE);
    g_signal_connect(row->remove_btn, "clicked",
        G_CALLBACK(+[](GtkButton*, gpointer data) {
            auto* r = static_cast<HotkeyRowData*>(data);
            if (r->on_remove) r->on_remove(r);
        }), row);
    gtk_box_pack_start(GTK_BOX(row->box), row->remove_btn, FALSE, FALSE, 0);

    return row;
}

// ========== HotkeyGroup ==========
struct HotkeyGroupData {
    GtkWidget* frame;
    GtkWidget* rows_box;
    std::vector<HotkeyRowData*> rows;
    std::string default_hotkey;

    void update_remove_buttons() {
        for (auto* r : rows) {
            gtk_widget_set_sensitive(r->remove_btn, rows.size() > 1 ? TRUE : FALSE);
        }
    }

    void add_row(const std::string& hk) {
        auto* row = create_hotkey_row(hk, [this](HotkeyRowData* r) { remove_row(r); }, rows.size() > 0);
        rows.push_back(row);
        gtk_box_pack_start(GTK_BOX(rows_box), row->box, FALSE, FALSE, 0);
        gtk_widget_show_all(row->box);
        update_remove_buttons();
    }

    void remove_row(HotkeyRowData* row) {
        if (rows.size() <= 1) return;
        auto it = std::find(rows.begin(), rows.end(), row);
        if (it != rows.end()) {
            gtk_container_remove(GTK_CONTAINER(rows_box), row->box);
            rows.erase(it);
            delete row;
            update_remove_buttons();
        }
    }

    std::vector<std::string> get_hotkeys() const {
        std::vector<std::string> result;
        for (const auto* r : rows) {
            if (!r->hotkey.empty()) result.push_back(r->hotkey);
        }
        return result;
    }

    bool handle_key_press(const std::string& keyname) {
        for (auto* r : rows) {
            if (!r->capturing) continue;

            if (keyname == "Escape") {
                r->capturing = false;
                r->keys_pressed.clear();
                hotkey_row_update_label(r);
                return true;
            }
            if (keyname == "BackSpace") {
                r->hotkey = "";
                r->keys_pressed.clear();
                r->capturing = false;
                hotkey_row_update_label(r);
                return true;
            }

            std::string normalized = normalize_gdk_key(keyname);
            if (!normalized.empty()) {
                r->keys_pressed.insert(normalized);
                // Update display
                std::string display;
                for (const auto& k : r->keys_pressed) {
                    if (!display.empty()) display += "+";
                    display += display_key(k);
                }
                display += "...";
                gtk_button_set_label(GTK_BUTTON(r->button), display.c_str());
            }
            return true;
        }
        return false;
    }

    bool handle_key_release() {
        for (auto* r : rows) {
            if (r->capturing && !r->keys_pressed.empty()) {
                // Save the combo
                std::string combo;
                for (const auto& k : r->keys_pressed) {
                    if (!combo.empty()) combo += "+";
                    combo += k;
                }
                r->hotkey = combo;
                r->capturing = false;
                r->keys_pressed.clear();
                hotkey_row_update_label(r);
                return true;
            }
        }
        return false;
    }

    bool is_capturing() const {
        for (const auto* r : rows) {
            if (r->capturing) return true;
        }
        return false;
    }
};

static HotkeyGroupData* create_hotkey_group(const std::string& title,
                                              const std::vector<std::string>& hotkeys,
                                              const std::string& default_hk) {
    auto* group = new HotkeyGroupData();
    group->default_hotkey = default_hk;

    group->frame = gtk_frame_new(("  " + title + "  ").c_str());
    gtk_frame_set_shadow_type(GTK_FRAME(group->frame), GTK_SHADOW_ETCHED_IN);

    GtkWidget* vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_widget_set_margin_start(vbox, 12);
    gtk_widget_set_margin_end(vbox, 12);
    gtk_widget_set_margin_top(vbox, 8);
    gtk_widget_set_margin_bottom(vbox, 8);
    gtk_container_add(GTK_CONTAINER(group->frame), vbox);

    group->rows_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_box_pack_start(GTK_BOX(vbox), group->rows_box, FALSE, FALSE, 0);

    auto hks = hotkeys.empty() ? std::vector<std::string>{default_hk} : hotkeys;
    for (const auto& hk : hks) {
        group->add_row(hk);
    }

    // Button row
    GtkWidget* btn_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_halign(btn_box, GTK_ALIGN_CENTER);

    GtkWidget* add_btn = gtk_button_new_with_label("+ Add");
    g_signal_connect(add_btn, "clicked",
        G_CALLBACK(+[](GtkButton*, gpointer data) {
            auto* g = static_cast<HotkeyGroupData*>(data);
            g->add_row("");
        }), group);
    gtk_box_pack_start(GTK_BOX(btn_box), add_btn, FALSE, FALSE, 0);

    GtkWidget* reset_btn = gtk_button_new_with_label("Reset");
    g_signal_connect(reset_btn, "clicked",
        G_CALLBACK(+[](GtkButton*, gpointer data) {
            auto* g = static_cast<HotkeyGroupData*>(data);
            // Remove all rows
            for (auto* r : g->rows) {
                gtk_container_remove(GTK_CONTAINER(g->rows_box), r->box);
                delete r;
            }
            g->rows.clear();
            g->add_row(g->default_hotkey);
        }), group);
    gtk_box_pack_start(GTK_BOX(btn_box), reset_btn, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(vbox), btn_box, FALSE, FALSE, 4);

    return group;
}

// ========== SettingsDialog ==========
struct SettingsDialogData {
    // Audio
    GtkWidget* mic_combo;
    GtkWidget* spk_combo;
    GtkWidget* mute_check;
    GtkWidget* vad_check;
    GtkWidget* vad_scale;
    GtkWidget* silence_spin;
    GtkWidget* max_dur_spin;

    // Hotkeys
    HotkeyGroupData* trigger_group;
    HotkeyGroupData* cancel_group;
    GtkWidget* mode_combo;
    GtkWidget* escape_check;

    // Model
    GtkWidget* model_size_combo;
    GtkWidget* device_combo;
    GtkWidget* compute_combo;
    GtkWidget* beam_spin;
    GtkWidget* language_combo;
    GtkWidget* threads_spin;

    // Output
    GtkWidget* method_combo;
    GtkWidget* also_copy_check;
    GtkWidget* auto_paste_check;
    GtkWidget* paste_delay_spin;
    GtkWidget* ending_combo;
    GtkWidget* lowercase_check;

    // Feedback
    GtkWidget* feedback_check;
    GtkWidget* volume_scale;
    GtkWidget* freq_start_spin;
    GtkWidget* freq_stop_spin;
    GtkWidget* freq_error_spin;
    GtkWidget* feedback_dur_spin;
};

static GtkWidget* build_combo(const std::vector<std::pair<std::string, std::string>>& items,
                               const std::string& active_id) {
    GtkWidget* combo = gtk_combo_box_text_new();
    int active_idx = 0;
    for (size_t i = 0; i < items.size(); i++) {
        gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combo), items[i].first.c_str(), items[i].second.c_str());
        if (items[i].first == active_id) active_idx = static_cast<int>(i);
    }
    gtk_combo_box_set_active(GTK_COMBO_BOX(combo), active_idx);
    return combo;
}

static GtkWidget* build_labeled_row(const std::string& label, GtkWidget* widget, int label_width = 90) {
    GtkWidget* row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget* lbl = gtk_label_new(label.c_str());
    gtk_label_set_xalign(GTK_LABEL(lbl), 0);
    gtk_widget_set_size_request(lbl, label_width, -1);
    gtk_box_pack_start(GTK_BOX(row), lbl, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(row), widget, TRUE, TRUE, 0);
    return row;
}

static std::string truncate_name(const std::string& name, size_t max_len) {
    if (name.size() <= max_len) return name;
    return name.substr(0, max_len - 3) + "...";
}

static GtkWidget* build_audio_section(SettingsDialogData* data, const Config& config) {
    GtkWidget* frame = gtk_frame_new("  Audio  ");
    gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_ETCHED_IN);

    GtkWidget* vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_set_margin_start(vbox, 12);
    gtk_widget_set_margin_end(vbox, 12);
    gtk_widget_set_margin_top(vbox, 8);
    gtk_widget_set_margin_bottom(vbox, 8);
    gtk_container_add(GTK_CONTAINER(frame), vbox);

    // Microphone
    data->mic_combo = gtk_combo_box_text_new();
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(data->mic_combo), "default", "System Default");

    auto devices = AudioManager::list_devices();
    int active_input = 0;
    for (size_t i = 0; i < devices.inputs.size(); i++) {
        std::string idx = std::to_string(devices.inputs[i].index);
        gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(data->mic_combo),
            idx.c_str(), truncate_name(devices.inputs[i].name, 35).c_str());
        if (config.audio.device && (*config.audio.device == idx || *config.audio.device == devices.inputs[i].name)) {
            active_input = static_cast<int>(i) + 1;
        }
    }
    gtk_combo_box_set_active(GTK_COMBO_BOX(data->mic_combo), active_input);
    gtk_box_pack_start(GTK_BOX(vbox), build_labeled_row("Microphone:", data->mic_combo), FALSE, FALSE, 0);

    // Speaker
    data->spk_combo = gtk_combo_box_text_new();
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(data->spk_combo), "default", "System Default");
    int active_output = 0;
    for (size_t i = 0; i < devices.outputs.size(); i++) {
        std::string idx = std::to_string(devices.outputs[i].index);
        gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(data->spk_combo),
            idx.c_str(), truncate_name(devices.outputs[i].name, 35).c_str());
        if (config.audio.output_device && (*config.audio.output_device == idx || *config.audio.output_device == devices.outputs[i].name)) {
            active_output = static_cast<int>(i) + 1;
        }
    }
    gtk_combo_box_set_active(GTK_COMBO_BOX(data->spk_combo), active_output);
    gtk_box_pack_start(GTK_BOX(vbox), build_labeled_row("Speaker:", data->spk_combo), FALSE, FALSE, 0);

    // Mute other apps
    data->mute_check = gtk_check_button_new_with_label("Mute other apps");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(data->mute_check), config.audio.mute_other_apps);
    gtk_box_pack_start(GTK_BOX(vbox), data->mute_check, FALSE, FALSE, 0);

    // Advanced expander
    GtkWidget* expander = gtk_expander_new("Advanced");
    GtkWidget* adv_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_widget_set_margin_start(adv_box, 8);
    gtk_widget_set_margin_top(adv_box, 8);
    gtk_container_add(GTK_CONTAINER(expander), adv_box);

    data->vad_check = gtk_check_button_new_with_label("Voice Activity Detection");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(data->vad_check), config.audio.vad_enabled);
    gtk_box_pack_start(GTK_BOX(adv_box), data->vad_check, FALSE, FALSE, 0);

    data->vad_scale = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0.0, 1.0, 0.1);
    gtk_range_set_value(GTK_RANGE(data->vad_scale), config.audio.vad_threshold);
    gtk_scale_set_digits(GTK_SCALE(data->vad_scale), 1);
    gtk_box_pack_start(GTK_BOX(adv_box), build_labeled_row("VAD threshold:", data->vad_scale, 110), FALSE, FALSE, 0);

    data->silence_spin = gtk_spin_button_new_with_range(0.1, 2.0, 0.1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(data->silence_spin), config.audio.silence_duration);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(data->silence_spin), 1);
    gtk_box_pack_start(GTK_BOX(adv_box), build_labeled_row("Silence trim (s):", data->silence_spin, 110), FALSE, FALSE, 0);

    data->max_dur_spin = gtk_spin_button_new_with_range(10, 600, 10);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(data->max_dur_spin), config.audio.max_duration);
    gtk_box_pack_start(GTK_BOX(adv_box), build_labeled_row("Max duration (s):", data->max_dur_spin, 110), FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(vbox), expander, FALSE, FALSE, 0);
    return frame;
}

static GtkWidget* build_hotkeys_section(SettingsDialogData* data, const Config& config) {
    GtkWidget* frame = gtk_frame_new("  Hotkeys  ");
    gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_ETCHED_IN);

    GtkWidget* vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_set_margin_start(vbox, 12);
    gtk_widget_set_margin_end(vbox, 12);
    gtk_widget_set_margin_top(vbox, 8);
    gtk_widget_set_margin_bottom(vbox, 8);
    gtk_container_add(GTK_CONTAINER(frame), vbox);

    data->trigger_group = create_hotkey_group("Recording (hold to speak)", config.hotkeys.trigger, "shift+super");
    gtk_box_pack_start(GTK_BOX(vbox), data->trigger_group->frame, FALSE, FALSE, 0);

    data->cancel_group = create_hotkey_group("Cancel Recording", config.hotkeys.cancel, "esc");
    gtk_box_pack_start(GTK_BOX(vbox), data->cancel_group->frame, FALSE, FALSE, 0);

    GtkWidget* hint = gtk_label_new(nullptr);
    gtk_label_set_markup(GTK_LABEL(hint), "<small>Click button, press keys. Backspace clears.</small>");
    gtk_widget_set_opacity(hint, 0.6);
    gtk_box_pack_start(GTK_BOX(vbox), hint, FALSE, FALSE, 0);

    // Advanced
    GtkWidget* expander = gtk_expander_new("Advanced");
    GtkWidget* adv_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_widget_set_margin_start(adv_box, 8);
    gtk_widget_set_margin_top(adv_box, 8);
    gtk_container_add(GTK_CONTAINER(expander), adv_box);

    data->mode_combo = gtk_combo_box_text_new();
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(data->mode_combo), "push_to_talk", "Push to Talk (hold)");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(data->mode_combo), "toggle", "Toggle (press twice)");
    gtk_combo_box_set_active(GTK_COMBO_BOX(data->mode_combo), config.hotkeys.mode == "push_to_talk" ? 0 : 1);
    gtk_box_pack_start(GTK_BOX(adv_box), build_labeled_row("Mode:", data->mode_combo, 110), FALSE, FALSE, 0);

    data->escape_check = gtk_check_button_new_with_label("Escape cancels recording");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(data->escape_check), config.hotkeys.escape_to_cancel);
    gtk_box_pack_start(GTK_BOX(adv_box), data->escape_check, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(vbox), expander, FALSE, FALSE, 0);
    return frame;
}

static GtkWidget* build_model_section(SettingsDialogData* data, const Config& config) {
    GtkWidget* frame = gtk_frame_new("  Model  ");
    gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_ETCHED_IN);

    GtkWidget* vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_set_margin_start(vbox, 12);
    gtk_widget_set_margin_end(vbox, 12);
    gtk_widget_set_margin_top(vbox, 8);
    gtk_widget_set_margin_bottom(vbox, 8);
    gtk_container_add(GTK_CONTAINER(frame), vbox);

    data->model_size_combo = build_combo(MODEL_SIZES, config.model.size);
    gtk_box_pack_start(GTK_BOX(vbox), build_labeled_row("Size:", data->model_size_combo), FALSE, FALSE, 0);

    // Advanced
    GtkWidget* expander = gtk_expander_new("Advanced");
    GtkWidget* adv_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_widget_set_margin_start(adv_box, 8);
    gtk_widget_set_margin_top(adv_box, 8);
    gtk_container_add(GTK_CONTAINER(expander), adv_box);

    data->device_combo = build_combo(DEVICES, config.model.device);
    gtk_box_pack_start(GTK_BOX(adv_box), build_labeled_row("Device:", data->device_combo, 100), FALSE, FALSE, 0);

    data->compute_combo = build_combo(COMPUTE_TYPES, config.model.compute_type);
    gtk_box_pack_start(GTK_BOX(adv_box), build_labeled_row("Compute:", data->compute_combo, 100), FALSE, FALSE, 0);

    data->beam_spin = gtk_spin_button_new_with_range(1, 5, 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(data->beam_spin), config.model.beam_size);
    gtk_box_pack_start(GTK_BOX(adv_box), build_labeled_row("Beam size:", data->beam_spin, 100), FALSE, FALSE, 0);

    data->language_combo = build_combo(LANGUAGES, config.model.language);
    gtk_box_pack_start(GTK_BOX(adv_box), build_labeled_row("Language:", data->language_combo, 100), FALSE, FALSE, 0);

    data->threads_spin = gtk_spin_button_new_with_range(1, 16, 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(data->threads_spin), config.model.num_threads);
    gtk_box_pack_start(GTK_BOX(adv_box), build_labeled_row("CPU threads:", data->threads_spin, 100), FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(vbox), expander, FALSE, FALSE, 0);
    return frame;
}

static GtkWidget* build_output_section(SettingsDialogData* data, const Config& config) {
    GtkWidget* frame = gtk_frame_new("  Output  ");
    gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_ETCHED_IN);

    GtkWidget* vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_set_margin_start(vbox, 12);
    gtk_widget_set_margin_end(vbox, 12);
    gtk_widget_set_margin_top(vbox, 8);
    gtk_widget_set_margin_bottom(vbox, 8);
    gtk_container_add(GTK_CONTAINER(frame), vbox);

    std::vector<std::pair<std::string, std::string>> methods = {
        {"inject", "Type text (xdotool)"},
        {"clipboard", "Copy to clipboard"},
    };
    data->method_combo = build_combo(methods, config.output.method);
    gtk_box_pack_start(GTK_BOX(vbox), build_labeled_row("Method:", data->method_combo), FALSE, FALSE, 0);

    // Advanced
    GtkWidget* expander = gtk_expander_new("Advanced");
    GtkWidget* adv_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_widget_set_margin_start(adv_box, 8);
    gtk_widget_set_margin_top(adv_box, 8);
    gtk_container_add(GTK_CONTAINER(expander), adv_box);

    data->also_copy_check = gtk_check_button_new_with_label("Also copy to clipboard");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(data->also_copy_check), config.output.also_copy_to_clipboard);
    gtk_box_pack_start(GTK_BOX(adv_box), data->also_copy_check, FALSE, FALSE, 0);

    data->auto_paste_check = gtk_check_button_new_with_label("Auto-paste after copy");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(data->auto_paste_check), config.output.auto_paste);
    gtk_box_pack_start(GTK_BOX(adv_box), data->auto_paste_check, FALSE, FALSE, 0);

    data->paste_delay_spin = gtk_spin_button_new_with_range(0.01, 0.5, 0.01);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(data->paste_delay_spin), config.output.paste_delay);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(data->paste_delay_spin), 2);
    gtk_box_pack_start(GTK_BOX(adv_box), build_labeled_row("Paste delay (s):", data->paste_delay_spin, 110), FALSE, FALSE, 0);

    data->ending_combo = build_combo(ENDING_ACTIONS, config.output.ending_action);
    gtk_box_pack_start(GTK_BOX(adv_box), build_labeled_row("Ending action:", data->ending_combo, 110), FALSE, FALSE, 0);

    data->lowercase_check = gtk_check_button_new_with_label("Convert to lowercase");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(data->lowercase_check), config.output.lowercase);
    gtk_box_pack_start(GTK_BOX(adv_box), data->lowercase_check, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(vbox), expander, FALSE, FALSE, 0);
    return frame;
}

static GtkWidget* build_feedback_section(SettingsDialogData* data, const Config& config) {
    GtkWidget* frame = gtk_frame_new("  Feedback  ");
    gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_ETCHED_IN);

    GtkWidget* vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_set_margin_start(vbox, 12);
    gtk_widget_set_margin_end(vbox, 12);
    gtk_widget_set_margin_top(vbox, 8);
    gtk_widget_set_margin_bottom(vbox, 8);
    gtk_container_add(GTK_CONTAINER(frame), vbox);

    // Primary row: enabled + volume
    GtkWidget* primary_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);

    data->feedback_check = gtk_check_button_new_with_label("Enabled");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(data->feedback_check), config.feedback.enabled);
    gtk_box_pack_start(GTK_BOX(primary_row), data->feedback_check, FALSE, FALSE, 0);

    GtkWidget* vol_label = gtk_label_new("Volume:");
    gtk_label_set_xalign(GTK_LABEL(vol_label), 0);
    gtk_box_pack_start(GTK_BOX(primary_row), vol_label, FALSE, FALSE, 0);

    data->volume_scale = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0.0, 1.0, 0.1);
    gtk_range_set_value(GTK_RANGE(data->volume_scale), config.feedback.volume);
    gtk_scale_set_digits(GTK_SCALE(data->volume_scale), 1);
    gtk_widget_set_size_request(data->volume_scale, 120, -1);
    gtk_box_pack_start(GTK_BOX(primary_row), data->volume_scale, TRUE, TRUE, 0);

    gtk_box_pack_start(GTK_BOX(vbox), primary_row, FALSE, FALSE, 0);

    // Advanced
    GtkWidget* expander = gtk_expander_new("Advanced");
    GtkWidget* adv_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_widget_set_margin_start(adv_box, 8);
    gtk_widget_set_margin_top(adv_box, 8);
    gtk_container_add(GTK_CONTAINER(expander), adv_box);

    data->freq_start_spin = gtk_spin_button_new_with_range(200, 2000, 50);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(data->freq_start_spin), config.feedback.frequency_start);
    gtk_box_pack_start(GTK_BOX(adv_box), build_labeled_row("Start freq (Hz):", data->freq_start_spin, 110), FALSE, FALSE, 0);

    data->freq_stop_spin = gtk_spin_button_new_with_range(200, 2000, 50);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(data->freq_stop_spin), config.feedback.frequency_stop);
    gtk_box_pack_start(GTK_BOX(adv_box), build_labeled_row("Stop freq (Hz):", data->freq_stop_spin, 110), FALSE, FALSE, 0);

    data->freq_error_spin = gtk_spin_button_new_with_range(200, 2000, 50);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(data->freq_error_spin), config.feedback.frequency_error);
    gtk_box_pack_start(GTK_BOX(adv_box), build_labeled_row("Error freq (Hz):", data->freq_error_spin, 110), FALSE, FALSE, 0);

    data->feedback_dur_spin = gtk_spin_button_new_with_range(0.05, 0.5, 0.05);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(data->feedback_dur_spin), config.feedback.duration);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(data->feedback_dur_spin), 2);
    gtk_box_pack_start(GTK_BOX(adv_box), build_labeled_row("Duration (s):", data->feedback_dur_spin, 110), FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(vbox), expander, FALSE, FALSE, 0);
    return frame;
}

static std::string get_combo_active_id(GtkWidget* combo) {
    const gchar* id = gtk_combo_box_get_active_id(GTK_COMBO_BOX(combo));
    return id ? id : "";
}

static void save_settings(const SettingsDialogData* data, const std::string& config_path) {
    if (config_path.empty()) return;

    try {
        Config cfg;
        try {
            cfg = Config::load(config_path);
        } catch (...) {
            cfg = Config::default_config();
        }

        // Model
        cfg.model.size = get_combo_active_id(data->model_size_combo);
        cfg.model.device = get_combo_active_id(data->device_combo);
        cfg.model.compute_type = get_combo_active_id(data->compute_combo);
        cfg.model.beam_size = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(data->beam_spin));
        cfg.model.language = get_combo_active_id(data->language_combo);
        cfg.model.num_threads = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(data->threads_spin));

        // Audio
        std::string mic_id = get_combo_active_id(data->mic_combo);
        cfg.audio.device = (mic_id == "default") ? std::nullopt : std::optional<std::string>(mic_id);
        std::string spk_id = get_combo_active_id(data->spk_combo);
        cfg.audio.output_device = (spk_id == "default") ? std::nullopt : std::optional<std::string>(spk_id);
        cfg.audio.vad_enabled = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(data->vad_check));
        cfg.audio.vad_threshold = static_cast<float>(gtk_range_get_value(GTK_RANGE(data->vad_scale)));
        cfg.audio.silence_duration = static_cast<float>(gtk_spin_button_get_value(GTK_SPIN_BUTTON(data->silence_spin)));
        cfg.audio.max_duration = static_cast<float>(gtk_spin_button_get_value(GTK_SPIN_BUTTON(data->max_dur_spin)));
        cfg.audio.mute_other_apps = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(data->mute_check));

        // Hotkeys
        cfg.hotkeys.trigger = data->trigger_group->get_hotkeys();
        cfg.hotkeys.cancel = data->cancel_group->get_hotkeys();
        cfg.hotkeys.mode = get_combo_active_id(data->mode_combo);
        cfg.hotkeys.escape_to_cancel = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(data->escape_check));

        // Output
        cfg.output.method = get_combo_active_id(data->method_combo);
        cfg.output.also_copy_to_clipboard = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(data->also_copy_check));
        cfg.output.auto_paste = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(data->auto_paste_check));
        cfg.output.paste_delay = static_cast<float>(gtk_spin_button_get_value(GTK_SPIN_BUTTON(data->paste_delay_spin)));
        cfg.output.ending_action = get_combo_active_id(data->ending_combo);
        cfg.output.lowercase = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(data->lowercase_check));

        // Feedback
        cfg.feedback.enabled = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(data->feedback_check));
        cfg.feedback.volume = static_cast<float>(gtk_range_get_value(GTK_RANGE(data->volume_scale)));
        cfg.feedback.frequency_start = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(data->freq_start_spin));
        cfg.feedback.frequency_stop = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(data->freq_stop_spin));
        cfg.feedback.frequency_error = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(data->freq_error_spin));
        cfg.feedback.duration = static_cast<float>(gtk_spin_button_get_value(GTK_SPIN_BUTTON(data->feedback_dur_spin)));

        cfg.save(config_path);
        spdlog::info("Saved settings to {}", config_path);
    } catch (const std::exception& e) {
        spdlog::error("Failed to save settings: {}", e.what());
    }
}

void show_settings_dialog(TrayManager* mgr, const Config& config,
                           const std::string& config_path,
                           std::function<void()> on_open,
                           std::function<void()> on_close) {
    spdlog::info("Settings opened - pausing daemon");
    if (on_open) on_open();

    auto* data = new SettingsDialogData();

    GtkWidget* dialog = gtk_dialog_new_with_buttons(
        "AutoWhisper Settings", nullptr,
        static_cast<GtkDialogFlags>(0),  // not modal — no parent to be modal relative to
        "Cancel", GTK_RESPONSE_CANCEL,
        "Save", GTK_RESPONSE_OK,
        nullptr);
    gtk_window_set_default_size(GTK_WINDOW(dialog), 450, -1);
    gtk_window_set_resizable(GTK_WINDOW(dialog), TRUE);
    gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER);
    gtk_window_set_type_hint(GTK_WINDOW(dialog), GDK_WINDOW_TYPE_HINT_NORMAL);
    gtk_window_set_skip_taskbar_hint(GTK_WINDOW(dialog), FALSE);

    GtkWidget* content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));

    GtkWidget* notebook = gtk_notebook_new();
    gtk_widget_set_margin_start(notebook, 8);
    gtk_widget_set_margin_end(notebook, 8);
    gtk_widget_set_margin_top(notebook, 8);
    gtk_widget_set_margin_bottom(notebook, 8);
    gtk_box_pack_start(GTK_BOX(content), notebook, TRUE, TRUE, 0);

    // General tab
    GtkWidget* general_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_set_margin_start(general_box, 12);
    gtk_widget_set_margin_end(general_box, 12);
    gtk_widget_set_margin_top(general_box, 12);
    gtk_widget_set_margin_bottom(general_box, 8);
    gtk_box_pack_start(GTK_BOX(general_box), build_audio_section(data, config), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(general_box), build_hotkeys_section(data, config), FALSE, FALSE, 0);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), general_box, gtk_label_new("General"));

    // Advanced tab
    GtkWidget* advanced_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_set_margin_start(advanced_box, 12);
    gtk_widget_set_margin_end(advanced_box, 12);
    gtk_widget_set_margin_top(advanced_box, 12);
    gtk_widget_set_margin_bottom(advanced_box, 8);
    gtk_box_pack_start(GTK_BOX(advanced_box), build_model_section(data, config), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(advanced_box), build_output_section(data, config), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(advanced_box), build_feedback_section(data, config), FALSE, FALSE, 0);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), advanced_box, gtk_label_new("Advanced"));

    // Key capture events
    g_signal_connect(dialog, "key-press-event",
        G_CALLBACK(+[](GtkWidget*, GdkEventKey* event, gpointer user_data) -> gboolean {
            auto* d = static_cast<SettingsDialogData*>(user_data);
            const gchar* keyname = gdk_keyval_name(event->keyval);
            if (!keyname) return FALSE;
            if (d->trigger_group->handle_key_press(keyname)) return TRUE;
            if (d->cancel_group->handle_key_press(keyname)) return TRUE;
            return FALSE;
        }), data);

    g_signal_connect(dialog, "key-release-event",
        G_CALLBACK(+[](GtkWidget*, GdkEventKey*, gpointer user_data) -> gboolean {
            auto* d = static_cast<SettingsDialogData*>(user_data);
            if (d->trigger_group->handle_key_release()) return TRUE;
            if (d->cancel_group->handle_key_release()) return TRUE;
            return FALSE;
        }), data);

    gtk_widget_show_all(dialog);
    gtk_window_present(GTK_WINDOW(dialog));
    int response = gtk_dialog_run(GTK_DIALOG(dialog));

    if (response == GTK_RESPONSE_OK) {
        // Check for model changes
        std::string new_size = get_combo_active_id(data->model_size_combo);
        std::string new_device = get_combo_active_id(data->device_combo);
        std::string new_compute = get_combo_active_id(data->compute_combo);

        bool model_changed = (new_size != config.model.size ||
                               new_device != config.model.device ||
                               new_compute != config.model.compute_type);

        save_settings(data, config_path);

        if (model_changed) {
            spdlog::info("Model settings changed - restart required");
        }
    }

    gtk_widget_destroy(dialog);

    // Clean up hotkey groups
    for (auto* r : data->trigger_group->rows) delete r;
    delete data->trigger_group;
    for (auto* r : data->cancel_group->rows) delete r;
    delete data->cancel_group;
    delete data;

    spdlog::info("Settings closed - resuming daemon");
    if (on_close) on_close();
}

} // namespace autowhisper
