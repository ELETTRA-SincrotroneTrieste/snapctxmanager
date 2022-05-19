#ifndef TGPROPFETCH_H
#define TGPROPFETCH_H

#include "snapctxmanager.h"

namespace Tango {
class DevFailed;
class DevErrorList;
}

class TgUtils
{
public:
    std::vector<Ast> get(const std::vector<std::string >&srcs);
    std::string data_type(int dt) const;
    std::string data_format(int f) const;
    std::string writable(int wt) const;

    std::string err;
private:
    std::string strerror(const Tango::DevFailed &e);
    std::string strerror(const Tango::DevErrorList &errors);
};

#endif // TGPROPFETCH_H
