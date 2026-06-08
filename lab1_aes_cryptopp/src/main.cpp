#include <common/cli_parser.hpp>
#include "aes_service.hpp"
#include "key_manager.hpp"
#include "nonce_manager.hpp"
#include "sidecar.hpp"

#include <cryptopp/base64.h>
#include <cryptopp/hex.h>
#include <cryptopp/filters.h>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
#include <numeric>
#include <stdexcept>
#include <string>
#include <vector>

using json = nlohmann::json;

// ─── File I/O ──────────────────────────────────────────────────────────────

static std::vector<uint8_t> read_file(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) throw std::runtime_error("Cannot read: " + path);
    return {std::istreambuf_iterator<char>(f), {}};
}

static void write_file(const std::string& path, const std::vector<uint8_t>& data) {
    std::ofstream f(path, std::ios::binary);
    if (!f) throw std::runtime_error("Cannot write: " + path);
    f.write(reinterpret_cast<const char*>(data.data()), data.size());
}

// ─── Encoding layer ────────────────────────────────────────────────────────

static std::vector<uint8_t> encode_data(const std::vector<uint8_t>& data,
                                        const std::string& enc)
{
    using namespace CryptoPP;
    if (enc == "hex") {
        std::string out;
        HexEncoder h(new StringSink(out), false /*lowercase*/);
        h.Put(data.data(), data.size()); h.MessageEnd();
        return std::vector<uint8_t>(out.begin(), out.end());
    }
    if (enc == "base64") {
        std::string out;
        Base64Encoder b(new StringSink(out));
        b.Put(data.data(), data.size()); b.MessageEnd();
        return std::vector<uint8_t>(out.begin(), out.end());
    }
    return data; // raw (default)
}

static std::vector<uint8_t> decode_data(const std::vector<uint8_t>& data,
                                        const std::string& enc)
{
    using namespace CryptoPP;
    std::string in(data.begin(), data.end());
    if (enc == "hex") {
        std::string out;
        StringSource(in, true, new HexDecoder(new StringSink(out)));
        return std::vector<uint8_t>(out.begin(), out.end());
    }
    if (enc == "base64") {
        std::string out;
        StringSource(in, true, new Base64Decoder(new StringSink(out)));
        return std::vector<uint8_t>(out.begin(), out.end());
    }
    return data; // raw
}

// ─── Key helper ────────────────────────────────────────────────────────────

static std::vector<uint8_t> get_key(const common::CliArgs& args) {
    if (common::has(args, "key-hex"))
        return lab1::hex_to_bytes(common::get(args, "key-hex"));
    if (common::has(args, "key-file"))
        return lab1::load_key_hex(common::get(args, "key-file"));
    if (common::has(args, "key"))
        return lab1::load_key_auto(common::get(args, "key"));
    throw std::runtime_error("Thiếu --key-hex, --key-file, hoặc --key");
}

// ─── KAT runner ────────────────────────────────────────────────────────────

static void run_kat_file(const std::string& json_path) {
    std::ifstream f(json_path);
    if (!f) throw std::runtime_error("Cannot open KAT file: " + json_path);
    json j = json::parse(f);

    // File-level mode/algo as defaults; each test can override
    std::string file_mode = j.value("mode", "cbc");
    std::string file_algo = j.value("algorithm", json_path);

    std::cout << "KAT: " << file_algo << " [" << json_path << "]\n";

    int pass = 0, fail = 0;
    for (auto& t : j["tests"]) {
        std::string name = t.value("name", "case-" + std::to_string(pass + fail + 1));

        // Per-test mode overrides file-level
        std::string mode_str = t.value("mode", file_mode);
        auto mode = lab1::mode_from_string(mode_str);

        auto key    = lab1::hex_to_bytes(t["key"].get<std::string>());
        auto pt_hex = t.value("plaintext", "");
        auto iv_hex = t.value("iv", "");
        auto aad_hex = t.value("aad", "");
        auto exp_ct_hex  = t.value("ciphertext", "");
        auto exp_tag_hex = t.value("tag", "");

        std::vector<uint8_t> pt, iv, aad, exp_ct, exp_tag;
        if (!pt_hex.empty())      pt      = lab1::hex_to_bytes(pt_hex);
        if (!iv_hex.empty())      iv      = lab1::hex_to_bytes(iv_hex);
        if (!aad_hex.empty())     aad     = lab1::hex_to_bytes(aad_hex);
        if (!exp_ct_hex.empty())  exp_ct  = lab1::hex_to_bytes(exp_ct_hex);
        if (!exp_tag_hex.empty()) exp_tag = lab1::hex_to_bytes(exp_tag_hex);

        try {
            // no_padding=true: NIST vectors use full blocks, no PKCS7
            auto res = lab1::aes_encrypt(mode, key, pt, iv, aad, /*no_padding=*/true);

            bool ct_ok  = (res.ciphertext == exp_ct);
            bool tag_ok = exp_tag.empty() || (res.tag == exp_tag);

            if (ct_ok && tag_ok) {
                ++pass;
                std::cout << "  PASS [" << name << "]\n";
            } else {
                ++fail;
                std::cout << "  FAIL [" << name << "]";
                if (!ct_ok)  std::cout << " ciphertext-mismatch";
                if (!tag_ok) std::cout << " tag-mismatch";
                std::cout << "\n";
                if (!ct_ok) {
                    std::cout << "    expected: " << exp_ct_hex << "\n";
                    std::cout << "    got     : " << lab1::bytes_to_hex(res.ciphertext) << "\n";
                }
                if (!tag_ok) {
                    std::cout << "    exp tag : " << exp_tag_hex << "\n";
                    std::cout << "    got tag : " << lab1::bytes_to_hex(res.tag) << "\n";
                }
            }
        } catch (const std::exception& e) {
            ++fail;
            std::cout << "  FAIL [" << name << "] exception: " << e.what() << "\n";
        }
    }

    std::cout << "  => Passed " << pass << "/" << (pass + fail) << "\n\n";
    if (fail > 0)
        throw std::runtime_error("KAT FAILED: " + std::to_string(fail) +
                                 " vector(s) failed in " + json_path);
}

// ─── Benchmark ─────────────────────────────────────────────────────────────

static void run_benchmark(const std::vector<uint8_t>& key) {
    const std::vector<std::pair<std::string, lab1::AesMode>> modes = {
        {"cbc", lab1::AesMode::CBC}, {"cfb", lab1::AesMode::CFB},
        {"ofb", lab1::AesMode::OFB}, {"ctr", lab1::AesMode::CTR},
        {"gcm", lab1::AesMode::GCM}, {"ccm", lab1::AesMode::CCM},
    };
    const std::vector<std::size_t> sizes = {
        1024, 4096, 16384, 262144, 1048576, 8388608
    };

    using clock  = std::chrono::high_resolution_clock;
    using ms_dur = std::chrono::duration<double, std::milli>;

    std::cout << "algo,mode,size_bytes,latency_ms_mean,latency_ms_median,"
                 "latency_ms_stddev,ci95_ms,throughput_mb_s\n";
    std::cout << std::fixed;
    std::cout.precision(4);

    for (auto& [mode_name, mode] : modes) {
        for (auto sz : sizes) {
            std::vector<uint8_t> pt(sz, 0xAB);

            // Warmup ~1 s
            auto t_end = clock::now() + std::chrono::seconds(1);
            while (clock::now() < t_end)
                lab1::aes_encrypt(mode, key, pt);

            const int N = 30;
            std::vector<double> samples(N);
            for (int i = 0; i < N; ++i) {
                auto t0 = clock::now();
                lab1::aes_encrypt(mode, key, pt);
                samples[i] = ms_dur(clock::now() - t0).count();
            }

            std::sort(samples.begin(), samples.end());
            double sum = std::accumulate(samples.begin(), samples.end(), 0.0);
            double mean   = sum / N;
            double median = samples[N / 2];
            double sq = 0;
            for (auto s : samples) sq += (s - mean) * (s - mean);
            double stddev = std::sqrt(sq / N);
            double ci95   = 1.96 * stddev / std::sqrt(static_cast<double>(N));
            double tput   = (sz / 1048576.0) / (mean / 1000.0);

            std::cout << "AES-256," << mode_name << "," << sz << ","
                      << mean << "," << median << "," << stddev << ","
                      << ci95 << "," << tput << "\n";
        }
    }
}

// ─── Usage ─────────────────────────────────────────────────────────────────

static void print_usage() {
    std::cout << R"(aestool — Lab 1: AES via Crypto++

Commands:
  keygen   --out key.hex

  encrypt  --mode <ecb|cbc|cfb|ofb|ctr|xts|ccm|gcm>
           --key-hex <hex> | --key-file <file> | --key <file>
           --in <file> | --text <string>
           [--iv <hex>] [--aad-file <file>] [--aad-text <str>]
           [--encode hex|base64|raw]  (default: raw)
           [--out <file>]
           [--allow-ecb]              (required for ECB mode)

  decrypt  --mode <mode>
           --key-hex <hex> | --key-file <file> | --key <file>
           --in <file>
           [--iv <hex>]    (or auto-loaded from sidecar)
           [--tag <hex>]   (or auto-loaded from sidecar)
           [--aad-file <file>] [--aad-text <str>]
           [--encode hex|base64|raw]
           [--out <file>]

  kat      <vectors.json> [more.json ...]
           Runs NIST KAT vectors; prints PASS/FAIL per case.

  benchmark [--key-hex <hex> | --key-file <file>]
           Measures encrypt throughput for CBC/CFB/OFB/CTR/GCM/CCM
           across 6 payload sizes; outputs CSV to stdout.

Sidecar: after encrypt, <out>.hdr.json stores alg/mode/iv/aad/tag.
         Decrypt auto-loads sidecar if --iv/--tag not given.
)";
}

// ─── main ──────────────────────────────────────────────────────────────────

int main(int argc, char* argv[]) {
    try {
        auto args = common::parse_args(argc, argv);
        std::string encode = common::get(args, "encode", "raw");

        // ── keygen ──────────────────────────────────────────────────────
        if (args.command == "keygen") {
            auto key = lab1::keygen_aes256();
            std::string out = common::get(args, "out", "key.hex");
            lab1::save_key_hex(out, key);
            std::cout << "Key saved : " << out << "\n";
            std::cout << "Key (hex) : " << lab1::bytes_to_hex(key) << "\n";

        // ── encrypt ─────────────────────────────────────────────────────
        } else if (args.command == "encrypt") {
            auto mode_str = common::get(args, "mode", "gcm");
            auto mode     = lab1::mode_from_string(mode_str);
            auto key      = get_key(args);

            // ECB safety gate
            if (mode == lab1::AesMode::ECB) {
                std::cerr << "WARNING: ECB is insecure — identical plaintext blocks "
                             "produce identical ciphertext (pattern leakage).\n";
                if (!common::has(args, "allow-ecb"))
                    throw std::runtime_error("ECB requires --allow-ecb to proceed");
            }

            // Load plaintext
            std::vector<uint8_t> plaintext;
            if (common::has(args, "in")) {
                plaintext = read_file(common::get(args, "in"));
            } else if (common::has(args, "text")) {
                auto s = common::get(args, "text");
                plaintext.assign(s.begin(), s.end());
            } else {
                throw std::runtime_error("Thiếu --in <file> hoặc --text <string>");
            }

            // ECB size limit: > 16 KiB is blocked
            if (mode == lab1::AesMode::ECB && plaintext.size() > 16384)
                throw std::runtime_error(
                    "ECB: input > 16 KiB is blocked for security. Use a different mode.");

            // IV: user-provided or auto-generated
            std::vector<uint8_t> iv_in;
            if (common::has(args, "iv"))
                iv_in = lab1::hex_to_bytes(common::get(args, "iv"));
            else
                iv_in = lab1::generate_iv(mode_str);

            lab1::validate_iv(mode_str, iv_in);

            // AAD
            std::vector<uint8_t> aad;
            if (common::has(args, "aad-file"))
                aad = read_file(common::get(args, "aad-file"));
            else if (common::has(args, "aad-text")) {
                auto s = common::get(args, "aad-text");
                aad.assign(s.begin(), s.end());
            }

            // Nonce-reuse guard for stream/AEAD modes
            if (mode == lab1::AesMode::CTR ||
                mode == lab1::AesMode::CCM ||
                mode == lab1::AesMode::GCM)
            {
                lab1::check_and_record_nonce(
                    lab1::bytes_to_hex(key), lab1::bytes_to_hex(iv_in));
            }

            auto result = lab1::aes_encrypt(mode, key, plaintext, iv_in, aad);

            std::string out = common::get(args, "out", "out.enc");
            write_file(out, encode_data(result.ciphertext, encode));

            // Write sidecar
            lab1::SidecarMeta meta;
            meta.alg  = "AES-256";
            meta.mode = mode_str;
            meta.iv   = result.iv;
            meta.aad  = aad;
            meta.tag  = result.tag;
            lab1::sidecar_write(out, meta);

            std::cout << "Encrypted -> " << out << "\n";
            std::cout << "Sidecar   -> " << out << ".hdr.json\n";
            if (!result.iv.empty())
                std::cout << "IV  : " << lab1::bytes_to_hex(result.iv) << "\n";
            if (!result.tag.empty())
                std::cout << "Tag : " << lab1::bytes_to_hex(result.tag) << "\n";

        // ── decrypt ─────────────────────────────────────────────────────
        } else if (args.command == "decrypt") {
            auto key = get_key(args);

            std::string in_path = common::get(args, "in", "");
            if (in_path.empty())
                throw std::runtime_error("Thiếu --in <file>");

            // Try to auto-load sidecar
            bool has_sidecar = false;
            lab1::SidecarMeta meta;
            try { meta = lab1::sidecar_read(in_path); has_sidecar = true; }
            catch (...) {}

            std::string mode_str = common::get(args, "mode",
                has_sidecar ? meta.mode : "gcm");
            auto mode = lab1::mode_from_string(mode_str);

            std::vector<uint8_t> iv, tag, aad;

            if (common::has(args, "iv"))
                iv = lab1::hex_to_bytes(common::get(args, "iv"));
            else if (has_sidecar)
                iv = meta.iv;
            else if (mode != lab1::AesMode::ECB)
                throw std::runtime_error(
                    "Thiếu --iv (sidecar không tìm thấy tại " + in_path + ".hdr.json)");

            lab1::validate_iv(mode_str, iv);

            if (common::has(args, "tag"))
                tag = lab1::hex_to_bytes(common::get(args, "tag"));
            else if (has_sidecar)
                tag = meta.tag;

            if (common::has(args, "aad-file"))
                aad = read_file(common::get(args, "aad-file"));
            else if (common::has(args, "aad-text")) {
                auto s = common::get(args, "aad-text");
                aad.assign(s.begin(), s.end());
            } else if (has_sidecar)
                aad = meta.aad;

            auto raw        = read_file(in_path);
            auto ciphertext = decode_data(raw, encode);

            auto plaintext = lab1::aes_decrypt(mode, key, ciphertext, iv, tag, aad);

            std::string out = common::get(args, "out", "out.dec");
            write_file(out, plaintext);
            std::cout << "Decrypted -> " << out << "\n";

        // ── kat ─────────────────────────────────────────────────────────
        } else if (args.command == "kat") {
            if (args.positional.empty())
                throw std::runtime_error("Usage: aestool kat <vectors.json> [...]");
            int total_fail = 0;
            for (auto& p : args.positional) {
                try { run_kat_file(p); }
                catch (const std::exception& e) {
                    std::cerr << e.what() << "\n";
                    ++total_fail;
                }
            }
            if (total_fail > 0)
                return 1;

        // ── benchmark ───────────────────────────────────────────────────
        } else if (args.command == "benchmark") {
            std::vector<uint8_t> key;
            if (common::has(args, "key-hex"))
                key = lab1::hex_to_bytes(common::get(args, "key-hex"));
            else if (common::has(args, "key-file"))
                key = lab1::load_key_hex(common::get(args, "key-file"));
            else if (common::has(args, "key"))
                key = lab1::load_key_auto(common::get(args, "key"));
            else
                key = lab1::keygen_aes256();
            run_benchmark(key);

        } else {
            print_usage();
        }

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
