#ifndef SNAPDBSCHEMA_H
#define SNAPDBSCHEMA_H

#include <string>
#include <map>
#include <vector>

class Connection;

class SnapDbSchemaListener {
public:
    virtual ~SnapDbSchemaListener() {}
    virtual void onProgress(double percent) = 0;
    virtual void onFinished(double elapsed) = 0;
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

    explicit Ast(const std::string& src);

    Ast() : id(-1), max_dim_x(0), max_dim_y(0) {}

    int id;
    std::string full_name, device, domain, family,  member, att_name;
    unsigned max_dim_x, max_dim_y;
    char data_type, data_format, writable;
    std::string facility;
    int substitute;
    char archivable, levelg;

private:
    void m_split_src(const std::string &src);
};

class Context {
public:
    Context(const std::string& name,
            const std::string& author,
            const std::string& reason,
            const std::string& description = "");

    Context() : id(-1) {}

    std::string name, author, reason, description;
    int id;
};


class SnapDbSchemaP;

class SnapDbSchema
{
public:
    SnapDbSchema();
    ~SnapDbSchema();

    int register_context(Connection *connection,
                         const std::string& name,
                         const std::string& author,
                         const std::string& reason,
                         const std::string& description);

    int link_attributes(Connection *connection, int context_id,
                        const std::vector<Ast> &srcs);

    int ctx_id(Connection *conn, const std::string &find);
    int ctx_remove(Connection *conn, const std::string &id_or_nam);
    int srcs_remove(Connection *conn, const std::string& ctxnam, const std::vector<std::string> & srcs);
    std::map<std::string, std::vector<Context> > get_contexts_with_atts(Connection *conn, const std::vector<std::string> &atts);
    int rename(Connection *conn, const std::vector<std::string>& olda, const std::vector<Ast> &v);
    bool get_context(Connection *conn, const std::string& id_or_nam, Context &ctx, std::vector<Ast>& v);
    int search(Connection *conn, const std::string &search, std::vector<Context> &ctxs);
    std::vector<Context> ctxlist(Connection *conn);

    std::string error() const;
    std::string warning() const;


private:
    SnapDbSchemaP *d;
};

#endif // SNAPDBSCHEMA_H
