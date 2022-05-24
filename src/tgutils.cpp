#include "tgutils.h"
#include <tango.h>

std::vector<Ast> TgUtils::get(const std::vector<std::string> &srcs) {
    err.clear();
    std::vector<Ast> r;
    for(const std::string &s : srcs) {
        int last = s.find_last_of('/');
        std::string dev = s.substr(0, last);
        std::string a = s.substr(last + 1);


        try {
            Tango::DeviceProxy *de = new Tango::DeviceProxy(dev);
            Tango::AttributeInfoEx x = de->get_attribute_config(a);
            r.push_back(Ast(s, x.max_dim_x, x.max_dim_y, x.data_type, x.data_format, x.writable));
            delete de;
        } catch(const Tango::DevFailed& e) {
            if(err.length() > 0)
                err += "\n";
            err += strerror(e);
        }
    }
    //    err = "test error";
    return r;
}

string TgUtils::data_type(int dt) const {
//    return std::string(Tango::data_type_to_string(dt));
    return std::to_string(dt);
}

string TgUtils::data_format(int f) const {
    switch(f) {
    case Tango::SPECTRUM:
        return "SPE";
    case Tango::SCALAR:
        return "SCA";
    case Tango::IMAGE:
        return "IMG";
    default:
        return "UNKNOWN";
    }
}

string TgUtils::writable(int wt) const {
    switch(wt) {
    case Tango::READ:
        return "R";
    case Tango::WRITE:
        return "W";
    case Tango::READ_WRITE:
        return "RW";
    case Tango::READ_WITH_WRITE:
        return "RWW";
    default:
        return "UNKNOWN";
    }
}

std::string TgUtils::strerror(const Tango::DevFailed &e)
{
    std::string msg;
    if(e.errors.length() > 0)
        msg = strerror(e.errors);

    return msg;
}

std::string TgUtils::strerror(const Tango::DevErrorList &errors)
{
    std::string msg;
    for(int i = errors.length() - 1; i >= 0; i--)
    {
        msg += errors[i].origin;
        msg += "\n";
        msg += errors[i].desc;
        msg += "\n";
        msg += errors[i].reason;
        msg += "\n\n";
    }
    return msg;
}
