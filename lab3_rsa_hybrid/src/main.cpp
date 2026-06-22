// rsatool — RSA-OAEP + Hybrid AES-256-GCM encryption CLI (Lab 3)
#include "rsa_oaep.hpp"
#include "hybrid.hpp"

#include <common/cli_parser.hpp>
#include <common/file_io.hpp>
#include <common/codec.hpp>

#include <nlohmann/json.hpp>

#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

using json = nlohmann::json;

// ── Utility ───────────────────────────────────────────────────────────────────

static std::string iso_timestamp() {
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    struct tm tm_buf{};
#ifdef _WIN32
    gmtime_s(&tm_buf, &t);
#else
    gmtime_r(&t, &tm_buf);
#endif
    std::ostringstream ss;
    ss << std::put_time(&tm_buf, "%Y-%m-%dT%H:%M:%SZ");
    return ss.str();
}

static std::string stem(const std::string& path) {
    auto slash = path.find_last_of("/\\");
    std::string name = (slash == std::string::npos) ? path : path.substr(slash + 1);
    auto dot = name.rfind('.');
    return (dot == std::string::npos) ? name : name.substr(0, dot);
}

static void print_usage() {
    std::cerr <<
        "rsatool — RSA-OAEP + Hybrid AES-256-GCM encryption (Lab 3)\n\n"
        "Commands:\n"
        "  keygen   --bits 3072|4096 --pub pub.pem --priv priv.pem\n"
        "  encrypt  --in msg.bin --pub pub.pem --out ct.bin [--label L]\n"
        "  decrypt  --in ct.bin --priv priv.pem --out msg.bin [--label L]\n"
        "  kat      [vectors.json]   (default: kat/rsa_oaep.json)\n"
        "  bench    [--bits 3072|4096]\n\n"
        "Notes:\n"
        "  encrypt auto-selects direct RSA-OAEP or hybrid based on message size.\n"
        "  Key files: .pem (PEM), .der (binary DER), .meta.json (metadata).\n";
}

// ── keygen ────────────────────────────────────────────────────────────────────

static int cmd_keygen(const common::CliArgs& a) {
    int bits = std::stoi(common::get(a, "bits", "3072"));
    std::string pub_path  = common::get(a, "pub",  "pub.pem");
    std::string priv_path = common::get(a, "priv", "priv.pem");

    std::cerr << "Generating RSA-" << bits << " key pair (may take a few seconds)...\n";
    auto kp = rsa3::keygen(bits);
    std::cerr << "Done.\n";

    common::write_text(priv_path, rsa3::pem_encode_private(kp.priv_der));
    common::write_text(pub_path,  rsa3::pem_encode_public (kp.pub_der));
    common::write_binary(stem(priv_path) + ".der", kp.priv_der);
    common::write_binary(stem(pub_path)  + ".der", kp.pub_der);

    json meta;
    meta["creation_time"] = iso_timestamp();
    meta["modulus_bits"]  = bits;
    meta["hash"]          = "SHA-256";
    meta["algorithm"]     = "RSA-OAEP";
    common::write_text(stem(priv_path) + ".meta.json", meta.dump(2) + "\n");

    std::cout << "Private key : " << priv_path << " / " << stem(priv_path) << ".der\n"
              << "Public  key : " << pub_path  << " / " << stem(pub_path)  << ".der\n"
              << "Metadata    : " << stem(priv_path) << ".meta.json\n";
    return 0;
}

// ── encrypt ───────────────────────────────────────────────────────────────────

static int cmd_encrypt(const common::CliArgs& a) {
    std::string in_path  = common::get(a, "in",  "");
    std::string pub_path = common::get(a, "pub", "pub.pem");
    std::string out_path = common::get(a, "out", "");
    std::string label    = common::get(a, "label", "");

    if (in_path.empty() || out_path.empty()) {
        std::cerr << "encrypt: --in and --out are required\n"; return 1;
    }

    auto plain   = common::read_binary(in_path);
    auto pub_der = rsa3::pem_decode(common::read_text(pub_path));
    int  bits    = rsa3::get_modulus_bits(pub_der);
    bool hybrid  = plain.size() > rsa3::oaep_max_len(bits);

    std::cerr << "Input : " << plain.size() << " bytes | RSA-" << bits
              << " | " << (hybrid ? "Hybrid RSA-OAEP-AES-256-GCM" : "RSA-OAEP direct") << "\n";

    auto env  = rsa3::encrypt(pub_der, plain, label);
    auto data = rsa3::serialize(env);
    common::write_binary(out_path, data);
    std::cout << "Encrypted: " << out_path << " (" << data.size() << " bytes)\n";
    return 0;
}

// ── decrypt ───────────────────────────────────────────────────────────────────

static int cmd_decrypt(const common::CliArgs& a) {
    std::string in_path   = common::get(a, "in",   "");
    std::string priv_path = common::get(a, "priv", "priv.pem");
    std::string out_path  = common::get(a, "out",  "");
    std::string label     = common::get(a, "label", "");

    if (in_path.empty() || out_path.empty()) {
        std::cerr << "decrypt: --in and --out are required\n"; return 1;
    }

    auto data     = common::read_binary(in_path);
    auto priv_der = rsa3::pem_decode(common::read_text(priv_path));
    auto env      = rsa3::deserialize(data);
    auto plain    = rsa3::decrypt(priv_der, env, label);
    common::write_binary(out_path, plain);
    std::cout << "Decrypted: " << out_path << " (" << plain.size() << " bytes)\n";
    return 0;
}

// ── kat ───────────────────────────────────────────────────────────────────────

static int cmd_kat(const common::CliArgs& a) {
    std::string vec_path = a.positional.empty() ? "kat/rsa_oaep.json" : a.positional[0];
    json j;
    try { j = json::parse(common::read_text(vec_path)); }
    catch (...) { std::cerr << "KAT: cannot parse " << vec_path << "\n"; return 1; }

    int passed = 0, failed = 0;
    for (auto& test : j.at("tests")) {
        std::string name  = test.value("name",  "?");
        std::string type  = test.value("type",  "roundtrip");
        int  bits         = test.value("modulus_bits", 3072);
        std::string label = test.value("label", "");

        try {
            if (type == "roundtrip" || type == "hybrid_roundtrip") {
                auto kp = rsa3::keygen(bits);
                std::vector<uint8_t> plain;
                if (test.contains("plaintext_hex"))
                    plain = common::hex_decode(test["plaintext_hex"].get<std::string>());
                else
                    plain.assign(test.value("payload_size", 64), (uint8_t)0xAB);

                auto env       = rsa3::encrypt(kp.pub_der, plain, label);
                auto recovered = rsa3::decrypt(kp.priv_der, env, label);
                if (recovered != plain) throw std::runtime_error("roundtrip mismatch");
                std::cout << "[PASS] " << name << "\n"; ++passed;

            } else if (type == "negative_wrong_key") {
                auto kp1 = rsa3::keygen(bits);
                auto kp2 = rsa3::keygen(bits);
                auto pt  = common::hex_decode(test["plaintext_hex"].get<std::string>());
                auto ct  = rsa3::oaep_encrypt(kp1.pub_der, pt);
                bool threw = false;
                try { rsa3::oaep_decrypt(kp2.priv_der, ct); } catch (...) { threw = true; }
                if (!threw) throw std::runtime_error("expected throw, got none");
                std::cout << "[PASS] " << name << "\n"; ++passed;

            } else if (type == "negative_tampered_ct") {
                auto kp  = rsa3::keygen(bits);
                auto pt  = common::hex_decode(test["plaintext_hex"].get<std::string>());
                auto env = rsa3::encrypt(kp.pub_der, pt, label);
                auto buf = rsa3::serialize(env);
                if (!buf.empty()) buf.back() ^= 0xFF;
                bool threw = false;
                try {
                    auto e2 = rsa3::deserialize(buf);
                    rsa3::decrypt(kp.priv_der, e2, label);
                } catch (...) { threw = true; }
                if (!threw) throw std::runtime_error("expected throw, got none");
                std::cout << "[PASS] " << name << "\n"; ++passed;

            } else {
                std::cout << "[SKIP] " << name << " (type=" << type << ")\n";
            }
        } catch (const std::exception& e) {
            std::cout << "[FAIL] " << name << " — " << e.what() << "\n"; ++failed;
        }
    }
    std::cout << "\nKAT: " << passed << " passed, " << failed << " failed\n";
    return (failed > 0) ? 1 : 0;
}

// ── bench ─────────────────────────────────────────────────────────────────────

static void bench_rsa(int bits) {
    using namespace std::chrono;
    const int NG = 5, NED = 20;
    std::cout << "\n--- RSA-" << bits << " ---\n";

    double kg_sum = 0;
    rsa3::RsaKeyPair kp;
    for (int i = 0; i < NG; ++i) {
        auto t0 = high_resolution_clock::now();
        kp = rsa3::keygen(bits);
        auto t1 = high_resolution_clock::now();
        kg_sum += duration<double, std::milli>(t1 - t0).count();
    }
    std::cout << std::fixed << std::setprecision(1)
              << "  Keygen  : " << kg_sum / NG << " ms  (N=" << NG << ")\n";

    std::vector<uint8_t> msg(16, 0xAA);
    double enc_sum = 0, dec_sum = 0;
    for (int i = 0; i < NED; ++i) {
        auto t0 = high_resolution_clock::now();
        auto ct = rsa3::oaep_encrypt(kp.pub_der, msg);
        auto t1 = high_resolution_clock::now();
        enc_sum += duration<double, std::milli>(t1 - t0).count();

        auto t2 = high_resolution_clock::now();
        rsa3::oaep_decrypt(kp.priv_der, ct);
        auto t3 = high_resolution_clock::now();
        dec_sum += duration<double, std::milli>(t3 - t2).count();
    }
    std::cout << "  Encrypt : " << enc_sum / NED << " ms  (N=" << NED << ")\n"
              << "  Decrypt : " << dec_sum / NED << " ms  (N=" << NED << ")\n";
}

static void bench_hybrid(int bits) {
    using namespace std::chrono;
    std::cout << "\n--- Hybrid RSA-" << bits << " + AES-256-GCM ---\n";
    auto kp = rsa3::keygen(bits);

    for (size_t sz : {size_t(1024), size_t(1) << 20, size_t(100) * 1024 * 1024}) {
        int N = (sz > (size_t(1) << 20)) ? 3 : 20;
        std::vector<uint8_t> plain(sz, 0xBB);
        double enc_sum = 0, dec_sum = 0;
        for (int i = 0; i < N; ++i) {
            auto t0 = high_resolution_clock::now();
            auto env  = rsa3::encrypt(kp.pub_der, plain);
            auto data = rsa3::serialize(env);
            auto t1 = high_resolution_clock::now();
            enc_sum += duration<double, std::milli>(t1 - t0).count();

            auto t2 = high_resolution_clock::now();
            auto e2 = rsa3::deserialize(data);
            rsa3::decrypt(kp.priv_der, e2);
            auto t3 = high_resolution_clock::now();
            dec_sum += duration<double, std::milli>(t3 - t2).count();
        }
        double em = enc_sum / N, dm = dec_sum / N;
        double tput = (sz / 1024.0 / 1024.0) / (em / 1000.0);
        std::cout << std::setprecision(1) << std::fixed
                  << "  " << std::setw(10) << sz << " B"
                  << "  enc=" << em << " ms"
                  << "  dec=" << dm << " ms"
                  << "  tput=" << tput << " MB/s"
                  << "  N=" << N << "\n";
    }
}

static int cmd_bench(const common::CliArgs& a) {
    std::string bits_str = common::get(a, "bits", "all");
    std::cout << "RSA-OAEP Benchmark\n==================\n";
    if (bits_str == "all" || bits_str == "3072") bench_rsa(3072);
    if (bits_str == "all" || bits_str == "4096") bench_rsa(4096);
    if (bits_str == "all" || bits_str == "3072") bench_hybrid(3072);
    if (bits_str == "all" || bits_str == "4096") bench_hybrid(4096);
    return 0;
}

// ── main ──────────────────────────────────────────────────────────────────────

int main(int argc, char** argv) {
    if (argc < 2) { print_usage(); return 1; }
    auto a = common::parse_args(argc, argv);
    std::string cmd = argv[1];
    try {
        if      (cmd == "keygen")  return cmd_keygen (a);
        else if (cmd == "encrypt") return cmd_encrypt(a);
        else if (cmd == "decrypt") return cmd_decrypt(a);
        else if (cmd == "kat")     return cmd_kat    (a);
        else if (cmd == "bench")   return cmd_bench  (a);
        else { print_usage(); return 1; }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}
