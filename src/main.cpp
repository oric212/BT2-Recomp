#include <iostream>
#include <string_view>

int main(int argc, char* argv[]) {
    if (argc > 1) {
        const std::string_view argument{argv[1]};
        if (argument == "--help" || argument == "-h") {
            std::cout << "BT2 Recomp foundation\n"
                         "Usage: bt2-recomp [--help]\n";
            return 0;
        }
        std::cerr << "Unknown argument: " << argument << '\n';
        return 2;
    }

    std::cout << "BT2 Recomp foundation is running.\n";
    return 0;
}
