#include "avatar/avatar.h"

#include <spdlog/spdlog.h>

#import <AppKit/AppKit.h>
#import <QuartzCore/QuartzCore.h>

#include <vector>

namespace autowhisper {

// macOS shell: a non-activating floating NSPanel whose layer shows the
// shared renderer's premultiplied BGRA buffer (CGBitmapInfo little-endian,
// premultiplied first — byte-identical to the X11/Win32 paths). All AppKit
// work happens on the main thread; the daemon already runs NSApp there.

struct AvatarManager::Impl {
    AvatarManager* mgr = nullptr;
    AvatarTicker* ticker = nullptr;
    NSPanel* panel = nil;
    NSTimer* timer = nil;
    int size = 96;
    std::vector<uint32_t> buf;

    void frame() {
        auto snap = ticker->tick();
        const AvatarCharacter& ch = *find_character(mgr->config_.character);
        avatar_rasterize(buf.data(), size, ch, snap.state, snap.t, snap.phase,
                         snap.level);

        CGColorSpaceRef space = CGColorSpaceCreateDeviceRGB();
        CGContextRef ctx = CGBitmapContextCreate(
            buf.data(), size, size, 8, size_t(size) * 4, space,
            kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Little);
        CGImageRef image = ctx ? CGBitmapContextCreateImage(ctx) : nullptr;
        if (image) {
            panel.contentView.layer.contents = (__bridge id)image;
            CGImageRelease(image);
        }
        if (ctx) CGContextRelease(ctx);
        CGColorSpaceRelease(space);
    }

    void open() {
        NSScreen* screen = [NSScreen mainScreen];
        NSRect sf = screen.visibleFrame;
        NSRect rect = NSMakeRect(NSMaxX(sf) - size - 28, NSMinY(sf) + 64, size, size);

        panel = [[NSPanel alloc]
            initWithContentRect:rect
                      styleMask:NSWindowStyleMaskBorderless |
                                NSWindowStyleMaskNonactivatingPanel
                        backing:NSBackingStoreBuffered
                          defer:NO];
        panel.level = NSFloatingWindowLevel;
        panel.opaque = NO;
        panel.backgroundColor = [NSColor clearColor];
        panel.hasShadow = NO;
        panel.movableByWindowBackground = YES;  // drag the orb anywhere
        panel.collectionBehavior = NSWindowCollectionBehaviorCanJoinAllSpaces |
                                   NSWindowCollectionBehaviorStationary;
        panel.contentView.wantsLayer = YES;
        panel.contentView.layer.contentsGravity = kCAGravityResize;

        buf.assign(size_t(size) * size, 0);
        frame();
        [panel orderFrontRegardless];

        Impl* self_impl = this;
        timer = [NSTimer scheduledTimerWithTimeInterval:1.0 / 30.0
                                                repeats:YES
                                                  block:^(NSTimer*) {
                                                    self_impl->frame();
                                                  }];
    }

    void close() {
        [timer invalidate];
        timer = nil;
        [panel orderOut:nil];
        panel = nil;
    }
};

AvatarManager::AvatarManager(const AvatarConfig& config, LevelProvider level,
                             AmbientProvider ambient)
    : config_(config), level_(std::move(level)), ambient_(std::move(ambient)),
      impl_(std::make_unique<Impl>()) {
    impl_->mgr = this;
    impl_->size = config_.size;
}

AvatarManager::~AvatarManager() { stop(); }

void AvatarManager::start() {
    if (!config_.enabled || running_.exchange(true)) return;
    ticker_ = std::make_unique<AvatarTicker>(AvatarStateMachine{}, level_, ambient_);
    impl_->ticker = ticker_.get();
    Impl* impl = impl_.get();
    dispatch_async(dispatch_get_main_queue(), ^{
      impl->open();
    });
    spdlog::info("avatar: {} joins ({})", find_character(config_.character)->name,
                 find_character(config_.character)->epithet);
}

void AvatarManager::stop() {
    if (!running_.exchange(false)) return;
    Impl* impl = impl_.get();
    if ([NSThread isMainThread]) {
        // Daemon cleanup runs on the main thread after NSApp unwinds;
        // dispatch_sync to the main queue would deadlock here.
        impl->close();
    } else {
        dispatch_sync(dispatch_get_main_queue(), ^{
          impl->close();
        });
    }
}

void AvatarManager::set_state(AvatarState s) {
    if (running_.load() && impl_->ticker) impl_->ticker->request(s);
}

} // namespace autowhisper
