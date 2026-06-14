#include "platform/macos/onboarding.h"

#import <AppKit/AppKit.h>
#import <ApplicationServices/ApplicationServices.h>
#import <AVFoundation/AVFoundation.h>
#import <CoreGraphics/CoreGraphics.h>
#import <Foundation/Foundation.h>
#include <mach-o/dyld.h>

namespace autowhisper {

bool aw_macos_is_app_bundle_launch() {
    NSString* bundle_path = [[NSBundle mainBundle] bundlePath];
    if (bundle_path != nil && [bundle_path hasSuffix:@".app"]) return true;

    uint32_t size = 0;
    _NSGetExecutablePath(nullptr, &size);
    if (size == 0) return false;
    std::string executable_path(size, '\0');
    if (_NSGetExecutablePath(executable_path.data(), &size) != 0) return false;
    executable_path.resize(std::char_traits<char>::length(executable_path.c_str()));
    return executable_path.find(".app/Contents/MacOS/") != std::string::npos;
}

std::string aw_macos_permissions_status_json() {
    // Hand-built JSON (no nlohmann import in this AppKit TU). Each item
    // mirrors the /api/platform shape: id/name/state/required/detail/deep_link.
    auto bdj = [](const std::string& s) {
        std::string out;
        for (char c : s) {
            if (c == '"' || c == '\\') out += '\\';
            out += c;
        }
        return out;
    };
    auto item = [&](const char* id, const char* name, const char* state,
                    const char* detail, const char* link) {
        return std::string("{\"id\":\"") + id + "\",\"name\":\"" + name +
               "\",\"state\":\"" + state + "\",\"required\":true,\"detail\":\"" +
               bdj(detail) + "\",\"deep_link\":\"" + link + "\"}";
    };

    const char* sec = "x-apple.systempreferences:com.apple.preference.security";

    const char* mic_state = "unknown";
    switch ([AVCaptureDevice authorizationStatusForMediaType:AVMediaTypeAudio]) {
        case AVAuthorizationStatusAuthorized:    mic_state = "granted"; break;
        case AVAuthorizationStatusDenied:        mic_state = "denied"; break;
        case AVAuthorizationStatusRestricted:    mic_state = "restricted"; break;
        case AVAuthorizationStatusNotDetermined: mic_state = "not_determined"; break;
    }
    const char* listen_state = CGPreflightListenEventAccess() ? "granted" : "denied";
    const char* post_state = CGPreflightPostEventAccess() ? "granted" : "denied";

    std::string json = "{\"applicable\":true,\"platform\":\"macos\",\"permissions\":[";
    json += item("microphone", "Microphone", mic_state,
                 "Required to capture your voice.",
                 (std::string(sec) + "?Privacy_Microphone").c_str());
    json += "," + item("input_monitoring", "Input Monitoring", listen_state,
                       "Required for the global push-to-talk hotkey (the event tap). "
                       "If denied, no shortcut fires.",
                       (std::string(sec) + "?Privacy_ListenEvent").c_str());
    json += "," + item("accessibility", "Accessibility", post_state,
                       "Required to type the transcribed text into other apps.",
                       (std::string(sec) + "?Privacy_Accessibility").c_str());
    json += "]}";
    return json;
}

void aw_macos_prompt_required_permissions() {
    // Input Monitoring: required for the global push-to-talk event tap.
    if (!CGPreflightListenEventAccess()) {
        (void)CGRequestListenEventAccess();
    }

    // Accessibility/PostEvent: required for text injection / Cmd+V automation.
    if (!CGPreflightPostEventAccess()) {
        (void)CGRequestPostEventAccess();
    }

    // AX prompt opens the Accessibility pane when the app has not been trusted.
    NSDictionary* options = @{
        (__bridge NSString*)kAXTrustedCheckOptionPrompt: @YES
    };
    (void)AXIsProcessTrustedWithOptions((__bridge CFDictionaryRef)options);

    // Microphone: required for capture. If the user has not decided yet,
    // trigger the native TCC prompt while setup stays visible.
    AVAuthorizationStatus status = [AVCaptureDevice authorizationStatusForMediaType:AVMediaTypeAudio];
    if (status == AVAuthorizationStatusNotDetermined) {
        [AVCaptureDevice requestAccessForMediaType:AVMediaTypeAudio completionHandler:^(__unused BOOL granted) {}];
    }
}

bool aw_macos_launch_setup_helper(const std::string& config_path,
                                  const std::string& setup_error) {
    NSString* executable_path = [[NSBundle mainBundle] pathForResource:@"AutoWhisperSettings" ofType:nil];
    if (executable_path == nil) {
        uint32_t size = 0;
        _NSGetExecutablePath(nullptr, &size);
        if (size > 0) {
            std::string self_path(size, '\0');
            if (_NSGetExecutablePath(self_path.data(), &size) == 0) {
                self_path.resize(std::char_traits<char>::length(self_path.c_str()));
                NSString* sibling = [[NSString stringWithUTF8String:self_path.c_str()] stringByDeletingLastPathComponent];
                executable_path = [sibling stringByAppendingPathComponent:@"AutoWhisperSettings"];
            }
        }
    }
    if (executable_path == nil) return false;

    NSMutableArray<NSString*>* arguments = [NSMutableArray array];
    if (!config_path.empty()) {
        [arguments addObject:@"--config"];
        [arguments addObject:[NSString stringWithUTF8String:config_path.c_str()]];
    }
    if (!setup_error.empty()) {
        [arguments addObject:@"--setup-error"];
        [arguments addObject:[NSString stringWithUTF8String:setup_error.c_str()]];
    }

    NSTask* task = [[NSTask alloc] init];
    [task setLaunchPath:executable_path];
    [task setArguments:arguments];

    @try {
        [task launch];
        return true;
    } @catch (NSException*) {
        return false;
    }
}

} // namespace autowhisper
