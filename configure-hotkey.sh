#!/bin/bash
# AutoWhisper Hotkey Configuration - press keys to set shortcuts

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
CONFIG="$SCRIPT_DIR/config.toml"
VENV_DIR="$SCRIPT_DIR/.hotkey-venv"

# Setup venv with pynput if needed
setup_venv() {
    if [ ! -f "$VENV_DIR/bin/python" ]; then
        echo "Setting up Python environment..."
        python3 -m venv "$VENV_DIR"
        "$VENV_DIR/bin/pip" install -q pynput
    elif ! "$VENV_DIR/bin/python" -c "import pynput" 2>/dev/null; then
        "$VENV_DIR/bin/pip" install -q pynput
    fi
}

capture_hotkey() {
    "$VENV_DIR/bin/python" << 'PYEOF'
from pynput import keyboard
pressed, combo = set(), set()
MAP = {'ctrl_l':'ctrl','ctrl_r':'ctrl','alt_l':'alt','alt_r':'alt','alt_gr':'alt',
       'shift_l':'shift','shift_r':'shift','cmd':'super','cmd_l':'super','cmd_r':'super'}

def norm(k):
    if hasattr(k, 'char') and k.char:
        return k.char.lower()
    if hasattr(k, 'name') and k.name:
        n = k.name.lower()
        return MAP.get(n, n)
    if hasattr(k, 'vk'):
        return f'key{k.vk}'
    return None

def on_press(k):
    n = norm(k)
    if n:
        pressed.add(n)
        combo.update(pressed)

def on_release(k):
    n = norm(k)
    if n:
        pressed.discard(n)
    if not pressed and combo:
        if combo == {'escape'}:
            print('CANCEL')
        else:
            mods = ['ctrl','alt','shift','super']
            # Filter out unrecognized keycodes (keyXXXXX) when modifiers are present
            # This allows physical keys that generate modifiers + raw keycode to work
            has_mods = any(m in combo for m in mods)
            filtered = {x for x in combo if not (has_mods and x.startswith('key') and x[3:].isdigit())}
            out = [m for m in mods if m in filtered] + sorted(x for x in filtered if x not in mods)
            print('+'.join(out))
        return False

with keyboard.Listener(on_press=on_press, on_release=on_release) as l:
    l.join()
PYEOF
}

setup_venv

echo "=== AutoWhisper Hotkey Setup ==="
echo ""
grep -E "^(trigger|cancel|mode)" "$CONFIG" 2>/dev/null | sed 's/^/Current: /'
echo ""
echo "1) Set trigger hotkey"
echo "2) Set cancel hotkey"
echo "q) Quit"
echo ""
read -p "Choice: " c

case "$c" in
    1) echo "Press your hotkey combo (Escape=cancel)..."
       hk=$(capture_hotkey)
       [ -n "$hk" ] && [ "$hk" != "CANCEL" ] && sed -i "s/^trigger *= *\"[^\"]*\"/trigger = \"$hk\"/" "$CONFIG" && echo "Set trigger=$hk"
       ;;
    2) echo "Press your hotkey combo (Escape=cancel)..."
       hk=$(capture_hotkey)
       [ -n "$hk" ] && [ "$hk" != "CANCEL" ] && sed -i "s/^cancel *= *\"[^\"]*\"/cancel = \"$hk\"/" "$CONFIG" && echo "Set cancel=$hk"
       ;;
esac
