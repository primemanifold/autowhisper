#include "tray/tray.h"

#include <spdlog/spdlog.h>

#import <AppKit/AppKit.h>

#include <mach-o/dyld.h>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

// Obj-C target/action shim that bridges back to the C++ quit callback.
@interface AWTrayTarget : NSObject
@property (nonatomic, copy) void (^onQuit)(void);
@property (nonatomic, copy) void (^onOpenSettings)(void);
@property (nonatomic, copy) void (^onOpenLogs)(void);
- (void)quitClicked:(id)sender;
- (void)openSettingsClicked:(id)sender;
- (void)openLogsClicked:(id)sender;
@end

@implementation AWTrayTarget
- (void)quitClicked:(id)sender          { if (self.onQuit)          self.onQuit();          }
- (void)openSettingsClicked:(id)sender  { if (self.onOpenSettings)  self.onOpenSettings();  }
- (void)openLogsClicked:(id)sender      { if (self.onOpenLogs)      self.onOpenLogs();      }
@end

namespace autowhisper {

namespace {

// Render one unicode character with an underlying mic icon style.
// We use SF Symbols on macOS 11+ — "mic" variants. Fall back to text if
// the symbol API is unavailable.
NSImage* mic_image_for_state(TrayState state) API_AVAILABLE(macos(11.0)) {
    NSString* symbol = @"mic";
    switch (state) {
        case TrayState::IDLE:       symbol = @"mic";                break;
        case TrayState::RECORDING:  symbol = @"mic.fill";           break;
        case TrayState::PROCESSING: symbol = @"waveform";           break;
        case TrayState::ERROR:      symbol = @"exclamationmark.triangle"; break;
    }
    NSImage* img = [NSImage imageWithSystemSymbolName:symbol
                           accessibilityDescription:@"AutoWhisper"];
    if (img) {
        [img setTemplate:YES];  // Auto-adapt to dark/light menu bar
    }
    return img;
}

std::string state_label(TrayState s) {
    switch (s) {
        case TrayState::IDLE:       return "AutoWhisper \u2014 idle";
        case TrayState::RECORDING:  return "Recording\u2026";
        case TrayState::PROCESSING: return "Transcribing\u2026";
        case TrayState::ERROR:      return "Error \u2014 click to view";
    }
    return "AutoWhisper";
}

} // namespace

// Obj-C object pointers in a C++ struct default to __unsafe_unretained under
// ARC. Mark each as __strong so the struct actually owns the reference.
struct TrayManager::Impl {
    __strong NSStatusItem* item = nil;
    __strong AWTrayTarget* target = nil;
    __strong NSMenuItem* header_item = nil;
    __strong NSMenuItem* input_item = nil;
    __strong NSMenuItem* output_item = nil;
    __strong NSMenuItem* hotkey_item = nil;
    __strong NSMenuItem* cancel_item = nil;
    std::string config_path;
};

TrayManager::TrayManager(bool enabled,
                          QuitCallback on_quit,
                          const std::string& config_path)
    : enabled_(enabled),
      on_quit_(std::move(on_quit)),
      config_path_(config_path),
      impl_(std::make_shared<Impl>()) {
    impl_->config_path = config_path;
}

TrayManager::~TrayManager() { stop(); }

void TrayManager::start() {
    if (!enabled_) return;

    // Capture a shared_ptr<Impl> into each block so it stays alive across
    // the dispatch_async boundary even if ~TrayManager() runs first.
    auto impl = impl_;
    auto quit_cb = on_quit_;
    auto config_path = impl_->config_path;

    dispatch_async(dispatch_get_main_queue(), ^{
        NSStatusBar* bar = [NSStatusBar systemStatusBar];
        impl->item = [bar statusItemWithLength:NSVariableStatusItemLength];
        impl->item.button.accessibilityLabel = @"AutoWhisper";

        if (@available(macOS 11.0, *)) {
            impl->item.button.image = mic_image_for_state(TrayState::IDLE);
        } else {
            impl->item.button.title = @"AW";
        }

        impl->target = [[AWTrayTarget alloc] init];
        impl->target.onQuit = ^{
            spdlog::info("Tray: quit requested");
            if (quit_cb) quit_cb();
        };
        impl->target.onOpenSettings = ^{
            // Launch the settings UI via the same binary.
            NSString* exe = [[NSBundle mainBundle] executablePath];
            if (!exe) {
                char buf[1024]; uint32_t sz = sizeof(buf);
                if (_NSGetExecutablePath(buf, &sz) == 0) {
                    exe = [NSString stringWithUTF8String:buf];
                }
            }
            if (exe) {
                NSTask* task = [[NSTask alloc] init];
                task.executableURL = [NSURL fileURLWithPath:exe];
                NSMutableArray* args = [NSMutableArray arrayWithObjects:@"config", @"ui", nil];
                if (!config_path.empty()) {
                    [args addObject:@"-c"];
                    [args addObject:[NSString stringWithUTF8String:config_path.c_str()]];
                }
                task.arguments = args;
                NSError* err = nil;
                [task launchAndReturnError:&err];
                if (err) spdlog::warn("Tray: failed to launch settings UI: {}",
                                      err.localizedDescription.UTF8String);
            }
        };
        impl->target.onOpenLogs = ^{
            NSString* home = NSHomeDirectory();
            NSString* log = [home stringByAppendingPathComponent:@"Library/Logs/autowhisper/autowhisper.log"];
            [[NSWorkspace sharedWorkspace] openURL:[NSURL fileURLWithPath:log]];
        };

        NSMenu* menu = [[NSMenu alloc] init];
        menu.autoenablesItems = NO;

        NSMenuItem* header = [[NSMenuItem alloc]
            initWithTitle:[NSString stringWithUTF8String:state_label(TrayState::IDLE).c_str()]
                   action:nil
            keyEquivalent:@""];
        [header setEnabled:NO];
        [menu addItem:header];
        impl->header_item = header;

        [menu addItem:[NSMenuItem separatorItem]];

        NSMenuItem* settings = [[NSMenuItem alloc]
            initWithTitle:@"Open Settings\u2026"
                   action:@selector(openSettingsClicked:)
            keyEquivalent:@","];
        settings.target = impl->target;
        [menu addItem:settings];

        NSMenuItem* logs = [[NSMenuItem alloc]
            initWithTitle:@"Open Logs"
                   action:@selector(openLogsClicked:)
            keyEquivalent:@""];
        logs.target = impl->target;
        [menu addItem:logs];

        [menu addItem:[NSMenuItem separatorItem]];

        NSMenuItem* input_dev = [[NSMenuItem alloc] initWithTitle:@"Input: \u2014" action:nil keyEquivalent:@""];
        [input_dev setEnabled:NO]; [menu addItem:input_dev];
        impl->input_item = input_dev;

        NSMenuItem* output_dev = [[NSMenuItem alloc] initWithTitle:@"Output: \u2014" action:nil keyEquivalent:@""];
        [output_dev setEnabled:NO]; [menu addItem:output_dev];
        impl->output_item = output_dev;

        NSMenuItem* hotkey = [[NSMenuItem alloc] initWithTitle:@"Hotkey: \u2014" action:nil keyEquivalent:@""];
        [hotkey setEnabled:NO]; [menu addItem:hotkey];
        impl->hotkey_item = hotkey;

        NSMenuItem* cancel = [[NSMenuItem alloc] initWithTitle:@"Cancel: \u2014" action:nil keyEquivalent:@""];
        [cancel setEnabled:NO]; [menu addItem:cancel];
        impl->cancel_item = cancel;

        [menu addItem:[NSMenuItem separatorItem]];

        NSMenuItem* quit = [[NSMenuItem alloc]
            initWithTitle:@"Quit AutoWhisper"
                   action:@selector(quitClicked:)
            keyEquivalent:@"q"];
        quit.target = impl->target;
        [menu addItem:quit];

        impl->item.menu = menu;
        spdlog::info("NSStatusItem created");
    });
}

void TrayManager::stop() {
    if (!enabled_) return;
    auto impl = impl_;
    dispatch_async(dispatch_get_main_queue(), ^{
        if (impl->item) {
            [[NSStatusBar systemStatusBar] removeStatusItem:impl->item];
            impl->item = nil;
        }
    });
}

void TrayManager::set_state(TrayState state) {
    state_ = state;
    if (!enabled_) return;
    auto impl = impl_;
    dispatch_async(dispatch_get_main_queue(), ^{
        if (!impl->item) return;
        if (@available(macOS 11.0, *)) {
            impl->item.button.image = mic_image_for_state(state);
        }
        impl->header_item.title =
            [NSString stringWithUTF8String:state_label(state).c_str()];
        impl->item.button.accessibilityLabel =
            [NSString stringWithFormat:@"AutoWhisper, %s", state_label(state).c_str()];
    });
}

void TrayManager::set_input_device(const std::string& name) {
    input_device_ = name;
    auto impl = impl_;
    std::string label = "Input: " + name;
    dispatch_async(dispatch_get_main_queue(), ^{
        if (impl->input_item) {
            impl->input_item.title = [NSString stringWithUTF8String:label.c_str()];
        }
    });
}

void TrayManager::set_output_device(const std::string& name) {
    output_device_ = name;
    auto impl = impl_;
    std::string label = "Output: " + name;
    dispatch_async(dispatch_get_main_queue(), ^{
        if (impl->output_item) {
            impl->output_item.title = [NSString stringWithUTF8String:label.c_str()];
        }
    });
}

void TrayManager::set_hotkey(const std::vector<std::string>& hotkeys) {
    trigger_hotkeys_ = hotkeys;
    auto impl = impl_;
    std::string label = "Hotkey: " + format_hotkeys(hotkeys);
    dispatch_async(dispatch_get_main_queue(), ^{
        if (impl->hotkey_item) {
            impl->hotkey_item.title = [NSString stringWithUTF8String:label.c_str()];
        }
    });
}

void TrayManager::set_cancel_hotkey(const std::vector<std::string>& hotkeys) {
    cancel_hotkeys_ = hotkeys;
    auto impl = impl_;
    std::string label = "Cancel: " + format_hotkeys(hotkeys);
    dispatch_async(dispatch_get_main_queue(), ^{
        if (impl->cancel_item) {
            impl->cancel_item.title = [NSString stringWithUTF8String:label.c_str()];
        }
    });
}

} // namespace autowhisper
