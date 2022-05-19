#ifndef PRINT_H
#define PRINT_H

#include <string>
#include <snapctxmanager.h>

class Utils
{
public:
    void out_ctxs(const std::vector<Context> &v) const;
    void out_ctx(const Context &c, const std::vector<Ast> &v) const;
    void usage(const char *appnam);
    std::string conf_dir() const;
    std::string conf_file_path() const;
    bool conf_file_exists() const;
    bool configure();
};

#endif // PRINT_H
