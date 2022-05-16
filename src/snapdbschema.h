#ifndef SNAPDBSCHEMA_H
#define SNAPDBSCHEMA_H

#include <string>
#include <vector>

class Connection;

class SnapDbSchemaListener {
public:
    virtual ~SnapDbSchemaListener() {}
    virtual void onProgress(double percent) = 0;
    virtual void onFinished(double elapsed) = 0;
};

class Context {
public:
    Context(const std::string& name,
            const std::string& author,
            const std::string& reason,
            const std::string& description = "");

    std::string name, author, reason, description;
};

class Ast {
public:
    Ast(const std::string& src,
        unsigned max_x,
        unsigned max_y,
        char dt,
        char df,
        char wri,
        int substitute = 0,
        int archivable = 0,
        int levelg = 0);

    int id;
    char data_type, data_format, writable, levelg, archivable;
    std::string src;
    unsigned max_dim_x, max_dim_y;
    std::string facility;
    int substitute;
};

class SnapDbSchema
{
public:
    SnapDbSchema();

    int register_context(Connection *connection,
                         const std::string& name,
                         const std::string& author,
                         const std::string& reason,
                         const std::string& description);

    int link_attributes(Connection *connection, int context_id,
                        const std::vector<std::string> & srcs) ;
};

#endif // SNAPDBSCHEMA_H
