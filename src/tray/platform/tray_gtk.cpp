#include "tray/tray.h"

#include <spdlog/spdlog.h>

#include <climits>
#include <filesystem>
#include <thread>
#include <unistd.h>

#include <gtk/gtk.h>

#ifdef HAVE_AYATANA_APPINDICATOR
#include <libayatana-appindicator/app-indicator.h>
#elif defined(HAVE_APPINDICATOR)
#include <libappindicator/app-indicator.h>
#endif

namespace fs = std::filesystem;

namespace autowhisper {

static const char* ICON_NAMES[] = {
    "idle", "recording", "processing", "recording", "processing", "error"};
static const char* STATE_TITLES[] = {
    "AutoWhisper - Ready",
    "AutoWhisper - Dictate: recording...",
    "AutoWhisper - Dictate: transcribing...",
    "AutoWhisper - Ask Fabric: recording...",
    "AutoWhisper - Ask Fabric: thinking...",
    "AutoWhisper - Error",
};

struct TrayManager::Impl {
#if defined(HAVE_AYATANA_APPINDICATOR) || defined(HAVE_APPINDICATOR)
    AppIndicator* indicator = nullptr;
#endif
    GMainLoop* gtk_loop = nullptr;
    std::thread gtk_thread;
    GtkWidget* trigger_label = nullptr;
    GtkWidget* ask_trigger_label = nullptr;
    GtkWidget* cancel_label = nullptr;
    TrayManager* manager = nullptr;

    std::string get_icon_path(TrayState state) const {
        std::vector<std::string> search_paths;

        // Relative to the executable (handles build/ and installed locations)
        try {
            auto exe_dir = fs::read_symlink("/proc/self/exe").parent_path();
            search_paths.push_back((exe_dir / "icons").string());
            search_paths.push_back((exe_dir.parent_path() / "icons").string());
        } catch (...) {}

        // Standard install locations (relative to prefix: /usr or /usr/local)
        try {
            auto prefix = fs::read_symlink("/proc/self/exe").parent_path().parent_path();
            search_paths.push_back((prefix / "share" / "autowhisper" / "icons").string());
        } catch (...) {}
        search_paths.push_back("/usr/share/autowhisper/icons");

        // CWD-relative fallbacks
        search_paths.push_back("icons");
        search_paths.push_back("../icons");

        std::string name = ICON_NAMES[static_cast<int>(state)];

        for (const auto& dir : search_paths) {
            std::string svg = dir + "/" + name + ".svg";
            if (fs::exists(svg)) {
                spdlog::debug("Found icon: {}", svg);
                return fs::absolute(svg).string();
            }

            std::string png = dir + "/" + name + ".png";
            if (fs::exists(png)) {
                spdlog::debug("Found icon: {}", png);
                return fs::absolute(png).string();
            }
        }

        spdlog::warn("Icon '{}' not found, using fallback", name);
        return "audio-input-microphone";
    }
};

TrayManager::TrayManager(bool enabled, QuitCallback on_quit,
                          const std::string& config_path)
    : enabled_(enabled),
      on_quit_(std::move(on_quit)),
      config_path_(config_path),
      impl_(std::make_shared<Impl>()) {
    impl_->manager = this;

#if !defined(HAVE_AYATANA_APPINDICATOR) && !defined(HAVE_APPINDICATOR)
    if (enabled_) {
        spdlog::warn("Tray icon disabled: AppIndicator not available. "
                     "Install with: sudo apt install gir1.2-ayatanaappindicator3-0.1");
        enabled_ = false;
    }
#endif
}

TrayManager::~TrayManager() {
    stop();
}

void TrayManager::start() {
    if (!enabled_) return;

#if defined(HAVE_AYATANA_APPINDICATOR) || defined(HAVE_APPINDICATOR)
    const auto initial_trigger_hotkeys = trigger_hotkeys_;
    const auto initial_ask_hotkeys = ask_trigger_hotkeys_;
    const auto initial_cancel_hotkeys = cancel_hotkeys_;
    impl_->gtk_thread = std::thread([this, initial_trigger_hotkeys,
                                     initial_ask_hotkeys, initial_cancel_hotkeys]() {
        gtk_init(nullptr, nullptr);

        std::string icon_path = impl_->get_icon_path(TrayState::IDLE);

        impl_->indicator = app_indicator_new(
            "autowhisper", icon_path.c_str(),
            APP_INDICATOR_CATEGORY_APPLICATION_STATUS);
        app_indicator_set_status(impl_->indicator, APP_INDICATOR_STATUS_ACTIVE);
        app_indicator_set_title(impl_->indicator, STATE_TITLES[0]);

        // Create menu
        GtkWidget* menu = gtk_menu_new();

        // Version header
        GtkWidget* version_item = gtk_menu_item_new_with_label("AutoWhisper v" AUTOWHISPER_VERSION);
        gtk_widget_set_sensitive(version_item, FALSE);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), version_item);

        gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());

        // Hotkey info
        std::string trigger_text = "Dictate: " + format_hotkeys(initial_trigger_hotkeys);
        impl_->trigger_label = gtk_menu_item_new_with_label(trigger_text.c_str());
        gtk_widget_set_sensitive(impl_->trigger_label, FALSE);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), impl_->trigger_label);

        std::string ask_trigger_text = initial_ask_hotkeys.empty()
            ? "Ask Fabric: off"
            : "Ask Fabric: " + format_hotkeys(initial_ask_hotkeys);
        impl_->ask_trigger_label = gtk_menu_item_new_with_label(ask_trigger_text.c_str());
        gtk_widget_set_sensitive(impl_->ask_trigger_label, FALSE);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), impl_->ask_trigger_label);

        std::string cancel_text = "Cancel: " + format_hotkeys(initial_cancel_hotkeys);
        impl_->cancel_label = gtk_menu_item_new_with_label(cancel_text.c_str());
        gtk_widget_set_sensitive(impl_->cancel_label, FALSE);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), impl_->cancel_label);

        gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());

        // Open Config
        GtkWidget* config_item = gtk_menu_item_new_with_label("Open Config");
        auto open_config_cb = +[](GtkMenuItem*, gpointer data) {
            auto* mgr = static_cast<TrayManager*>(data);
            if (mgr->config_path_.empty()) {
                spdlog::warn("No config path known; cannot launch settings UI");
                return;
            }
            char exe_path[PATH_MAX];
            ssize_t n = ::readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
            if (n <= 0) {
                spdlog::warn("readlink(/proc/self/exe) failed; cannot launch settings UI");
                return;
            }
            exe_path[n] = '\0';

            gchar* argv[6];
            argv[0] = exe_path;
            argv[1] = (gchar*)"config";
            argv[2] = (gchar*)"ui";
            argv[3] = (gchar*)"--config";
            argv[4] = (gchar*)mgr->config_path_.c_str();
            argv[5] = nullptr;

            GError* err = nullptr;
            GSpawnFlags flags = (GSpawnFlags)(G_SPAWN_STDOUT_TO_DEV_NULL | G_SPAWN_STDERR_TO_DEV_NULL);
            gboolean ok = g_spawn_async(
                nullptr, argv, nullptr, flags,
                nullptr, nullptr, nullptr, &err);
            if (!ok) {
                spdlog::warn("g_spawn_async failed: {}", err ? err->message : "unknown");
                if (err) g_error_free(err);
            }
        };
        g_signal_connect(config_item, "activate", G_CALLBACK(open_config_cb), this);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), config_item);

        // Quit
        GtkWidget* quit_item = gtk_menu_item_new_with_label("Quit");
        g_signal_connect(quit_item, "activate",
            G_CALLBACK(+[](GtkMenuItem*, gpointer data) {
                auto* mgr = static_cast<TrayManager*>(data);
                spdlog::info("Quit requested from tray menu");
                if (mgr->on_quit_) mgr->on_quit_();
                mgr->stop();
            }), this);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), quit_item);

        gtk_widget_show_all(menu);
        app_indicator_set_menu(impl_->indicator, GTK_MENU(menu));

        impl_->gtk_loop = g_main_loop_new(nullptr, FALSE);
        g_main_loop_run(impl_->gtk_loop);
    });

    spdlog::info("Tray icon started");
#endif
}

void TrayManager::stop() {
    if (impl_->gtk_loop) {
        g_main_loop_quit(impl_->gtk_loop);
        impl_->gtk_loop = nullptr;
    }

    if (impl_->gtk_thread.joinable()) {
        impl_->gtk_thread.join();
    }

    spdlog::info("Tray icon stopped");
}

void TrayManager::set_state(TrayState state) {
    if (!enabled_) return;
    if (state == state_) return;

    state_ = state;

#if defined(HAVE_AYATANA_APPINDICATOR) || defined(HAVE_APPINDICATOR)
    if (impl_->indicator) {
        struct UpdateData {
            AppIndicator* indicator;
            std::string icon_path;
            std::string title;
        };

        auto* data = new UpdateData{
            impl_->indicator,
            impl_->get_icon_path(state),
            STATE_TITLES[static_cast<int>(state)]
        };

        g_idle_add(+[](gpointer ptr) -> gboolean {
            auto* d = static_cast<UpdateData*>(ptr);
            app_indicator_set_icon_full(d->indicator, d->icon_path.c_str(), d->title.c_str());
            app_indicator_set_title(d->indicator, d->title.c_str());
            delete d;
            return FALSE;
        }, data);
    }
#endif

    spdlog::debug("Tray state changed to {}", static_cast<int>(state));
}

void TrayManager::set_input_device(const std::string& name) {
    input_device_ = name;
}

void TrayManager::set_output_device(const std::string& name) {
    output_device_ = name;
}

void TrayManager::set_hotkey(const std::vector<std::string>& hotkeys) {
    trigger_hotkeys_ = hotkeys;
#if defined(HAVE_AYATANA_APPINDICATOR) || defined(HAVE_APPINDICATOR)
    struct LabelUpdate { std::shared_ptr<Impl> impl; std::string label; };
    auto* data = new LabelUpdate{impl_, "Dictate: " + format_hotkeys(hotkeys)};
    g_idle_add(+[](gpointer ptr) -> gboolean {
        auto* d = static_cast<LabelUpdate*>(ptr);
        if (d->impl->trigger_label) {
            gtk_menu_item_set_label(GTK_MENU_ITEM(d->impl->trigger_label), d->label.c_str());
        }
        delete d;
        return FALSE;
    }, data);
#endif
}

void TrayManager::set_ask_hotkey(const std::vector<std::string>& hotkeys) {
    ask_trigger_hotkeys_ = hotkeys;
#if defined(HAVE_AYATANA_APPINDICATOR) || defined(HAVE_APPINDICATOR)
    struct LabelUpdate { std::shared_ptr<Impl> impl; std::string label; };
    auto* data = new LabelUpdate{
        impl_, hotkeys.empty() ? "Ask Fabric: off"
                               : "Ask Fabric: " + format_hotkeys(hotkeys)};
    g_idle_add(+[](gpointer ptr) -> gboolean {
        auto* d = static_cast<LabelUpdate*>(ptr);
        if (d->impl->ask_trigger_label) {
            gtk_menu_item_set_label(GTK_MENU_ITEM(d->impl->ask_trigger_label), d->label.c_str());
        }
        delete d;
        return FALSE;
    }, data);
#endif
}

void TrayManager::set_cancel_hotkey(const std::vector<std::string>& hotkeys) {
    cancel_hotkeys_ = hotkeys;
#if defined(HAVE_AYATANA_APPINDICATOR) || defined(HAVE_APPINDICATOR)
    struct LabelUpdate { std::shared_ptr<Impl> impl; std::string label; };
    auto* data = new LabelUpdate{impl_, "Cancel: " + format_hotkeys(hotkeys)};
    g_idle_add(+[](gpointer ptr) -> gboolean {
        auto* d = static_cast<LabelUpdate*>(ptr);
        if (d->impl->cancel_label) {
            gtk_menu_item_set_label(GTK_MENU_ITEM(d->impl->cancel_label), d->label.c_str());
        }
        delete d;
        return FALSE;
    }, data);
#endif
}

} // namespace autowhisper
