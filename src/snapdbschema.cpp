#include "snapdbschema.h"
#include "tgutils.h"
#include <mysqlconnection.h>
#include <mysqlresult.h>
#include <result.h>
#include <string.h> // memset
#include <dbmacros.h>
#include <algorithm>

class SnapDbSchemaP {
public:
    std::string err, warn;
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

    m_split_src(src);

    //    printf("%s: %s\t\t%s\t\t%s\t\t%s\t\t\t{%s}\e[0m\n",
    //           __PRETTY_FUNCTION__, domain.c_str(), family.c_str(), member.c_str(), att_name.c_str(), facility.c_str());
}

Ast::Ast(const std::string &src) :
    full_name(src),
    max_dim_x(0),
    max_dim_y(0),
    data_type(0),
    data_format(0),
    writable(0) {

    printf("Ast string constructor from %s\n", src.c_str());
    m_split_src(src);

    printf("full name %s facility %s \n", full_name.c_str(), facility.c_str());
}

void Ast::m_split_src(const std::string& src) {
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
    d->err.clear();
    char q[2048];
    memset(q, 0, sizeof(char) * 2048);

    snprintf(q, 2048, "SELECT id_context FROM context WHERE name='%s'", name.c_str());
    Result *res = connection->query(q);
    if(res && res->getRowCount() > 0) {
        d->err = "context with name '" + name + "' already registered";
    }
    else if(!res) {
        d->err = std::string(connection->getError());
    }
    else {
        snprintf(q, 2048, "INSERT INTO context (time,name,author,reason,description) "
                          "VALUES ( NOW(), '%s', '%s', '%s', '%s' )", name.c_str(),author.c_str(),reason.c_str(), description.c_str());
        res = connection->query(q);
        const char* err= connection->getError();
        if(strlen(err) > 0) {
            d->err = std::string(connection->getError());
        }
        else {
            context_id = connection->getLastInsertId();
        }
        if(res)
            delete res;
    }
    return context_id;
}

int SnapDbSchema::link_attributes(Connection *connection, int context_id, const std::vector<Ast> &srcs) {
    d->err.clear(); d->warn.clear();
    int r = 0;
    std::vector <int> att_ids;
    char q[2048];
    //
    // populate ast with new attributes
    //
    for(size_t i = 0; i < srcs.size() && d->err.length() == 0; i++) {
        memset(q, 0, sizeof(char) * 2048);
        const Ast& a = srcs[i];
        Result *res = nullptr;
        Row *row = nullptr;
        // find if attribute already in ast
        snprintf(q, 2048, "SELECT ID FROM ast WHERE full_name='%s' AND facility='%s'", a.full_name.c_str(), a.facility.c_str());
        res = connection->query(q);
        if(res && res->getRowCount() > 0) {
            res->next();
            row = res->getCurrentRow();
            // reuse existing att ID for the given full_name and facility
            att_ids.push_back(atoi(row->getField(0)));
            delete res;
        }
        else if(!res) {
            d->err = connection->getError();
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
                d->err = std::string(connection->getError());
            }
            else {
                // add new att ID
                att_ids.push_back(connection->getLastInsertId());
                r += connection->getAffectedRows();
            }
            if(res)
                delete res;
        }
        if(row)
            delete row;
    }

    //
    // 2. insert (id_context, id_att) into list for each id att
    //
    // 2a. check for potential duplicates
    std::vector <int> dupids; // will be effectively inserted into list
    std::string dups;
    if(d->err.length() == 0 && att_ids.size() == srcs.size()) {
        for(size_t i = 0; i < att_ids.size() && d->err.length() == 0; i++) {
            int &aid = att_ids[i];
            snprintf(q, 2048, "SELECT id_context,id_att FROM list WHERE id_context=%d "
                              "AND id_att=%d", context_id, aid);
            Result *res = connection->query(q);
            const char* err= connection->getError();
            if(res && strlen(err) == 0) {
                if(res->getRowCount() >= 0) {
                    dupids.push_back(aid);
                    dups +=  dups.length() > 0 ? "," + srcs[i].full_name : srcs[i].full_name;
                }
            }
            else {
                d->err = std::string(err);
            }
            if(res) delete res;
        }
    }
    else if(att_ids.size() != srcs.size())
        d->err = "inconsistent number of requested src and IDs from db";

    if(dups.length() > 0)
        d->warn = "skipped duplicate sources " + dups;

    // 2b. insert // att_ids.size() may differ from srcs.size because 2a may have erased
    //     duplicate (id_context,id_att)
    for(size_t i = 0; i < att_ids.size() && d->err.length() == 0; i++) {
        int &aid = att_ids[i];
        if(std::find(dupids.begin(), dupids.end(), aid) == dupids.end()) {
            memset(q, 0, sizeof(char) * 2048);
            snprintf(q, 2048, "INSERT INTO list (id_context, id_att) VALUES (%d, %d)", context_id, aid);
            Result *res = connection->query(q);
            const char* err= connection->getError();
            if(strlen(err) > 0) {
                d->err = std::string(err);
            }
            else {
                r += connection->getAffectedRows();
            }
            if(res) delete res;
        }
    }
    return r;
}

int SnapDbSchema::ctx_id(Connection *conn, const std::string &find) {
    int id = -1;
    d->err.clear();
    char q[2048];
    memset(q, 0, sizeof(char) * 2048);

    int find_id = atoi(find.c_str());
    snprintf(q, 2048, find_id > 0 ? "SELECT id_context FROM context WHERE id_context=%s" :
                                    "SELECT id_context FROM context WHERE name='%s'", find.c_str());
    Result *res = conn->query(q);
    if(res && res->getRowCount() == 0) {
        d->err = "not found";
    }
    else if(res && res->next()) {
        Row *row = res->getCurrentRow();
        id = atoi(row->getField(0));
        delete row;
    }
    if(res)
        delete res;
    return id;
}

int SnapDbSchema::ctx_remove(Connection *conn, const std::string& id_or_nam) {
    int r = 0;
    d->err.clear();
    char q[2048];
    int cxid = ctx_id(conn, id_or_nam);
    if(cxid > 0) {
        Result *res = nullptr;
        std::vector <std::string> srcs;
        memset(q, 0, sizeof(char) * 2048);
        snprintf(q, 2048, "SELECT full_name FROM ast,context,list WHERE context.id_context=list.id_context AND ast.ID=list.id_att AND context.id_context=%d", cxid);
        res = conn->query(q);
        if(res && res->getRowCount() > 0) {
            Row *row = nullptr;
            while(res->next() > 0) {
                if((row = res->getCurrentRow()) != nullptr) {
                    srcs.push_back(row->getField(0));
                    delete row;
                }
            }
            delete res;
        }
        // 1. delete srcs related to context ctxnam
        //
        r += srcs_remove(conn, id_or_nam, srcs);
        if(d->err.length() == 0) {
            //
            // 2. delete context
            snprintf(q, 2048, "DELETE FROM context WHERE id_context=%d", cxid);
            res = conn->query(q);
            const char* err= conn->getError();
            if(strlen(err) > 0) {
                d->err = std::string(err);
            }
            else {
                r += conn->getAffectedRows();
            }
            if(res)
                delete res;
        }
        else
            d->err = "could not complete delete context '" + id_or_nam + "': " + d->err;

    }
    return r;
}

int SnapDbSchema::srcs_remove(Connection *conn, const std::string &id_or_nam, const std::vector<std::string> &srcs) {
    int ar = 0; // affected rows
    d->err.clear();
    char q[2048];
    int cxid = ctx_id(conn, id_or_nam);
    if(cxid > 0) {
        std::vector<int> id_atts;
        std::string nfsrcs; // not found srcs
        for(const std::string & s : srcs) {
            snprintf(q, 2048, "SELECT list.id_att FROM context,list,ast WHERE ast.full_name='%s'"
                              " AND context.id_context=list.id_context AND context.id_context=%d AND list.id_att=ast.ID", s.c_str(), cxid);
            printf("\e[0;36m%s\e[0m\n", q);
            Result *res = conn->query(q);
            if(res && res->getRowCount() == 1 && res->next()) {
                Row *row = res->getCurrentRow();
                id_atts.push_back(atoi(row->getField(0)));
                delete row;
            }
            else if(res && res->getRowCount() == 0) {
                if(nfsrcs.length() > 0)
                    nfsrcs += ",";
                nfsrcs += s;
            }
            if(res)
                delete res;
        }
        if(nfsrcs.length() > 0) {
            // not found srcs: cancel the entire operation
            d->err = nfsrcs + " not found";
        }
        else {
            // ok delete list
            for(int idatt : id_atts) {
                snprintf(q, 2048, "DELETE FROM list WHERE id_context=%d AND id_att=%d", cxid, idatt);
                Result *res = conn->query(q);
                if(!res)
                    d->err = std::string(conn->getError());
                else {
                    ar += static_cast<MySqlConnection *>(conn)->getAffectedRows();
                    delete res;
                }
            }
            // orphan attributes in ast?
            std::vector <int> id_atts_del;
            for(size_t i = 0; i < id_atts.size() && d->err.length() == 0; i++) {
                snprintf(q, 2048, "SELECT context.name,ast.full_name FROM context,list,ast"
                                  " WHERE id_att=%d AND context.id_context=list.id_context"
                                  " AND ast.ID=list.id_att", id_atts[i]);
                Result *res = conn->query(q);
                printf("%s: verifying if att id %d is referenced in other contexts...\e[0;36m%s\e[0m\n", __PRETTY_FUNCTION__, id_atts[i], q);
                if(!res)
                    d->err = std::string(conn->getError());
                else {
                    int ro;
                    for(ro = 0; ro < res->getRowCount(); ro++) {
                        res->next();
                        Row *row = res->getCurrentRow();
                        printf("'%s' referenced in context '%s'\n", row->getField(1), row->getField(0));
                        delete row;
                    }
                    if(ro == 0) {
                        printf("no references\n");
                        snprintf(q, 2048, "DELETE FROM ast WHERE ID=%d", id_atts[i]);
                        res = conn->query(q);
                        if(!res)
                            d->err = std::string(conn->getError());
                        else {
                            ar += conn->getAffectedRows();
                        }
                    }
                    delete res;
                }
            }
        }
    }
    return ar;
}

std::map<std::string, std::vector<Context>> SnapDbSchema::get_contexts_with_atts(Connection *conn, const std::vector<std::string> &atts) {
    std::map<std::string, std::vector<Context>> map;
    d->err.clear();
    char q[2048];
    for(size_t i = 0; d->err.length() == 0 && i < atts.size(); i++) {
        const std::string& a = atts[i];
        memset(q, 0, sizeof(char) * 2048);
        snprintf(q, 2048, "SELECT name,author,reason,description FROM ast,context,list WHERE ast.ID=list.id_att AND context.id_context=list.id_context AND ast.full_name='%s'",
                 a.c_str());
        Result *res = conn->query(q);
        for(int i = 0; res != nullptr && i < res->getRowCount(); i++) {
            res->next();
            Row* row = res->getCurrentRow();
            Context c(row->getField(0), row->getField(1), row->getField(2), row->getField(3));
            map[a].push_back(c);
            delete row;
        }
        if(res)
            delete res;
        if(strlen(conn->getError()) > 0)
            d->err = conn->getError();
    }
    return map;
}

int SnapDbSchema::rename(Connection *conn, const std::vector<std::string> &olda,  const std::vector<Ast> &v) {
    int r = 0;
    d->err.clear();
    if(olda.size() == v.size()) {
        char q[2048];
        // find if new attributes already in ast
        for(size_t i = 0;  d->err.length() == 0 && i < v.size(); i++) {
            const Ast old_a(olda[i]);
            const Ast& a = v[i];
            memset(q, 0, sizeof(char) * 2048);
            snprintf(q, 2048, "SELECT ID FROM ast WHERE full_name='%s' AND facility='%s'", old_a.full_name.c_str(),  old_a.facility.c_str());
            Result *res = conn->query(q);
            if(res && res->getRowCount() == 1 && res->next()) {
                Row *row = res->getCurrentRow();
                int id = atoi(row->getField(0));
                memset(q, 0, sizeof(char) * 2048);
                snprintf(q, 2048, "UPDATE ast SET full_name='%s', device='%s', domain='%s', "
                                  " family='%s', member='%s', att_name='%s',"
                                  "data_type=%d, data_format=%d, writable=%d, max_dim_x=%d, max_dim_y=%d, levelg=%d, facility='%s',"
                                  "archivable=%d, substitute=%d WHERE ID=%d",
                         a.full_name.c_str(), a.device.c_str(), a.domain.c_str(),
                         a.family.c_str(), a.member.c_str(), a.att_name.c_str(),
                         a.data_type, a.data_format, a.writable, a.max_dim_x, a.max_dim_y, a.levelg, a.facility.c_str(),
                         a.archivable, a.substitute, id);
                Result *updres = conn->query(q);
                if(updres && conn->getAffectedRows() == 1) {
                    r++;
                }
                else
                    d->err = std::string(conn->getError());
                if(updres)
                    delete updres;
            }
            else if(res && res->getRowCount() == 0) {
                d->err = a.full_name + " " + (a.facility != "HOST:port" ? ( "(" + a.facility + ")" ) : "") + "not found";
            }
            if(res)
                delete res;
            else
                d->err = conn->getError();
        }
    }
    else
        d->err = "old and new attribute name lists differ in size";
    return r;
}

bool SnapDbSchema::get_context(Connection *conn, const std::string &id_or_nam, Context &ctx, std::vector<Ast> &v) {
    d->err.clear();
    v.clear();
    char q[2048];
    memset(q, 0, sizeof(char) * 2048);
    int id = atoi(id_or_nam.c_str()) > 0;
    snprintf(q, 2048, id > 0 ? "SELECT id_context,name,author,reason,description FROM context WHERE id_context=%s ORDER BY id_context ASC" :
                               "SELECT id_context,name,author,reason,description FROM context WHERE name='%s' ORDER BY id_context ASC", id_or_nam.c_str());
    Result *res = conn->query(q);
    if(res && res->getRowCount() == 0) {
        d->err = "not found";
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
                          "FROM list,ast WHERE ast.ID=list.id_att AND list.id_context=%d "
                          "ORDER BY full_name ASC", ctx.id);
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
            d->err = "no attributes in context";

    }
    else if(res)
        d->err = "multiple matches for '" + id_or_nam + "'";
    else
        d->err = conn->getError();

    return d->err.length() == 0;
}

int SnapDbSchema::search(Connection *conn, const std::string &search, std::vector<Context> &ctxs) {
    d->err.clear();
    ctxs.clear();
    char q[2048];
    memset(q, 0, sizeof(char) * 2048);
    snprintf(q, 2048, "SELECT id_context,name,author,reason,description FROM context WHERE name LIKE '%%%s%%' ORDER BY id_context ASC", search.c_str());
    Result *res = conn->query(q);
    if(res && res->getRowCount() > 0) {

        Row *row;
        while(res && d->err.length() == 0 && res->next()) {
            row = res->getCurrentRow();
            if(row->getFieldCount() == 5) {
                Context c(row->getField(1), row->getField(2), row->getField(3), row->getField(4));
                c.id = atoi(row->getField(0));
                ctxs.push_back(c);
            }
        }
    }
    if(!res)
        d->err = conn->getError();

    return ctxs.size();
}

std::vector<Context> SnapDbSchema::ctxlist(Connection *conn) {
    std::vector<Context> ctxs;
    search(conn, "", ctxs);
    return ctxs;
}

std::string SnapDbSchema::error() const {
    return d->err;
}

std::string SnapDbSchema::warning() const
{
    return d->warn;
}

