#include "snapdbschema.h"
#include "tgutils.h"
#include <mysqlconnection.h>
#include <mysqlresult.h>
#include <result.h>
#include <string.h> // memset
#include <hdbxmacros.h>
#include <algorithm>

class SnapDbSchemaP {
public:
    std::string msg;
};

Context::Context(const std::string &nam,
                 const std::string &auth,
                 const std::string &reas,
                 const std::string &desc)
    : name(nam), author(auth), reason(reas) {
    desc.length() > 0 ? description = desc : description = reas;
}

Ast::Ast(const std::string &src,
         unsigned max_x,
         unsigned max_y,
         char dt,
         char df,
         char wri,
         int sub,
         int archiva,
         int levg)
    : full_name(src),
      max_dim_x(max_x),
      max_dim_y(max_y),
      data_type(dt),
      data_format(df),
      writable(wri),
      substitute(sub),
      archivable(archiva),
      levelg(levg) {

    int last = src.find_last_of('/');
    att_name = src.substr(last + 1);
    bool has_facility = (std::count(src.begin(), src.end(), '/') == 4);
    facility = has_facility ? src.substr(0, src.find("/")) : "HOST:port";
    size_t sod = has_facility ? (facility.length() + 1) : 0; // start of domain
    int devlen = last - sod;
    device = src.substr(sod, devlen);
    // if no facilty specified SELECT facility FROM ast returns "HOST:PORT"
    domain = device.substr(0, device.find("/"));
    size_t flen = device.find("/", domain.length() + 1) - domain.length() - 1;
    family = device.substr(device.find("/") + 1, flen);
    member = device.substr(device.find_last_of("/") + 1);
    if(has_facility) {
        full_name = src.substr(src.find("/") + 1);
    }
    printf("%s: %s\t\t%s\t\t%s\t\t%s\t\t\t{%s}\e[0m\n",
           __PRETTY_FUNCTION__, domain.c_str(), family.c_str(), member.c_str(), att_name.c_str(), facility.c_str());
}


SnapDbSchema::SnapDbSchema() {
    d = new SnapDbSchemaP;
}

SnapDbSchema::~SnapDbSchema() {
    delete d;
}

int SnapDbSchema::register_context(Connection *connection, const std::string &name,
                                   const std::string &author, const std::string &reason,
                                   const std::string &description) {
    int context_id = 0; // the new context id, if successful
    d->msg.clear();
    char q[2048];
    memset(q, 0, sizeof(char) * 2048);

    snprintf(q, 2048, "SELECT id_context FROM context WHERE name='%s'", name.c_str());
    Result *res = connection->query(q);
    if(res && res->getRowCount() > 0) {
        d->msg = "context with name '" + name + "' already registered";
    }
    else if(!res) {
        d->msg = std::string(connection->getError());
    }
    else {
        snprintf(q, 2048, "INSERT INTO context (time,name,author,reason,description) "
                          "VALUES ( NOW(), '%s', '%s', '%s', '%s' )", name.c_str(),author.c_str(),reason.c_str(), description.c_str());
        res = connection->query(q);
        const char* err= connection->getError();
        if(strlen(err) > 0) {
            d->msg = std::string(connection->getError());
        }
        else {
            context_id = static_cast<MySqlConnection *>(connection)->lastInsertId();
        }
    }
    return context_id;
}

int SnapDbSchema::link_attributes(Connection *connection, int context_id, const std::vector<Ast> &srcs) {
    d->msg.clear();
    int r = 0;
    std::vector <int> att_ids;
    char q[2048];
    //
    // populate ast with new attributes
    //
    for(size_t i = 0; i < srcs.size() && d->msg.length() == 0; i++) {
        memset(q, 0, sizeof(char) * 2048);
        const Ast& a = srcs[i];
        Result *res;
        Row *row;
        // find if attribute already in ast
        snprintf(q, 2048, "SELECT ID FROM ast WHERE full_name='%s' AND facility='%s'", a.full_name.c_str(), a.facility.c_str());
        res = connection->query(q);
        if(res && res->getRowCount() > 0) {
            res->next();
            row = res->getCurrentRow();
            // reuse existing att ID for the given full_name and facility
            att_ids.push_back(atoi(row->getField(0)));
            printf("\e[1;32m i \e[0m attribute '%s' facility '%s' already in ast with ID %s\e[0m\n", a.full_name.c_str(), a.facility.c_str(), row->getField(0));
        }
        else if(!res) {
            d->msg = connection->getError();
        }
        else if(res->getRowCount() == 0) { // need insert new row

            snprintf(q, 2048, "INSERT INTO ast (time,full_name,device,domain,family,member,att_name,data_type,"
                              "data_format,writable,max_dim_x,max_dim_y,levelg,facility,archivable,substitute)"
                              "VALUES (NOW(),'%s', '%s', '%s', '%s', '%s', '%s', %d, "
                              "%d, %d, %u, %u, %d, '%s', %d, %d)",
                     a.full_name.c_str(), a.device.c_str(), a.domain.c_str(), a.family.c_str(), a.member.c_str(),
                     a.att_name.c_str(), a.data_type, a.data_format, a.writable, a.max_dim_x, a.max_dim_y,
                     a.levelg, a.facility.c_str(), a.archivable, a.substitute );

            res = connection->query(q);
            const char* err= connection->getError();
            if(strlen(err) > 0) {
                d->msg = std::string(connection->getError());
            }
            else {
                // add new att ID
                att_ids.push_back(static_cast<MySqlConnection *>(connection)->lastInsertId());
                r += static_cast<MySqlConnection *>(connection)->getAffectedRows();
            }
        }
    }

    //
    // 2. insert (id_context, id_att) into list for each id att
    //
    if(d->msg.length() == 0 && att_ids.size() == r) {
        for(size_t i = 0; i < att_ids.size() && d->msg.length() == 0; i++) {
            memset(q, 0, sizeof(char) * 2048);
            int &aid = att_ids[i];
            snprintf(q, 2048, "INSERT INTO list (id_context, id_att) VALUES (%d, %d)", context_id, aid);
            Result *res = connection->query(q);
            const char* err= connection->getError();
            if(strlen(err) > 0) {
                d->msg = std::string(err);
            }
            else {
                r += static_cast<MySqlConnection *>(connection)->getAffectedRows();
            }
        }
    }
    return r;
}

int SnapDbSchema::ctx_remove(Connection *conn, const std::string& ctxnam)
{
    int r = 0;
    d->msg.clear();
    char q[2048];
    memset(q, 0, sizeof(char) * 2048);

    snprintf(q, 2048, "SELECT id_context FROM context WHERE name='%s'", ctxnam.c_str());
    Result *res = conn->query(q);
    if(res && res->getRowCount() == 0) {
        d->msg = "not found";
    }
    else if(res && res->getRowCount() == 1) {
        std::vector <std::string> srcs;
        memset(q, 0, sizeof(char) * 2048);
        snprintf(q, 2048, "SELECT full_name FROM ast,context,list WHERE context.id_context=list.id_context AND ast.ID=list.id_att AND context.name='%s'", ctxnam.c_str());
        res = conn->query(q);
        if(res && res->getRowCount() > 0) {
            Row *row;
            while(res->next() > 0) {
                row = res->getCurrentRow();
                srcs.push_back(row->getField(0));
            }
        }
        // 1. delete srcs related to context ctxnam
        //
        r += srcs_remove(conn, ctxnam, srcs);
        if(d->msg.length() == 0) {
            //
            // 2. delete context
            snprintf(q, 2048, "DELETE FROM context WHERE name='%s'", ctxnam.c_str());
            res = conn->query(q);
            const char* err= conn->getError();
            if(strlen(err) > 0) {
                d->msg = std::string(err);
            }
            else {
                r = static_cast<MySqlConnection *>(conn)->getAffectedRows();
            }
        }
        else
            d->msg = "could not complete delete context '" + ctxnam + "': " + d->msg;
    }
    return r;
}

int SnapDbSchema::srcs_remove(Connection *conn, const std::string &ctxnam, const std::vector<std::string> &srcs) {
    int ar = 0; // affected rows
    d->msg.clear();
    char q[2048];
    memset(q, 0, sizeof(char) * 2048);

    snprintf(q, 2048, "SELECT id_context FROM context WHERE name='%s'", ctxnam.c_str());
    Result *res = conn->query(q);
    if(res && res->getRowCount() == 0) {
        d->msg = "not found";
    }
    else if(res && res->getRowCount() == 1) {
        std::vector<int> id_atts;
        std::string nfsrcs; // not found srcs
        res->next();
        Row *row = res->getCurrentRow();
        printf("SnapDbSchema::srcs_remove get field %s atoi %d\n", row->getField(0), atoi(row->getField(0)));
        if(row->getFieldCount() == 1) {
            int ctx = atoi(row->getField(0));
            for(const std::string & s : srcs) {
                snprintf(q, 2048, "SELECT list.id_att FROM context,list,ast WHERE ast.full_name='%s'"
                                  " AND context.id_context=list.id_context AND context.id_context=%d AND list.id_att=ast.ID", s.c_str(), ctx);
                printf("\e[0;36m%s\e[0m\n", q);
                res = conn->query(q);
                if(res && res->getRowCount() == 1) {
                    res->next();
                    row = res->getCurrentRow();
                    id_atts.push_back(atoi(row->getField(0)));
                }
                else if(res && res->getRowCount() == 0) {
                    if(nfsrcs.length() > 0)
                        nfsrcs += ",";
                    nfsrcs += s;
                }
            }
            if(nfsrcs.length() > 0) {
                // not found srcs: cancel the entire operation
                d->msg = nfsrcs + " not found";
            }
            else {
                // ok delete list
                for(int idatt : id_atts) {
                    snprintf(q, 2048, "DELETE FROM list WHERE id_context=%d AND id_att=%d", ctx, idatt);
                    res = conn->query(q);
                    if(!res)
                        d->msg = std::string(conn->getError());
                    else {
                        ar += static_cast<MySqlConnection *>(conn)->getAffectedRows();
                    }
                }
                // orphan attributes in ast?
                std::vector <int> id_atts_del;
                for(size_t i = 0; i < id_atts.size() && d->msg.length() == 0; i++) {
                    snprintf(q, 2048, "SELECT context.name,ast.full_name FROM context,list,ast"
                                      " WHERE id_att=%d AND context.id_context=list.id_context"
                                      " AND ast.ID=list.id_att", id_atts[i]);
                    res = conn->query(q);
                    printf("%s: verifying if att id %d is referenced in other contexts...\e[0;36m%s\e[0m\n", __PRETTY_FUNCTION__, id_atts[i], q);
                    if(!res)
                        d->msg = std::string(conn->getError());
                    else {
                        int ro;
                        for(ro = 0; ro < res->getRowCount(); ro++) {
                            res->next();
                            row = res->getCurrentRow();
                            printf("'%s' referenced in context '%s'\n", row->getField(1), row->getField(0));
                        }
                        if(ro == 0) {
                            printf("no references\n");
                            snprintf(q, 2048, "DELETE FROM ast WHERE ID=%d", id_atts[i]);
                            res = conn->query(q);
                            if(!res)
                                d->msg = std::string(conn->getError());
                            else {
                                ar += static_cast<MySqlConnection *>(conn)->getAffectedRows();
                            }
                        }
                    }
                }
            }
        }
    }
    return ar;
}

bool SnapDbSchema::get_context(Connection *conn, const std::string &id_or_nam, Context &ctx, std::vector<Ast> &v) {
    d->msg.clear();
    v.clear();
    char q[2048];
    memset(q, 0, sizeof(char) * 2048);
    int id = atoi(id_or_nam.c_str()) > 0;
    snprintf(q, 2048, id > 0 ? "SELECT id_context,name,author,reason,description FROM context WHERE id_context=%s" :
                               "SELECT id_context,name,author,reason,description FROM context WHERE name='%s'", id_or_nam.c_str());
    Result *res = conn->query(q);
    if(res && res->getRowCount() == 0) {
        d->msg = "not found";
    }
    else if(res && res->getRowCount() == 1) {
        res->next();
        Row * row = res->getCurrentRow();
        if(row->getFieldCount() == 5) {
            ctx.id = atoi(row->getField(0));
            ctx.name = row->getField(1);
            ctx.author = row->getField(2);
            ctx.reason = row->getField(3);
            ctx.description = row->getField(4);
        }
        memset(q, 0, sizeof(char) * 2048);
        enum Fields { ID = 0, FN, DT, DF, W, X, Y, F, MAXFIELDS };
        snprintf(q, 2048, "SELECT ID,full_name,data_type,data_format,writable, max_dim_x, max_dim_y, facility "
                          "FROM list,ast WHERE ast.ID=list.id_att AND list.id_context=%d", ctx.id);

         res = conn->query(q);
         v.reserve(res->getRowCount());
         while(res && res->next() > 0) {
             row = res->getCurrentRow();
             if(row->getFieldCount() == MAXFIELDS) {
                 Ast a(row->getField(FN), atoi(row->getField(X)), atoi(row->getField(Y)), atoi(row->getField(DT)), atoi(row->getField(DF)), atoi(row->getField(W)));
                 a.facility = row->getField(F);
                 a.id = atoi(row->getField(0));
                 v.push_back(a);
             }
         }
         if(v.size() == 0)
             d->msg = "no attributes in context";

    }
    else if(res)
        d->msg = "multiple matches for '" + id_or_nam + "'";
    else
        d->msg = conn->getError();

    return d->msg.length() == 0;
}

int SnapDbSchema::search(Connection *conn, const std::string &search, std::vector<Context> &ctxs) {
    d->msg.clear();
    ctxs.clear();
    char q[2048];
    memset(q, 0, sizeof(char) * 2048);
    snprintf(q, 2048, "SELECT id_context,name,author,reason,description FROM context WHERE name LIKE '%%%s%%'", search.c_str());
    Result *res = conn->query(q);
    if(res && res->getRowCount() == 0) {
        d->msg = "not found";
    }
    Row *row;
    while(res && d->msg.length() == 0 && res->next()) {
        row = res->getCurrentRow();
        if(row->getFieldCount() == 5) {
            Context c(row->getField(1), row->getField(2), row->getField(3), row->getField(4));
            c.id = atoi(row->getField(0));
            ctxs.push_back(c);
        }
    }
    if(!res)
        d->msg = conn->getError();

    return ctxs.size();
}


std::string SnapDbSchema::message() const {
    return d->msg;
}

