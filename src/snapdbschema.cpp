#include "snapdbschema.h"
#include <mysqlconnection.h>
#include <mysqlresult.h>
#include <result.h>
#include <string.h> // memset
#include <hdbxmacros.h>

Context::Context(const std::string &nam,
                 const std::string &auth,
                 const std::string &reas,
                 const std::string &desc)
    : name(nam), author(auth), reason(reas) {
    desc.length() > 0 ? description = desc : description = reas;
}

Ast::Ast(const std::string &_src,
         unsigned max_x,
         unsigned max_y,
         char dt,
         char df,
         char wri,
         int sub,
         int archiva,
         int levg)
    : src(_src),
      max_dim_x(max_x),
      max_dim_y(max_y),
      data_type(dt),
      data_format(df),
      writable(wri),
      substitute(sub),
      archivable(archiva),
      levelg(levg) {
}


SnapDbSchema::SnapDbSchema()
{

}

int SnapDbSchema::register_context(Connection *connection, const std::string &name, const std::string &author, const std::string &reason, const std::string &description) {
    char q[2048];
    memset(q, 0, sizeof(char) * 2048);
    snprintf(q, 2048, "INSERT INTO context (time,name,author,reason,description) "
                      "VALUES ( NOW(), %s, %s, %s, %s )", name.c_str(),author.c_str(),reason.c_str(), description.c_str());

    Result *res = connection->query(q);
    const char* err= connection->getError();
    if(strlen(err) == 0) {

    }
    else
        perr("%s: %s", __PRETTY_FUNCTION__, err);
}

int SnapDbSchema::link_attributes(Connection *connection, int context_id, const std::vector<std::string> &srcs) {

}

