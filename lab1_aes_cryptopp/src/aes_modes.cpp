#include "aes_modes.h"

#include <cryptopp/aes.h>
#include <cryptopp/modes.h>  
#include <cryptopp/xts.h>        
#include <cryptopp/gcm.h>    
#include <cryptopp/ccm.h>       
#include <cryptopp/filters.h>    
#include <cryptopp/osrng.h>     
#include <cryptopp/secblock.h>

using namespace CryptoPP;

namespace aes1 {
Mode parse_mode(const std::string& s) {
    if (s == "ecb") return Mode::ECB;
    if (s == "cbc") return Mode::CBC;
    if (s == "ofb") return Mode::OFB;
    if (s == "cfb") return Mode::CFB;
    if (s == "ctr") return Mode::CTR;
    if (s == "xts") return Mode::XTS;
    if (s == "ccm") return Mode::CCM;
    if (s == "gcm") return Mode::GCM;
    throw std::runtime_error("Unknown mode: '" + s + "'. Use: ecb|cbc|ofb|cfb|ctr|xts|ccm|gcm");
}

std::string mode_name(Mode m) {
    switch (m) {
        case Mode::ECB: return "AES-ECB";
        case Mode::CBC: return "AES-CBC";
        case Mode::OFB: return "AES-OFB";
        case Mode::CFB: return "AES-CFB";
        case Mode::CTR: return "AES-CTR";
        case Mode::XTS: return "AES-XTS";
        case Mode::CCM: return "AES-CCM";
        case Mode::GCM: return "AES-GCM";
    }
    return "AES-UNKNOWN";
}

size_t required_iv_len(Mode m) {
    switch (m) {
        case Mode::ECB: return 0;   
        case Mode::GCM: return GCM_IV;
        case Mode::CCM: return CCM_IV;
        default:        return BLOCK; 
    }
}
std::vector<uint8_t> generate_iv(size_t len) {
    AutoSeededRandomPool rng;
    std::vector<uint8_t> iv(len);
    rng.GenerateBlock(iv.data(), len);
    return iv;
}

static std::vector<uint8_t> resolve_iv(Mode m, const std::vector<uint8_t>& iv) {
    size_t req = required_iv_len(m);
    if (req == 0) return {};  
    if (iv.empty()) return generate_iv(req);
    if (iv.size() != req)
        throw std::runtime_error("IV length " + std::to_string(iv.size()) +
            " is wrong for mode " + mode_name(m) +
            " (expected " + std::to_string(req) + " bytes)");
    return iv;
}

static void check_key(Mode m, const std::vector<uint8_t>& key) {
    size_t ks = key.size();
    if (m == Mode::XTS) {
        if (ks != 32 && ks != 48 && ks != 64)
            throw std::runtime_error("XTS key must be 32/48/64 bytes (2× AES-128/192/256), got " + std::to_string(ks));
        return;
    }
    if (ks != 16 && ks != 24 && ks != 32)
        throw std::runtime_error("Key must be 16/24/32 bytes (AES-128/192/256), got " + std::to_string(ks));
}

template<class ENC>
static std::string stream_enc(ENC& e, const std::vector<uint8_t>& pt) {
    std::string ct;
    StreamTransformationFilter f(e, new StringSink(ct));
    f.Put(pt.data(), pt.size());
    f.MessageEnd();
    return ct;
}

template<class DEC>
static std::string stream_dec(DEC& d, const std::vector<uint8_t>& ct) {
    std::string pt;
    StreamTransformationFilter f(d, new StringSink(pt));
    f.Put(ct.data(), ct.size());
    f.MessageEnd();
    return pt;
}

CipherResult encrypt(Mode m,
                     const std::vector<uint8_t>& key,
                     const std::vector<uint8_t>& iv_in,
                     const std::vector<uint8_t>& plain,
                     const std::vector<uint8_t>& aad)
{
    check_key(m, key);
    auto iv = resolve_iv(m, iv_in);
    CipherResult res;
    res.iv = iv;

    switch (m) {
    case Mode::ECB: {
        if (plain.size() % BLOCK != 0)
            throw std::runtime_error("ECB: plaintext must be multiple of 16 bytes (got "
                + std::to_string(plain.size()) + ").");
        ECB_Mode<AES>::Encryption e;
        e.SetKey(key.data(), key.size());
        std::string ct;
        StreamTransformationFilter f(e, new StringSink(ct), StreamTransformationFilter::NO_PADDING);
        f.Put(plain.data(), plain.size());
        f.MessageEnd();
        res.ciphertext = {ct.begin(), ct.end()};
        break;
    }

    case Mode::CBC: {
        if (plain.size() % BLOCK != 0)
            throw std::runtime_error("CBC: plaintext must be multiple of 16 bytes (got "
                + std::to_string(plain.size()) + "). Use CTR/GCM for arbitrary-length data.");
        CBC_Mode<AES>::Encryption e;
        e.SetKeyWithIV(key.data(), key.size(), iv.data());
        std::string ct;
        StreamTransformationFilter f(e, new StringSink(ct), StreamTransformationFilter::NO_PADDING);
        f.Put(plain.data(), plain.size());
        f.MessageEnd();
        res.ciphertext = {ct.begin(), ct.end()};
        break;
    }

    case Mode::OFB: {
        OFB_Mode<AES>::Encryption e;
        e.SetKeyWithIV(key.data(), key.size(), iv.data());
        std::string ct;
        StreamTransformationFilter f(e, new StringSink(ct),
            StreamTransformationFilter::NO_PADDING);
        f.Put(plain.data(), plain.size());
        f.MessageEnd();
        res.ciphertext = {ct.begin(), ct.end()};
        break;
    }

    case Mode::CFB: {
        CFB_Mode<AES>::Encryption e;
        e.SetKeyWithIV(key.data(), key.size(), iv.data());
        std::string ct;
        StreamTransformationFilter f(e, new StringSink(ct),
            StreamTransformationFilter::NO_PADDING);
        f.Put(plain.data(), plain.size());
        f.MessageEnd();
        res.ciphertext = {ct.begin(), ct.end()};
        break;
    }
  
    case Mode::CTR: {
        CTR_Mode<AES>::Encryption e;
        e.SetKeyWithIV(key.data(), key.size(), iv.data());
        std::string ct;
        StreamTransformationFilter f(e, new StringSink(ct),
            StreamTransformationFilter::NO_PADDING);
        f.Put(plain.data(), plain.size());
        f.MessageEnd();
        res.ciphertext = {ct.begin(), ct.end()};
        break;
    }

    case Mode::XTS: {
        XTS_Mode<AES>::Encryption e;
        e.SetKeyWithIV(key.data(), key.size(), iv.data());
        std::string ct;
        StreamTransformationFilter f(e, new StringSink(ct),
            StreamTransformationFilter::NO_PADDING);
        f.Put(plain.data(), plain.size());
        f.MessageEnd();
        res.ciphertext = {ct.begin(), ct.end()};
        break;
    }

    case Mode::CCM: {
        CCM<AES, TAG_LEN>::Encryption e;
        e.SetKeyWithIV(key.data(), key.size(), iv.data(), iv.size());
        e.SpecifyDataLengths(aad.size(), plain.size(), 0);

        std::string ct;
        AuthenticatedEncryptionFilter aef(e, new StringSink(ct));
        if (!aad.empty()) {
            aef.ChannelPut(AAD_CHANNEL, aad.data(), aad.size());
            aef.ChannelMessageEnd(AAD_CHANNEL);
        }
        aef.ChannelPut(DEFAULT_CHANNEL, plain.data(), plain.size());
        aef.ChannelMessageEnd(DEFAULT_CHANNEL);

        if (ct.size() < TAG_LEN)
            throw std::runtime_error("CCM output too short");
        res.ciphertext = {ct.begin(), ct.end() - TAG_LEN};
        res.tag        = {ct.end() - TAG_LEN, ct.end()};
        break;
    }

    case Mode::GCM: {
        GCM<AES>::Encryption e;
        e.SetKeyWithIV(key.data(), key.size(), iv.data(), iv.size());

        std::string ct;
        AuthenticatedEncryptionFilter aef(e, new StringSink(ct),
            false, TAG_LEN);
        if (!aad.empty()) {
            aef.ChannelPut(AAD_CHANNEL, aad.data(), aad.size());
            aef.ChannelMessageEnd(AAD_CHANNEL);
        }
        aef.ChannelPut(DEFAULT_CHANNEL, plain.data(), plain.size());
        aef.ChannelMessageEnd(DEFAULT_CHANNEL);

        if (ct.size() < TAG_LEN)
            throw std::runtime_error("GCM output too short");
        res.ciphertext = {ct.begin(), ct.end() - TAG_LEN};
        res.tag        = {ct.end() - TAG_LEN, ct.end()};
        break;
    }

    }
    return res;
}

std::vector<uint8_t> decrypt(Mode m,
                              const std::vector<uint8_t>& key,
                              const std::vector<uint8_t>& iv,
                              const std::vector<uint8_t>& cipher,
                              const std::vector<uint8_t>& tag,
                              const std::vector<uint8_t>& aad)
{
    check_key(m, key);

    if (m != Mode::ECB) {
        size_t req = required_iv_len(m);
        if (iv.size() != req)
            throw std::runtime_error("IV length " + std::to_string(iv.size()) +
                " wrong for decrypt (expected " + std::to_string(req) + ")");
    }

    switch (m) {

    case Mode::ECB: {
        ECB_Mode<AES>::Decryption d;
        d.SetKey(key.data(), key.size());
        std::string pt;
        StreamTransformationFilter f(d, new StringSink(pt), StreamTransformationFilter::NO_PADDING);
        f.Put(cipher.data(), cipher.size());
        f.MessageEnd();
        return {pt.begin(), pt.end()};
    }

    case Mode::CBC: {
        CBC_Mode<AES>::Decryption d;
        d.SetKeyWithIV(key.data(), key.size(), iv.data());
        std::string pt;
        StreamTransformationFilter f(d, new StringSink(pt), StreamTransformationFilter::NO_PADDING);
        f.Put(cipher.data(), cipher.size());
        f.MessageEnd();
        return {pt.begin(), pt.end()};
    }

    case Mode::OFB: {
        OFB_Mode<AES>::Decryption d;
        d.SetKeyWithIV(key.data(), key.size(), iv.data());
        std::string pt;
        StreamTransformationFilter f(d, new StringSink(pt),
            StreamTransformationFilter::NO_PADDING);
        f.Put(cipher.data(), cipher.size());
        f.MessageEnd();
        return {pt.begin(), pt.end()};
    }

    case Mode::CFB: {
        CFB_Mode<AES>::Decryption d;
        d.SetKeyWithIV(key.data(), key.size(), iv.data());
        std::string pt;
        StreamTransformationFilter f(d, new StringSink(pt),
            StreamTransformationFilter::NO_PADDING);
        f.Put(cipher.data(), cipher.size());
        f.MessageEnd();
        return {pt.begin(), pt.end()};
    }

    case Mode::CTR: {
        CTR_Mode<AES>::Decryption d;
        d.SetKeyWithIV(key.data(), key.size(), iv.data());
        std::string pt;
        StreamTransformationFilter f(d, new StringSink(pt),
            StreamTransformationFilter::NO_PADDING);
        f.Put(cipher.data(), cipher.size());
        f.MessageEnd();
        return {pt.begin(), pt.end()};
    }

    case Mode::XTS: {
        XTS_Mode<AES>::Decryption d;
        d.SetKeyWithIV(key.data(), key.size(), iv.data());
        std::string pt;
        StreamTransformationFilter f(d, new StringSink(pt),
            StreamTransformationFilter::NO_PADDING);
        f.Put(cipher.data(), cipher.size());
        f.MessageEnd();
        return {pt.begin(), pt.end()};
    }

    case Mode::CCM: {
        std::vector<uint8_t> ct_with_tag = cipher;
        if (tag.size() != TAG_LEN)
            throw std::runtime_error("CCM: tag must be " + std::to_string(TAG_LEN) + " bytes");
        ct_with_tag.insert(ct_with_tag.end(), tag.begin(), tag.end());

        CCM<AES, TAG_LEN>::Decryption d;
        d.SetKeyWithIV(key.data(), key.size(), iv.data(), iv.size());
        d.SpecifyDataLengths(aad.size(), cipher.size(), 0);

        try {
            std::string pt;
            AuthenticatedDecryptionFilter adf(d, new StringSink(pt));
            if (!aad.empty()) {
                adf.ChannelPut(AAD_CHANNEL, aad.data(), aad.size());
                adf.ChannelMessageEnd(AAD_CHANNEL);
            }
            adf.ChannelPut(DEFAULT_CHANNEL, ct_with_tag.data(), ct_with_tag.size());
            adf.ChannelMessageEnd(DEFAULT_CHANNEL);
            return {pt.begin(), pt.end()};
        } catch (const CryptoPP::Exception& e) {
            throw std::runtime_error(
                std::string("Authentication tag verification FAILED (CCM): ") + e.what() +
                "\n  Ciphertext, tag, or AAD has been tampered with. Decryption aborted.");
        }
    }

    case Mode::GCM: {
        std::vector<uint8_t> ct_with_tag = cipher;
        if (tag.size() != TAG_LEN)
            throw std::runtime_error("GCM: tag must be " + std::to_string(TAG_LEN) + " bytes");
        ct_with_tag.insert(ct_with_tag.end(), tag.begin(), tag.end());

        GCM<AES>::Decryption d;
        d.SetKeyWithIV(key.data(), key.size(), iv.data(), iv.size());

        try {
            std::string pt;
            AuthenticatedDecryptionFilter adf(d, new StringSink(pt),
                AuthenticatedDecryptionFilter::DEFAULT_FLAGS, TAG_LEN);
            if (!aad.empty()) {
                adf.ChannelPut(AAD_CHANNEL, aad.data(), aad.size());
                adf.ChannelMessageEnd(AAD_CHANNEL);
            }
            adf.ChannelPut(DEFAULT_CHANNEL, ct_with_tag.data(), ct_with_tag.size());
            adf.ChannelMessageEnd(DEFAULT_CHANNEL);
            return {pt.begin(), pt.end()};
        } catch (const CryptoPP::Exception& e) {
            throw std::runtime_error(
                std::string("Authentication tag verification FAILED (GCM): ") + e.what() +
                "\n  Ciphertext, tag, or AAD has been tampered with. Decryption aborted.");
        }
    }

    } 
    return {};
}

}