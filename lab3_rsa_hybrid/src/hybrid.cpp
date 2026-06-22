// hybrid.cpp — RSA-OAEP + AES-256-GCM hybrid envelope
#include "hybrid.hpp"
#include "rsa_oaep.hpp"

#include <cryptopp/aes.h>
#include <cryptopp/gcm.h>
#include <cryptopp/osrng.h>
#include <cryptopp/filters.h>

#include <nlohmann/json.hpp>
#include <common/codec.hpp>
#include <common/rng.hpp>

#include <stdexcept>
#include <string>
#include <algorithm>

using json = nlohmann::json;

namespace rsa3 {

// ── AES-256-GCM helpers ───────────────────────────────────────────────────────

constexpr size_t AES_KEY_LEN = 32;
constexpr size_t GCM_IV_LEN  = 12;
constexpr size_t GCM_TAG_LEN = 16;

struct GcmResult { std::vector<uint8_t> ct; std::vector<uint8_t> tag; };

static GcmResult gcm_encrypt(const std::vector<uint8_t>& key,
                               const std::vector<uint8_t>& iv,
                               const std::vector<uint8_t>& plain) {
    using namespace CryptoPP;
    GCM<AES>::Encryption enc;
    enc.SetKeyWithIV(key.data(), key.size(), iv.data(), iv.size());

    std::string combined;
    AuthenticatedEncryptionFilter aef(enc, new StringSink(combined), false,
                                      static_cast<int>(GCM_TAG_LEN));
    aef.ChannelPut(DEFAULT_CHANNEL, plain.data(), plain.size());
    aef.ChannelMessageEnd(DEFAULT_CHANNEL);

    // combined = ciphertext || tag
    GcmResult r;
    size_t ct_len = combined.size() - GCM_TAG_LEN;
    r.ct  = {combined.begin(), combined.begin() + ct_len};
    r.tag = {combined.begin() + ct_len, combined.end()};
    return r;
}

static std::vector<uint8_t> gcm_decrypt(const std::vector<uint8_t>& key,
                                          const std::vector<uint8_t>& iv,
                                          const std::vector<uint8_t>& ct,
                                          const std::vector<uint8_t>& tag) {
    using namespace CryptoPP;
    std::vector<uint8_t> combined = ct;
    combined.insert(combined.end(), tag.begin(), tag.end());

    GCM<AES>::Decryption dec;
    dec.SetKeyWithIV(key.data(), key.size(), iv.data(), iv.size());

    try {
        std::string pt;
        AuthenticatedDecryptionFilter adf(dec, new StringSink(pt),
            AuthenticatedDecryptionFilter::DEFAULT_FLAGS,
            static_cast<int>(GCM_TAG_LEN));
        adf.ChannelPut(DEFAULT_CHANNEL, combined.data(), combined.size());
        adf.ChannelMessageEnd(DEFAULT_CHANNEL);
        return {pt.begin(), pt.end()};
    } catch (const CryptoPP::Exception& e) {
        throw std::runtime_error("AES-GCM authentication failed: " + std::string(e.what()));
    }
}

// ── Encrypt ───────────────────────────────────────────────────────────────────

Envelope encrypt(const std::vector<uint8_t>& pub_der,
                 const std::vector<uint8_t>& plain,
                 const std::string& label) {
    int bits = get_modulus_bits(pub_der);
    Envelope env;
    env.rsa_modulus = bits;
    env.label       = label;

    if (plain.size() <= oaep_max_len(bits)) {
        // Direct RSA-OAEP mode (small messages)
        env.mode   = "RSA-OAEP";
        env.rsa_ct = oaep_encrypt(pub_der, plain);
    } else {
        // Hybrid mode: AES-256-GCM data + RSA-OAEP wraps session key
        env.mode = "RSA-OAEP-AES-GCM";

        auto session_key = common::random_bytes(AES_KEY_LEN);
        auto iv          = common::random_bytes(GCM_IV_LEN);

        auto gcm = gcm_encrypt(session_key, iv, plain);

        env.wrapped_key = oaep_encrypt(pub_der, session_key);
        env.iv          = iv;
        env.tag         = gcm.tag;
        env.ciphertext  = gcm.ct;
    }
    return env;
}

// ── Decrypt ───────────────────────────────────────────────────────────────────

std::vector<uint8_t> decrypt(const std::vector<uint8_t>& priv_der,
                              const Envelope& env,
                              const std::string& label) {
    // Verify label matches
    if (env.label != label)
        throw std::runtime_error("Label mismatch (expected \""
            + label + "\", got \"" + env.label + "\")");

    if (env.mode == "RSA-OAEP") {
        return oaep_decrypt(priv_der, env.rsa_ct);
    } else if (env.mode == "RSA-OAEP-AES-GCM") {
        auto session_key = oaep_decrypt(priv_der, env.wrapped_key);
        return gcm_decrypt(session_key, env.iv, env.ciphertext, env.tag);
    }
    throw std::runtime_error("Unknown envelope mode: " + env.mode);
}

// ── Serialize / Deserialize ───────────────────────────────────────────────────

// Format: "RSA3ENV\n" + JSON_line + "\n---\n" + raw_ciphertext_bytes
// For RSA-OAEP (direct), ciphertext bytes = rsa_ct
// For hybrid, ciphertext bytes = AES-GCM ciphertext

std::vector<uint8_t> serialize(const Envelope& env) {
    json j;
    j["mode"]        = env.mode;
    j["rsa_modulus"] = env.rsa_modulus;
    j["hash"]        = "SHA-256";
    if (!env.label.empty()) j["label"] = env.label;

    std::vector<uint8_t> raw_ct;

    if (env.mode == "RSA-OAEP") {
        j["rsa_ct"] = common::base64_encode(env.rsa_ct);
        // no separate payload
    } else {
        j["wrapped_key"] = common::base64_encode(env.wrapped_key);
        j["iv"]          = common::base64_encode(env.iv);
        j["tag"]         = common::base64_encode(env.tag);
        raw_ct           = env.ciphertext;
    }

    std::string header = "RSA3ENV\n" + j.dump() + "\n---\n";
    std::vector<uint8_t> out(header.begin(), header.end());
    out.insert(out.end(), raw_ct.begin(), raw_ct.end());
    return out;
}

Envelope deserialize(const std::vector<uint8_t>& data) {
    // Find separator "\n---\n" after the JSON line
    const std::string SEP = "\n---\n";
    auto it = std::search(data.begin(), data.end(), SEP.begin(), SEP.end());
    if (it == data.end())
        throw std::runtime_error("Envelope parse error: separator not found");

    std::string header_str(data.begin(), it);
    // Find and skip "RSA3ENV\n"
    const std::string MAGIC = "RSA3ENV\n";
    if (header_str.substr(0, MAGIC.size()) != MAGIC)
        throw std::runtime_error("Envelope parse error: bad magic");
    std::string json_str = header_str.substr(MAGIC.size());

    json j;
    try { j = json::parse(json_str); }
    catch (const std::exception& e) {
        throw std::runtime_error(std::string("Envelope JSON parse error: ") + e.what());
    }

    Envelope env;
    env.mode        = j.at("mode").get<std::string>();
    env.rsa_modulus = j.at("rsa_modulus").get<int>();
    env.label       = j.value("label", "");

    auto payload_begin = it + static_cast<ptrdiff_t>(SEP.size());

    if (env.mode == "RSA-OAEP") {
        env.rsa_ct = common::base64_decode(j.at("rsa_ct").get<std::string>());
    } else if (env.mode == "RSA-OAEP-AES-GCM") {
        env.wrapped_key = common::base64_decode(j.at("wrapped_key").get<std::string>());
        env.iv          = common::base64_decode(j.at("iv").get<std::string>());
        env.tag         = common::base64_decode(j.at("tag").get<std::string>());
        env.ciphertext  = {payload_begin, data.end()};
    } else {
        throw std::runtime_error("Unknown envelope mode: " + env.mode);
    }
    return env;
}

} // namespace rsa3
