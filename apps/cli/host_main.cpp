#include <chorus/core.hpp>
#include <iostream>

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    std::cout << "Chorus Host CLI v" << chorus::version() << '\n';
    std::cout << "Protocol version: " << static_cast<int>(chorus::kProtocolVersion) << '\n';
    return 0;
}
