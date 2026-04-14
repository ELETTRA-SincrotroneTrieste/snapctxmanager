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

//        printf("%s: %s\t\t%s\t\t%s\t\t%s\t\t\t{%s}\e[0m\n",
//               __PRETTY_FUNCTION__, domain.c_str(), family.c_str(), member.c_str(), att_name.c_str(), facility.c_str());
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
                if(res->getRowCount() > 0) {
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
        d->warn = "skipped duplicate sources:\n" + dups + ":\nalready in context " + std::to_string(context_id);

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

int SnapDbSchema::srcs_remove(Connection *conn, const std::string &id_or_nam,
                              const std::vector<std::string> &srcs, bool purge_data) {
    int ar = 0; // affected rows
    d->err.clear();
    d->warn.clear();
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

            // purge snapshot data for this context's snapshots if requested
            if(purge_data && d->err.empty() && !id_atts.empty()) {
                static const char *val_tables[] = {
                    "t_sc_num_1val", "t_sc_num_2val",
                    "t_sc_str_1val", "t_sc_str_2val",
                    "t_sp_1val",     "t_sp_2val",
                    nullptr
                };
                for(int idatt : id_atts) {
                    for(int ti = 0; val_tables[ti] != nullptr && d->err.empty(); ti++) {
                        snprintf(q, 2048,
                                 "DELETE FROM %s WHERE id_att=%d"
                                 " AND id_snap IN (SELECT id_snap FROM snapshot WHERE id_context=%d)",
                                 val_tables[ti], idatt, cxid);
                        Result *res = conn->query(q);
                        if(!res)
                            d->err = std::string(conn->getError());
                        else {
                            ar += conn->getAffectedRows();
                            delete res;
                        }
                    }
                }
                d->warn = "Snapshot data deleted for removed attribute(s). "
                          "Omit --purge to keep data for future retrieval.";
            }
            else if(!purge_data && d->err.empty()) {
                d->warn = "Attribute(s) removed from context. Snapshot data left in place. "
                          "Use --purge to also delete all stored values.";
            }

            // orphan attributes in ast?
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

bool SnapDbSchema::get_context_sorted(Connection *conn, const std::string &id_or_nam, Context &ctx, std::vector<Ast> &v) {
    // First fetch the context metadata (same as get_context)
    bool found = get_context(conn, id_or_nam, ctx, v);
    if(!found || v.empty())
        return found;
    // Refetch attributes with section_order JOIN
    char q[2048];
    memset(q, 0, sizeof(char) * 2048);
    enum Fields { ID = 0, FN, DT, DF, W, X, Y, F, MAXFIELDS };
    snprintf(q, 2048,
             "SELECT ID,full_name,data_type,data_format,writable,max_dim_x,max_dim_y,facility "
             "FROM ast,list,section_order "
             "WHERE ast.ID=list.id_att AND section_order.section=ast.domain "
             "AND list.id_context=%d "
             "ORDER BY section_order.position,ast.full_name ASC", ctx.id);
    Result *res = conn->query(q);
    if(res && res->getRowCount() > 0) {
        v.clear();
        v.reserve(res->getRowCount());
        Row *row = nullptr;
        while(res->next() > 0) {
            row = res->getCurrentRow();
            if(row && row->getFieldCount() == MAXFIELDS) {
                Ast a(row->getField(FN), atoi(row->getField(X)), atoi(row->getField(Y)),
                      atoi(row->getField(DT)), atoi(row->getField(DF)), atoi(row->getField(W)));
                a.facility = row->getField(F);
                a.id = atoi(row->getField(0));
                v.push_back(a);
            }
        }
        delete res;
    }
    // If query failed (e.g. section_order doesn't exist), keep unsorted result from get_context
    return d->err.length() == 0;
}

std::vector<std::string> SnapDbSchema::section_order_list(Connection *conn) {
    std::vector<std::string> sections;
    Result *res = conn->query("SELECT section FROM section_order ORDER BY position");
    if(res && res->getRowCount() > 0) {
        while(res->next() > 0) {
            Row *row = res->getCurrentRow();
            if(row && row->getFieldCount() >= 1)
                sections.push_back(row->getField(0));
        }
        delete res;
    }
    return sections;
}

std::vector<std::string> SnapDbSchema::selection_type_order(Connection *conn, int ctx_id) {
    std::vector<std::string> types;
    char q[512];
    snprintf(q, sizeof(q),
             "SELECT DISTINCT type_order.type FROM selection,type_order "
             "WHERE selection.context=%d AND selection.type=type_order.type "
             "ORDER BY type_order.position", ctx_id);
    Result *res = conn->query(q);
    if(res && res->getRowCount() > 0) {
        while(res->next() > 0) {
            Row *row = res->getCurrentRow();
            if(row && row->getFieldCount() >= 1)
                types.push_back(row->getField(0));
        }
        delete res;
    }
    return types;
}

std::vector<DeviceInfo> SnapDbSchema::selection_devices(Connection *conn, int ctx_id) {
    std::vector<DeviceInfo> devices;
    char q[512];
    snprintf(q, sizeof(q),
             "SELECT device,devices.section,devices.type "
             "FROM devices,selection,section_order "
             "WHERE context=%d AND devices.section=selection.section AND "
             "devices.type=selection.type AND "
             "section_order.section=devices.section "
             "ORDER BY section_order.position", ctx_id);
    Result *res = conn->query(q);
    if(res && res->getRowCount() > 0) {
        while(res->next() > 0) {
            Row *row = res->getCurrentRow();
            if(row && row->getFieldCount() >= 3) {
                DeviceInfo di;
                di.device  = row->getField(0);
                di.section = row->getField(1);
                di.type    = row->getField(2);
                devices.push_back(di);
            }
        }
        delete res;
    }
    return devices;
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

bool SnapDbSchema::get_snapshot(Connection *conn, int snap_id, Snapshot &snap) {
    d->err.clear();
    char q[256];
    snprintf(q, sizeof(q),
             "SELECT id_snap,id_context,time,snap_comment FROM snapshot WHERE id_snap=%d LIMIT 1",
             snap_id);
    Result *res = conn->query(q);
    if(!res) { d->err = conn->getError(); return false; }
    bool found = false;
    if(res->next() > 0) {
        Row *row = res->getCurrentRow();
        if(row && row->getFieldCount() == 4) {
            snap.id_snap    = atoi(row->getField(0));
            snap.id_context = atoi(row->getField(1));
            snap.time       = row->getField(2) ? row->getField(2) : "";
            snap.comment    = row->getField(3) ? row->getField(3) : "";
            found = true;
            delete row;
        }
    }
    delete res;
    if(!found && d->err.empty())
        d->err = std::string("snapshot not found: id_snap=") + std::to_string(snap_id);
    return found;
}

int SnapDbSchema::snap_list(Connection *conn, int context_id, std::vector<Snapshot> &snaps) {
    d->err.clear();
    snaps.clear();
    char q[512];
    snprintf(q, sizeof(q),
             "SELECT id_snap,id_context,time,snap_comment FROM snapshot "
             "WHERE id_context=%d ORDER BY time DESC", context_id);
    Result *res = conn->query(q);
    if(!res) { d->err = conn->getError(); return 0; }
    while(res->next() > 0) {
        Row *row = res->getCurrentRow();
        if(row && row->getFieldCount() == 4) {
            Snapshot s;
            s.id_snap    = atoi(row->getField(0));
            s.id_context = atoi(row->getField(1));
            s.time       = row->getField(2) ? row->getField(2) : "";
            s.comment    = row->getField(3) ? row->getField(3) : "";
            snaps.push_back(s);
            delete row;
        }
    }
    delete res;
    return (int)snaps.size();
}

int SnapDbSchema::snap_save(Connection *conn, int context_id, const std::string &comment,
                             const std::vector<SnapSaveRecord> &data) {
    d->err.clear();
    char q[4096];
    // 1. Create snapshot row
    snprintf(q, sizeof(q),
             "INSERT INTO snapshot (id_context,time,snap_comment) VALUES (%d,NOW(),'%s')",
             context_id, comment.c_str());
    Result *res = conn->query(q);
    if(!res || strlen(conn->getError()) > 0) {
        d->err = conn->getError();
        if(res) delete res;
        return -1;
    }
    int snap_id = conn->getLastInsertId();
    if(res) delete res;

    // 2. Group records by snap_type and build bulk INSERT strings
    struct Vals {
        std::string num1, num2, str1, str2, sp1, sp2;
    } v;

    for(const SnapSaveRecord &r : data) {
        if(r.value == "NULL" || r.value.empty()) continue; // skip error records
        char row_buf[1024];
        switch(r.snap_type) {
        case stScNum1:
            snprintf(row_buf, sizeof(row_buf), "(%d,%d,%s),", snap_id, r.id_att, r.value.c_str());
            v.num1 += row_buf; break;
        case stScNum2:
            snprintf(row_buf, sizeof(row_buf), "(%d,%d,%s,%s),", snap_id, r.id_att,
                     r.value.c_str(), r.setpoint.empty() ? r.value.c_str() : r.setpoint.c_str());
            v.num2 += row_buf; break;
        case stScStr1:
            snprintf(row_buf, sizeof(row_buf), "(%d,%d,'%s'),", snap_id, r.id_att, r.value.c_str());
            v.str1 += row_buf; break;
        case stScStr2:
            snprintf(row_buf, sizeof(row_buf), "(%d,%d,'%s','%s'),", snap_id, r.id_att,
                     r.value.c_str(), r.setpoint.empty() ? r.value.c_str() : r.setpoint.c_str());
            v.str2 += row_buf; break;
        case stSp1:
            snprintf(row_buf, sizeof(row_buf), "(%d,%d,%d,'%s'),", snap_id, r.id_att,
                     r.dim_x, r.value.c_str());
            v.sp1 += row_buf; break;
        case stSp2:
            snprintf(row_buf, sizeof(row_buf), "(%d,%d,%d,'%s','%s'),", snap_id, r.id_att,
                     r.dim_x, r.value.c_str(), r.setpoint.empty() ? r.value.c_str() : r.setpoint.c_str());
            v.sp2 += row_buf; break;
        default: break;
        }
    }

    auto do_insert = [&](const std::string &table, std::string &vals) {
        if(vals.empty()) return;
        if(!vals.empty() && vals.back() == ',') vals.pop_back();
        snprintf(q, sizeof(q), "INSERT INTO %s VALUES%s", table.c_str(), vals.c_str());
        Result *r = conn->query(q);
        if(!r || strlen(conn->getError()) > 0)
            d->err += std::string(conn->getError()) + " ";
        if(r) delete r;
    };

    do_insert("t_sc_num_1val", v.num1);
    do_insert("t_sc_num_2val", v.num2);
    do_insert("t_sc_str_1val", v.str1);
    do_insert("t_sc_str_2val", v.str2);
    do_insert("t_sp_1val",     v.sp1);
    do_insert("t_sp_2val",     v.sp2);

    return d->err.empty() ? snap_id : -1;
}

int SnapDbSchema::snap_load(Connection *conn, int snap_id, std::vector<SnapLoadRecord> &data) {
    d->err.clear();
    data.clear();
    char q[1024];

    struct TblInfo {
        const char *table;
        SnapType    type;
        bool        has_setpoint;
        bool        has_dimx;
    };
    static const TblInfo tables[] = {
        { "t_sc_num_1val", stScNum1, false, false },
        { "t_sc_num_2val", stScNum2, true,  false },
        { "t_sc_str_1val", stScStr1, false, false },
        { "t_sc_str_2val", stScStr2, true,  false },
        { "t_sp_1val",     stSp1,    false, true  },
        { "t_sp_2val",     stSp2,    true,  true  },
        { nullptr, stInvalid, false, false }
    };

    for(int ti = 0; tables[ti].table != nullptr; ti++) {
        const TblInfo &ti_ = tables[ti];
        if(ti_.has_dimx && ti_.has_setpoint)
            // t_sp_2val: columns are (id_snap,id_att,dim_x,read_value,write_value)
            snprintf(q, sizeof(q),
                     "SELECT s.id_snap,s.id_att,a.full_name,a.device,s.read_value,s.write_value,s.dim_x,"
                     "a.data_type,a.data_format,a.writable "
                     "FROM %s s JOIN ast a ON s.id_att=a.ID WHERE s.id_snap=%d",
                     ti_.table, snap_id);
        else if(ti_.has_dimx)
            // t_sp_1val: columns are (id_snap,id_att,dim_x,value)
            snprintf(q, sizeof(q),
                     "SELECT s.id_snap,s.id_att,a.full_name,a.device,s.value,'',s.dim_x,"
                     "a.data_type,a.data_format,a.writable "
                     "FROM %s s JOIN ast a ON s.id_att=a.ID WHERE s.id_snap=%d",
                     ti_.table, snap_id);
        else if(ti_.has_setpoint)
            // t_sc_num_2val / t_sc_str_2val: columns are (id_snap,id_att,read_value,write_value)
            snprintf(q, sizeof(q),
                     "SELECT s.id_snap,s.id_att,a.full_name,a.device,s.read_value,s.write_value,0,"
                     "a.data_type,a.data_format,a.writable "
                     "FROM %s s JOIN ast a ON s.id_att=a.ID WHERE s.id_snap=%d",
                     ti_.table, snap_id);
        else
            snprintf(q, sizeof(q),
                     "SELECT s.id_snap,s.id_att,a.full_name,a.device,s.value,'',0,"
                     "a.data_type,a.data_format,a.writable "
                     "FROM %s s JOIN ast a ON s.id_att=a.ID WHERE s.id_snap=%d",
                     ti_.table, snap_id);

        Result *res = conn->query(q);
        if(!res) { d->err = conn->getError(); return (int)data.size(); }
        while(res->next() > 0) {
            Row *row = res->getCurrentRow();
            if(row && row->getFieldCount() == 10) {
                SnapLoadRecord r;
                r.id_snap     = atoi(row->getField(0));
                r.id_att      = atoi(row->getField(1));
                r.full_name   = row->getField(2) ? row->getField(2) : "";
                r.device      = row->getField(3) ? row->getField(3) : "";
                r.value       = row->getField(4) ? row->getField(4) : "NULL";
                r.setpoint    = row->getField(5) ? row->getField(5) : "";
                r.dim_x       = atoi(row->getField(6));
                r.data_type   = (char)atoi(row->getField(7));
                r.data_format = (char)atoi(row->getField(8));
                r.writable    = (char)atoi(row->getField(9));
                r.snap_type   = ti_.type;
                data.push_back(r);
                delete row;
            }
        }
        delete res;
    }
    return (int)data.size();
}

int SnapDbSchema::snap_query_by_atts(Connection *conn, const std::vector<std::string> &atts,
                                      std::vector<AttSnapRecord> &results) {
    d->err.clear();
    results.clear();
    if(atts.empty()) return 0;

    // Build IN clause: ('att1','att2',...)
    std::string in_list;
    for(size_t i = 0; i < atts.size(); i++) {
        if(i > 0) in_list += ",";
        in_list += "'" + atts[i] + "'";
    }

    struct TblInfo {
        const char *table;
        SnapType    type;
        bool        has_setpoint;
        bool        has_dimx;
    };
    static const TblInfo tables[] = {
        { "t_sc_num_1val", stScNum1, false, false },
        { "t_sc_num_2val", stScNum2, true,  false },
        { "t_sc_str_1val", stScStr1, false, false },
        { "t_sc_str_2val", stScStr2, true,  false },
        { "t_sp_1val",     stSp1,    false, true  },
        { "t_sp_2val",     stSp2,    true,  true  },
        { nullptr, stInvalid, false, false }
    };

    for(int ti = 0; tables[ti].table != nullptr; ti++) {
        const TblInfo &tbl = tables[ti];
        // Columns: ctx_id, ctx_name, snap_id, snap_time, snap_comment,
        //          full_name, data_type, data_format, writable,
        //          value, setpoint, dim_x  (12 total)
        std::string q;
        q.reserve(512);
        q = "SELECT c.id_context,c.name,sn.id_snap,sn.time,sn.snap_comment,"
            "a.full_name,a.data_type,a.data_format,a.writable,";
        if(tbl.has_setpoint && tbl.has_dimx)
            q += "v.read_value,v.write_value,v.dim_x";
        else if(tbl.has_setpoint)
            q += "v.read_value,v.write_value,0";
        else if(tbl.has_dimx)
            q += "v.value,'',v.dim_x";
        else
            q += "v.value,'',0";
        q += std::string(" FROM ") + tbl.table +
             " v JOIN ast a ON v.id_att=a.ID"
             " JOIN list l ON a.ID=l.id_att"
             " JOIN context c ON l.id_context=c.id_context"
             " JOIN snapshot sn ON sn.id_snap=v.id_snap AND sn.id_context=c.id_context"
             " WHERE a.full_name IN (" + in_list + ")"
             " ORDER BY a.full_name,sn.time DESC";

        Result *res = conn->query(q.c_str());
        if(!res) { d->err = conn->getError(); return (int)results.size(); }
        while(res->next() > 0) {
            Row *row = res->getCurrentRow();
            if(row && row->getFieldCount() == 12) {
                AttSnapRecord r;
                r.ctx_id      = atoi(row->getField(0));
                r.ctx_name    = row->getField(1) ? row->getField(1) : "";
                r.snap_id     = atoi(row->getField(2));
                r.snap_time   = row->getField(3) ? row->getField(3) : "";
                r.snap_comment= row->getField(4) ? row->getField(4) : "";
                r.full_name   = row->getField(5) ? row->getField(5) : "";
                r.data_type   = (char)atoi(row->getField(6));
                r.data_format = (char)atoi(row->getField(7));
                r.writable    = (char)atoi(row->getField(8));
                r.value       = row->getField(9)  ? row->getField(9)  : "NULL";
                r.setpoint    = row->getField(10) ? row->getField(10) : "";
                r.dim_x       = atoi(row->getField(11));
                r.snap_type   = tbl.type;
                results.push_back(r);
                delete row;
            }
        }
        delete res;
    }
    return (int)results.size();
}

std::string SnapDbSchema::error() const {
    return d->err;
}

std::string SnapDbSchema::warning() const
{
    return d->warn;
}

