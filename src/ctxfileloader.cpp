#include "ctxfileloader.h"

#include <fstream>
#include <sstream>
#include <algorithm>

std::vector<std::string> CtxFileLoader::load(const char *filenam)  {
    error.clear();
    std::vector<std::string> srcs;
    std::ifstream infile(filenam);
    std::string line;
    while (std::getline(infile, line) && error.length() == 0) {
        std::istringstream iss(line);
        std::string line;
        iss >> line;
        if(line.length() > 0 && line.at(0) != '#') {
            size_t n = std::count(line.begin(), line.end(), '/');
            if(n == 2 || n == 3)
                srcs.push_back(line);
            else {
                error = std::string(__PRETTY_FUNCTION__) + ": malformed line \"" + line + "\"";
                srcs.clear();
            }
        }
    }
    return srcs;
}
