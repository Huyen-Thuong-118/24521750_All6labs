// pqtool — Lab 6 CLI
//
// pqtool keygen  mldsa44  --priv priv.pem --pub pub.pem
// pqtool keygen  mlkem512 --priv priv.pem --pub pub.pem
// pqtool sign    mldsa44  --priv priv.pem --in msg.bin --out sig.bin [--enc base64]
// pqtool verify  mldsa44  --pub pub.pem   --in msg.bin --sig sig.bin [--enc base64]
// pqtool encaps  mlkem512 --pub pub.pem   --ct ct.bin  --ss ss.bin
// pqtool decaps  mlkem512 --priv priv.pem --ct ct.bin  --ss ss.bin
// pqtool cert    issue    --ca-priv capriv.pem --sub-pub subpub.pem \
//                          --subject "CN=example" --issuer "CN=CA" --out cert.json
// pqtool cert    verify   --ca-pub capub.pem --cert cert.json

#include "pqtool.hpp"
#include <algorithm>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

static std::vector<uint8_t> read_file(const std::string& p) {
    std::ifstream f(p, std::ios::binary);
    if (!f) throw std::runtime_error("Cannot open: " + p);
    return {std::istreambuf_iterator<char>(f), {}};
}

static void write_file(const std::string& p, const std::vector<uint8_t>& d) {
    std::ofstream f(p, std::ios::binary);
    if (!f) throw std::runtime_error("Cannot write: " + p);
    f.write(reinterpret_cast<const char*>(d.data()),
            static_cast<std::streamsize>(d.size()));
}

static std::string read_text(const std::string& p) {
    std::ifstream f(p); if (!f) throw std::runtime_error("Cannot read: " + p);
    return {std::istreambuf_iterator<char>(f), {}};
}

static void write_text(const std::string& p, const std::string& s) {
    std::ofstream f(p); if (!f) throw std::runtime_error("Cannot write: " + p);
    f << s;
}

static std::string arg(int argc, char** argv, const char* flag, const char* def = "") {
    for (int i = 1; i < argc - 1; i++)
        if (std::strcmp(argv[i], flag) == 0) return argv[i+1];
    return def;
}

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "pqtool keygen|sign|verify|encaps|decaps|cert mldsa44|mlkem512|...\n";
        return 1;
    }
    try {
        std::string cmd  = argv[1];
        std::string algo = argv[2];

        if (cmd == "keygen") {
            auto priv_path = arg(argc, argv, "--priv", "priv.pem");
            auto pub_path  = arg(argc, argv, "--pub",  "pub.pem");

            if (algo == "mldsa44" || algo == "mldsa-44") {
                auto [priv, pub] = pq::mldsa_keygen();
                write_text(priv_path, priv); write_text(pub_path, pub);
            } else if (algo == "mlkem512" || algo == "mlkem-512") {
                auto [priv, pub] = pq::mlkem_keygen();
                write_text(priv_path, priv); write_text(pub_path, pub);
            } else {
                throw std::runtime_error("Unknown algo: " + algo);
            }
            std::cout << "Keys → " << priv_path << " / " << pub_path << "\n";

        } else if (cmd == "sign") {
            auto priv_pem = read_text(arg(argc, argv, "--priv", "priv.pem"));
            auto msg      = read_file(arg(argc, argv, "--in",   "msg.bin"));
            auto out      = arg(argc, argv, "--out", "sig.bin");
            auto enc      = arg(argc, argv, "--enc", "raw");
            auto sig      = pq::mldsa_sign(priv_pem, msg);
            if (std::string(enc) == "base64") write_text(out, pq::to_base64(sig));
            else                               write_file(out, sig);
            std::cout << "Signature written (" << sig.size() << " bytes)\n";

        } else if (cmd == "verify") {
            auto pub_pem  = read_text(arg(argc, argv, "--pub", "pub.pem"));
            auto msg      = read_file(arg(argc, argv, "--in",  "msg.bin"));
            auto sig_path = arg(argc, argv, "--sig", "sig.bin");
            auto enc      = arg(argc, argv, "--enc", "raw");

            std::vector<uint8_t> sig;
            if (std::string(enc) == "base64") {
                auto s = read_text(sig_path);
                s.erase(std::remove(s.begin(), s.end(), '\n'), s.end());
                s.erase(std::remove(s.begin(), s.end(), '\r'), s.end());
                sig = pq::from_base64(s);
            } else {
                sig = read_file(sig_path);
            }
            bool ok = pq::mldsa_verify(pub_pem, msg, sig);
            std::cout << (ok ? "VALID\n" : "INVALID\n");
            return ok ? 0 : 2;

        } else if (cmd == "encaps") {
            auto pub_pem = read_text(arg(argc, argv, "--pub", "pub.pem"));
            auto ct_path = arg(argc, argv, "--ct", "ct.bin");
            auto ss_path = arg(argc, argv, "--ss", "ss.bin");
            auto r = pq::mlkem_encaps(pub_pem);
            write_file(ct_path, r.ciphertext);
            write_file(ss_path, r.shared_secret);
            std::cout << "ct → " << ct_path << " (" << r.ciphertext.size()
                      << " B)  ss → " << ss_path << " (" << r.shared_secret.size() << " B)\n";

        } else if (cmd == "decaps") {
            auto priv_pem = read_text(arg(argc, argv, "--priv", "priv.pem"));
            auto ct       = read_file(arg(argc, argv, "--ct",   "ct.bin"));
            auto ss_path  = arg(argc, argv, "--ss", "ss.bin");
            auto ss = pq::mlkem_decaps(priv_pem, ct);
            write_file(ss_path, ss);
            std::cout << "ss → " << ss_path << " (" << ss.size() << " B)\n";

        } else if (cmd == "cert") {
            if (algo == "issue") {
                auto ca_priv  = read_text(arg(argc, argv, "--ca-priv", "capriv.pem"));
                auto sub_pub  = read_text(arg(argc, argv, "--sub-pub", "subpub.pem"));
                auto subject  = arg(argc, argv, "--subject", "CN=subject");
                auto issuer   = arg(argc, argv, "--issuer",  "CN=CA");
                auto out_path = arg(argc, argv, "--out",     "cert.json");
                auto cert = pq::cert_issue(ca_priv, subject, sub_pub, issuer);
                write_text(out_path, pq::cert_to_json(cert));
                std::cout << "Certificate written to " << out_path << "\n";

            } else if (algo == "verify") {
                auto ca_pub   = read_text(arg(argc, argv, "--ca-pub", "capub.pem"));
                auto cert_txt = read_text(arg(argc, argv, "--cert",   "cert.json"));
                auto cert = pq::cert_from_json(cert_txt);
                bool ok = pq::cert_verify(ca_pub, cert);
                std::cout << (ok ? "Certificate VALID\n" : "Certificate INVALID\n");
                return ok ? 0 : 2;
            } else {
                throw std::runtime_error("cert sub-command: issue | verify");
            }
        } else {
            throw std::runtime_error("Unknown command: " + cmd);
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
