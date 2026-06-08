#include <iostream>

int main(int argc, char* argv[]) {
    (void)argc; (void)argv;
    std::cout << "sigtool — Lab 5: ECDSA-P256 & RSA-PSS Signatures\n"
              << "Usage: sigtool <keygen|sign|verify|kat|benchmark> [options]\n";
    return 0;
}
