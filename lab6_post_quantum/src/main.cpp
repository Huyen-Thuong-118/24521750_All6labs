#include <iostream>

int main(int argc, char* argv[]) {
    (void)argc; (void)argv;
    std::cout << "pqtool — Lab 6: ML-DSA-44 & ML-KEM-512 (Post-Quantum)\n"
              << "Usage: pqtool <keygen|sign|verify|encaps|decaps|cert|benchmark> [options]\n";
    return 0;
}
