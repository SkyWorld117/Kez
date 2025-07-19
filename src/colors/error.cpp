#include "colored_io.h"

int main(int argc, char* argv[]) {
    if (argc < 2) {
        ERROR("Usage: " + std::string(argv[0]) + " <message>");
        return 1;
    }

    std::string message = argv[1];
    ERROR(message);

    return 0;
}