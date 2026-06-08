#include <iostream>

int main(int argc, char* argv[]) {
    (void)argc; (void)argv;
    std::cout << "aestool — Lab 2: AES (manual C++ implementation, no crypto lib)\n"
              << "Usage: aestool <encrypt|decrypt|kat|benchmark> [options]\n";
    return 0;
}
