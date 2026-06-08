#include "aes_service.hpp"
#include <common/rng.hpp>
#include <stdexcept>
#include <algorithm>

// Crypto++ headers
#include <cryptopp/aes.h>
#include <cryptopp/modes.h>   // ECB/CBC/CFB/OFB/CTR
#include <cryptopp/xts.h>
#include <cryptopp/ccm.h>
#include <cryptopp/gcm.h>
#include <cryptopp/filters.h>
#include <cryptopp/osrng.h>

using namespace CryptoPP;

namespace lab1 {

// ─── Helpers ──────────────────────────────────────────────────────────────────

static std::vector<uint8_t> make_iv(std::size_t n) {
    return common::random_bytes(n);
}

static void check_key(const std::vector<uint8_t>& key) {
    if (key.size() != 32)
        throw std::runtime_error("Key phải đúng 32 bytes (AES-256)");
}

AesMode mode_from_string(const std::string& s) {
    std::string lc = s;
    std::transform(lc.begin(), lc.end(), lc.begin(), ::tolower);
    if (lc == "ecb") return AesMode::ECB;
    if (lc == "cbc") return AesMode::CBC;
    if (lc == "cfb") return AesMode::CFB;
    if (lc == "ofb") return AesMode::OFB;
    if (lc == "ctr") return AesMode::CTR;
    if (lc == "xts") return AesMode::XTS;
    if (lc == "ccm") return AesMode::CCM;
    if (lc == "gcm") return AesMode::GCM;
    throw std::runtime_error("Mode không hợp lệ: " + s);
}

// ─── Encrypt ──────────────────────────────────────────────────────────────────

AesResult aes_encrypt(AesMode mode,
                      const std::vector<uint8_t>& key,
                      const std::vector<uint8_t>& plaintext,
                      const std::vector<uint8_t>& iv_in,
                      const std::vector<uint8_t>& aad,
                      bool no_padding)
{
    check_key(key);

    // Validate user-supplied IV before use
    if (!iv_in.empty()) {
        std::size_t expected = 0;
        switch (mode) {
        case AesMode::ECB: throw std::runtime_error("ECB does not use an IV"); break;
        case AesMode::GCM: expected = 12; break;
        case AesMode::CCM: expected = 11; break;
        default: expected = 16; break;
        }
        if (iv_in.size() != expected)
            throw std::runtime_error(
                "IV length wrong: expected " + std::to_string(expected) +
                " bytes, got " + std::to_string(iv_in.size()));
    }

    AesResult res;
    std::string ct_str;

    const byte* kp = key.data();
    const byte* pp = plaintext.data();
    std::size_t  ps = plaintext.size();

    auto padding = no_padding
        ? StreamTransformationFilter::NO_PADDING
        : StreamTransformationFilter::DEFAULT_PADDING;

    auto run_stream = [&](auto& enc, const std::vector<uint8_t>& iv) {
        enc.SetKeyWithIV(kp, key.size(), iv.data(), iv.size());
        StringSource ss(pp, ps, true,
            new StreamTransformationFilter(enc, new StringSink(ct_str), padding));
        res.iv = iv;
    };

    switch (mode) {
    case AesMode::ECB: {
        // ECB: không dùng IV; mỗi block độc lập — chỉ dùng để demo KAT
        ECB_Mode<AES>::Encryption enc;
        enc.SetKey(kp, key.size());
        StringSource ss(pp, ps, true,
            new StreamTransformationFilter(enc, new StringSink(ct_str)));
        break;
    }
    case AesMode::CBC: {
        auto iv = iv_in.empty() ? make_iv(AES::BLOCKSIZE) : iv_in;
        CBC_Mode<AES>::Encryption enc;
        run_stream(enc, iv);
        break;
    }
    case AesMode::CFB: {
        auto iv = iv_in.empty() ? make_iv(AES::BLOCKSIZE) : iv_in;
        CFB_Mode<AES>::Encryption enc;
        run_stream(enc, iv);
        break;
    }
    case AesMode::OFB: {
        auto iv = iv_in.empty() ? make_iv(AES::BLOCKSIZE) : iv_in;
        OFB_Mode<AES>::Encryption enc;
        run_stream(enc, iv);
        break;
    }
    case AesMode::CTR: {
        // CTR: IV = 16-byte nonce (high 8B nonce | low 8B counter=0)
        auto iv = iv_in.empty() ? make_iv(AES::BLOCKSIZE) : iv_in;
        CTR_Mode<AES>::Encryption enc;
        run_stream(enc, iv);
        break;
    }
    case AesMode::XTS: {
        // XTS cần key gấp đôi (2×256 = 512-bit) và sector IV 16B
        // Đây là trường hợp đặc biệt: dùng key làm half-key (demo)
        if (key.size() != 32)
            throw std::runtime_error("XTS cần key 32 bytes (2×128-bit tweak+cipher)");
        auto iv = iv_in.empty() ? make_iv(AES::BLOCKSIZE) : iv_in;
        // XTS_Mode<AES>::Encryption cần key 2×keylen
        std::vector<uint8_t> xts_key(key.begin(), key.end());
        xts_key.insert(xts_key.end(), key.begin(), key.end()); // demo: duplicate
        XTS_Mode<AES>::Encryption enc;
        enc.SetKeyWithIV(xts_key.data(), xts_key.size(), iv.data(), iv.size());
        StringSource ss(pp, ps, true,
            new StreamTransformationFilter(enc, new StringSink(ct_str)));
        res.iv = iv;
        break;
    }
    case AesMode::CCM: {
        // CCM: tag 16B, nonce 11B (NIST khuyến nghị)
        auto iv = iv_in.empty() ? make_iv(11) : iv_in;
        CCM<AES, 16>::Encryption enc;
        enc.SetKeyWithIV(kp, key.size(), iv.data(), iv.size());
        enc.SpecifyDataLengths(aad.size(), ps, 0);
        AuthenticatedEncryptionFilter ef(enc, new StringSink(ct_str), false, 16);
        // AAD phải vào trước plaintext
        if (!aad.empty()) {
            ef.ChannelPut(AAD_CHANNEL, aad.data(), aad.size());
            ef.ChannelMessageEnd(AAD_CHANNEL);
        }
        // Dùng StringSource + Redirector để pump plaintext (tránh lỗi ordering)
        std::string pt_tmp(reinterpret_cast<const char*>(pp), ps);
        StringSource(pt_tmp, true, new Redirector(ef));
        std::size_t tag_len = 16;
        res.tag = std::vector<uint8_t>(ct_str.end() - tag_len, ct_str.end());
        ct_str.resize(ct_str.size() - tag_len);
        res.iv = iv;
        break;
    }
    case AesMode::GCM: {
        // GCM: tag 16B, nonce 12B (96-bit — NIST khuyến nghị)
        auto iv = iv_in.empty() ? make_iv(12) : iv_in;
        GCM<AES>::Encryption enc;
        enc.SetKeyWithIV(kp, key.size(), iv.data(), iv.size());
        AuthenticatedEncryptionFilter ef(enc, new StringSink(ct_str), false, 16);
        if (!aad.empty()) {
            ef.ChannelPut(AAD_CHANNEL, aad.data(), aad.size());
            ef.ChannelMessageEnd(AAD_CHANNEL);
        }
        std::string pt_tmp(reinterpret_cast<const char*>(pp), ps);
        StringSource(pt_tmp, true, new Redirector(ef));
        std::size_t tag_len = 16;
        res.tag = std::vector<uint8_t>(ct_str.end() - tag_len, ct_str.end());
        ct_str.resize(ct_str.size() - tag_len);
        res.iv = iv;
        break;
    }
    }

    res.ciphertext.assign(ct_str.begin(), ct_str.end());
    return res;
}

// ─── Decrypt ──────────────────────────────────────────────────────────────────

std::vector<uint8_t> aes_decrypt(AesMode mode,
                                 const std::vector<uint8_t>& key,
                                 const std::vector<uint8_t>& ciphertext,
                                 const std::vector<uint8_t>& iv,
                                 const std::vector<uint8_t>& tag,
                                 const std::vector<uint8_t>& aad,
                                 bool no_padding)
{
    check_key(key);

    // Validate IV length
    if (!iv.empty()) {
        std::size_t expected = 0;
        switch (mode) {
        case AesMode::ECB: throw std::runtime_error("ECB does not use an IV"); break;
        case AesMode::GCM: expected = 12; break;
        case AesMode::CCM: expected = 11; break;
        default: expected = 16; break;
        }
        if (iv.size() != expected)
            throw std::runtime_error(
                "IV length wrong: expected " + std::to_string(expected) +
                " bytes, got " + std::to_string(iv.size()));
    }

    std::string pt_str;
    const byte* kp  = key.data();
    const byte* cp  = ciphertext.data();
    std::size_t  cs = ciphertext.size();

    auto padding = no_padding
        ? StreamTransformationFilter::NO_PADDING
        : StreamTransformationFilter::DEFAULT_PADDING;

    auto run_stream_dec = [&](auto& dec) {
        dec.SetKeyWithIV(kp, key.size(), iv.data(), iv.size());
        StringSource ss(cp, cs, true,
            new StreamTransformationFilter(dec, new StringSink(pt_str), padding));
    };

    switch (mode) {
    case AesMode::ECB: {
        ECB_Mode<AES>::Decryption dec;
        dec.SetKey(kp, key.size());
        StringSource ss(cp, cs, true,
            new StreamTransformationFilter(dec, new StringSink(pt_str)));
        break;
    }
    case AesMode::CBC: { CBC_Mode<AES>::Decryption dec; run_stream_dec(dec); break; }
    case AesMode::CFB: { CFB_Mode<AES>::Decryption dec; run_stream_dec(dec); break; }
    case AesMode::OFB: { OFB_Mode<AES>::Decryption dec; run_stream_dec(dec); break; }
    case AesMode::CTR: { CTR_Mode<AES>::Decryption dec; run_stream_dec(dec); break; }
    case AesMode::XTS: {
        std::vector<uint8_t> xts_key(key.begin(), key.end());
        xts_key.insert(xts_key.end(), key.begin(), key.end());
        XTS_Mode<AES>::Decryption dec;
        dec.SetKeyWithIV(xts_key.data(), xts_key.size(), iv.data(), iv.size());
        StringSource ss(cp, cs, true,
            new StreamTransformationFilter(dec, new StringSink(pt_str)));
        break;
    }
    case AesMode::CCM: {
        std::string combined(cp, cp + cs);
        combined.append(tag.begin(), tag.end());
        CCM<AES, 16>::Decryption dec;
        dec.SetKeyWithIV(kp, key.size(), iv.data(), iv.size());
        dec.SpecifyDataLengths(aad.size(), cs, 0);
        // DEFAULT_FLAGS includes THROW_EXCEPTION — tag fail → exception (fail-closed)
        AuthenticatedDecryptionFilter df(dec, new StringSink(pt_str),
            AuthenticatedDecryptionFilter::DEFAULT_FLAGS, 16);
        if (!aad.empty()) {
            df.ChannelPut(AAD_CHANNEL, aad.data(), aad.size());
            df.ChannelMessageEnd(AAD_CHANNEL);
        }
        StringSource(combined, true, new Redirector(df));
        break;
    }
    case AesMode::GCM: {
        std::string combined(cp, cp + cs);
        combined.append(tag.begin(), tag.end());
        GCM<AES>::Decryption dec;
        dec.SetKeyWithIV(kp, key.size(), iv.data(), iv.size());
        AuthenticatedDecryptionFilter df(dec, new StringSink(pt_str),
            AuthenticatedDecryptionFilter::DEFAULT_FLAGS, 16);
        if (!aad.empty()) {
            df.ChannelPut(AAD_CHANNEL, aad.data(), aad.size());
            df.ChannelMessageEnd(AAD_CHANNEL);
        }
        StringSource(combined, true, new Redirector(df));
        break;
    }
    }

    return std::vector<uint8_t>(pt_str.begin(), pt_str.end());
}

} // namespace lab1
