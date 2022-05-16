#ifndef CTXFILELOADER_H
#define CTXFILELOADER_H

#include <vector>
#include <string>

class CtxFileLoader
{
public:
    std::vector<std::string> load(const char *filenam);

    std::string error;
};

#endif // CTXFILELOADER_H
