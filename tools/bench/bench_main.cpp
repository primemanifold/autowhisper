// AutoWhisper benchmark harness: measures model load time, per-utterance
// transcription latency, real-time factor, and word error rate against a
// manifest of audio fixtures. This is the source of truth for any
// speed/accuracy numbers published in the README or marketing surfaces.
//
// Usage:
//   autowhisper_bench --manifest bench/manifest.tsv --model distil-small.en \
//                     [--device cpu] [--threads 4] [--runs 3] [--json out.json]
//
// Manifest format (tab-separated): <wav path>\t<reference transcript>
// Relative wav paths resolve against the manifest's directory.

#include "bench/wer.h"
#include "config/config.h"
#include "inference/inference.h"
#include "models/models.h"
#include "runtime/audio_file.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace autowhisper;

namespace {

struct Utterance {
    std::string wav_path;
    std::string reference;
};

struct UtteranceResult {
    std::string wav_path;
    double audio_seconds = 0.0;
    double latency_seconds = 0.0;  // median across runs
    double rtf = 0.0;
    double wer = 0.0;
    std::string hypothesis;
};

std::vector<Utterance> load_manifest(const std::string& path) {
    std::ifstream in(path);
    if (!in) {
        throw std::runtime_error("Cannot open manifest: " + path);
    }
    const fs::path base = fs::path(path).parent_path();

    std::vector<Utterance> utterances;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#') continue;
        auto tab = line.find('\t');
        if (tab == std::string::npos) {
            throw std::runtime_error("Manifest line missing tab separator: " + line);
        }
        Utterance u;
        fs::path wav = line.substr(0, tab);
        u.wav_path = wav.is_absolute() ? wav.string() : (base / wav).string();
        u.reference = line.substr(tab + 1);
        utterances.push_back(std::move(u));
    }
    if (utterances.empty()) {
        throw std::runtime_error("Manifest has no utterances: " + path);
    }
    return utterances;
}

double median(std::vector<double> v) {
    std::sort(v.begin(), v.end());
    const size_t n = v.size();
    return n % 2 ? v[n / 2] : (v[n / 2 - 1] + v[n / 2]) / 2.0;
}

std::string json_escape(const std::string& s) {
    std::string out;
    for (char c : s) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\t': out += "\\t"; break;
            default: out += c;
        }
    }
    return out;
}

}  // namespace

int main(int argc, char** argv) {
    std::string manifest_path = "bench/manifest.tsv";
    std::string model = "distil-small.en";
    std::string device = "cpu";
    std::string json_path;
    int threads = 4;
    int runs = 3;

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        auto next = [&]() -> std::string {
            if (i + 1 >= argc) {
                std::cerr << "Missing value for " << arg << "\n";
                exit(2);
            }
            return argv[++i];
        };
        if (arg == "--manifest") manifest_path = next();
        else if (arg == "--model") model = next();
        else if (arg == "--device") device = next();
        else if (arg == "--threads") threads = std::stoi(next());
        else if (arg == "--runs") runs = std::stoi(next());
        else if (arg == "--json") json_path = next();
        else {
            std::cerr << "Unknown argument: " << arg << "\n"
                      << "Usage: autowhisper_bench --manifest FILE --model NAME "
                         "[--device cpu|cuda|auto] [--threads N] [--runs N] [--json FILE]\n";
            return 2;
        }
    }

    try {
        if (!is_model_downloaded(model)) {
            std::cerr << "Model '" << model << "' is not downloaded. Run: "
                      << "autowhisper model download " << model << "\n";
            return 1;
        }

        auto utterances = load_manifest(manifest_path);

        ModelConfig mc;
        mc.size = model;
        mc.device = device;
        mc.num_threads = threads;
        mc.language = model_is_english_only(model) ? "en" : "auto";

        WhisperInference inference(mc);
        const auto load_start = std::chrono::steady_clock::now();
        inference.load();
        const double load_seconds =
            std::chrono::duration<double>(std::chrono::steady_clock::now() - load_start)
                .count();

        std::vector<UtteranceResult> results;
        for (const auto& u : utterances) {
            UtteranceResult r;
            r.wav_path = u.wav_path;

            auto samples = decode_audio_file(u.wav_path).samples;
            r.audio_seconds = static_cast<double>(samples.size()) / 16000.0;

            std::vector<double> latencies;
            std::string text;
            for (int run = 0; run < runs; run++) {
                const auto t0 = std::chrono::steady_clock::now();
                text = inference.transcribe(samples);
                latencies.push_back(
                    std::chrono::duration<double>(std::chrono::steady_clock::now() - t0)
                        .count());
            }

            r.latency_seconds = median(latencies);
            r.rtf = r.audio_seconds > 0 ? r.latency_seconds / r.audio_seconds : 0.0;
            r.hypothesis = text;
            r.wer = bench::word_error_rate(u.reference, text);
            results.push_back(std::move(r));
        }

        double total_audio = 0, total_latency = 0, wer_sum = 0;
        for (const auto& r : results) {
            total_audio += r.audio_seconds;
            total_latency += r.latency_seconds;
            wer_sum += r.wer;
        }
        const double mean_wer = wer_sum / static_cast<double>(results.size());
        const double overall_rtf = total_audio > 0 ? total_latency / total_audio : 0.0;

        std::cout << "\nmodel: " << model << "  device: " << device
                  << "  threads: " << threads << "  runs/utterance: " << runs << "\n";
        std::cout << "model load: " << load_seconds << " s\n\n";
        std::cout << "audio_s\tlatency_s\trtf\twer\tfile\n";
        for (const auto& r : results) {
            std::cout << r.audio_seconds << "\t" << r.latency_seconds << "\t" << r.rtf
                      << "\t" << r.wer << "\t" << fs::path(r.wav_path).filename().string()
                      << "\n";
        }
        std::cout << "\noverall: rtf=" << overall_rtf << " mean_wer=" << mean_wer << "\n";

        if (!json_path.empty()) {
            std::ofstream out(json_path);
            out << "{\n"
                << "  \"model\": \"" << json_escape(model) << "\",\n"
                << "  \"device\": \"" << json_escape(device) << "\",\n"
                << "  \"threads\": " << threads << ",\n"
                << "  \"runs_per_utterance\": " << runs << ",\n"
                << "  \"load_seconds\": " << load_seconds << ",\n"
                << "  \"overall_rtf\": " << overall_rtf << ",\n"
                << "  \"mean_wer\": " << mean_wer << ",\n"
                << "  \"utterances\": [\n";
            for (size_t i = 0; i < results.size(); i++) {
                const auto& r = results[i];
                out << "    {\"file\": \"" << json_escape(r.wav_path) << "\", "
                    << "\"audio_seconds\": " << r.audio_seconds << ", "
                    << "\"latency_seconds\": " << r.latency_seconds << ", "
                    << "\"rtf\": " << r.rtf << ", "
                    << "\"wer\": " << r.wer << ", "
                    << "\"hypothesis\": \"" << json_escape(r.hypothesis) << "\"}"
                    << (i + 1 < results.size() ? "," : "") << "\n";
            }
            out << "  ]\n}\n";
            std::cout << "wrote " << json_path << "\n";
        }

        return 0;
    } catch (const std::exception& e) {
        std::cerr << "bench failed: " << e.what() << "\n";
        return 1;
    }
}
