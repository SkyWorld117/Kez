#include <fstream>
#include <sstream>

std::string read_file(const std::string& path) {
    std::ifstream ifs(path);
    if (!ifs) {
        return "";
    }
    std::ostringstream buffer;
    buffer << ifs.rdbuf();
    return buffer.str();
}
