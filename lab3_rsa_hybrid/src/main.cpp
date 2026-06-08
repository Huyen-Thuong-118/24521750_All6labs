#include <iostream>

int main(int argc, char* argv[]) {
    (void)argc; (void)argv;
    std::cout << "rsatool — Lab 3: RSA-OAEP + AES-GCM Hybrid\n"
              << "Usage: rsatool <keygen|encrypt|decrypt|kat|benchmark> [options]\n";
    return 0;
}
