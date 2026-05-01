# Build Warnings Audit

Source logs: `configure-audit.log`, `build-audit.log`.

- Compiler warning line count: 11
- Archiver warning line count: 6
- Configure warning/info line count: 8

## Compiler warnings

- `ranlib: warning: 'libggml-cpu.a(ggml-cpu-hbm.cpp.o)' has no symbols`
- `ranlib: warning: 'libggml-cpu.a(amx.cpp.o)' has no symbols`
- `ranlib: warning: 'libggml-cpu.a(mmq.cpp.o)' has no symbols`
- `/Users/odin-mac-730/src/autowhisper/src/audio/audio.h:53:9: warning: private field 'silence_threshold_samples_' is not used [-Wunused-private-field]`
- `/Users/odin-mac-730/src/autowhisper/deps/cpp-httplib/httplib.h:3568:42: warning: identifier '_t' preceded by whitespace in a literal operator declaration is deprecated [-Wdeprecated-literal-operator]`
- `/Users/odin-mac-730/src/autowhisper/src/pulseaudio/pulseaudio.h:25:11: warning: private field 'beep_duration_' is not used [-Wunused-private-field]`
- `/Users/odin-mac-730/src/autowhisper/src/pulseaudio/pulseaudio.h:26:10: warning: private field 'initialized_' is not used [-Wunused-private-field]`

## Archiver warnings

- `ranlib: warning: 'libggml-cpu.a(ggml-cpu-hbm.cpp.o)' has no symbols`
- `ranlib: warning: 'libggml-cpu.a(amx.cpp.o)' has no symbols`
- `ranlib: warning: 'libggml-cpu.a(mmq.cpp.o)' has no symbols`

## Configure warnings

- `CMake Deprecation Warning at deps/whisper.cpp/CMakeLists.txt:1 (cmake_minimum_required):`
- `-- Warning: ccache not found - consider installing it for faster compilation or disable this warning with GGML_CCACHE=OFF`
- `-- Could NOT find OpenMP_C (missing: OpenMP_C_FLAGS OpenMP_C_LIB_NAMES) `
- `-- Could NOT find OpenMP_CXX (missing: OpenMP_CXX_FLAGS OpenMP_CXX_LIB_NAMES) `
- `-- Could NOT find OpenMP (missing: OpenMP_C_FOUND OpenMP_CXX_FOUND) `
- `CMake Warning at deps/whisper.cpp/ggml/src/ggml-cpu/CMakeLists.txt:53 (message):`
- `  OpenMP not found`
- `CMake Deprecation Warning at deps/CLI11/CMakeLists.txt:1 (cmake_minimum_required):`
