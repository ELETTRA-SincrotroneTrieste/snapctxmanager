#include "snapctxmanager.h"
#include "snapctxmanager_p.h"
#include "snapctxman-listener.h"
#include "snapdbschema.h"
#include "tgutils.h"
#include "ctxfileloader.h"
#include <mysqlconnection.h>
#include <result.h>
#include <timeinterval.h>
#include <dbmacros.h>
#include <dbsettings.h>
#include <string.h>


SnapCtxManager::~SnapCtxManager()
{
    pdelete("SnapCtxManager %p", this);
    if(d->connection != NULL)
        d->connection->close();
    if(d->dbschema)
        delete d->dbschema;
    if(d->connection)
        delete d->connection;
    /* Data is deleted by the HdbExtractorPrivate destructor */
    delete d;
}

SnapCtxManager::DbType SnapCtxManager::dbType() const
{
    return d->dbType;
}

SnapCtxManager::SnapCtxManager(SnapDbSchemaListener *sl)
{
    d = new SnapCtxMan_P();
    d->connection = nullptr;
    d->dbschema = nullptr;
    d->dbType = DBUNDEFINED;
    d->hdbxSettings = NULL;
    d->dbsl = sl;
}

/** \brief This method is used to create a network connection with the database.
 *
 */
bool SnapCtxManager::connect(DbType dbType,
                             const char *host,
                             const char *db,
                             const char *user,
                             const char *passwd,
                             unsigned short port)
{
    bool success = false;
    d->msg.clear();
    switch(dbType)
    {
    case SNAPDBMYSQL:
        d->connection = new MySqlConnection();
        success = d->connection->connect(host, db, user, passwd, port);
        break;
    default:
        d->msg = "HdbExtractor: connect: database type unsupported";
        break;
    }
    d->dbType = dbType;

    if(!success && d->connection != nullptr)
        d->msg = d->connection->getError();

    if(!success)
        perr("%s", d->msg.c_str());
    else
        d->dbschema = new SnapDbSchema();

    return success;
}

std::vector<std::string> SnapCtxManager::ctx_load(const char *filenam) const {
    CtxFileLoader fl;
    std::vector<std::string> srcs = fl.load(filenam);
    d->msg = fl.error;
    return srcs;
}

bool SnapCtxManager::connect()
{
    int port = 3306;
    bool ok;
    const DbSettings *qc = d->hdbxSettings;
    DbType dbt;
    std::string dbty;
    if(qc->hasKey("dbtype"))  // db type explicitly set
        dbty = qc->get("dbtype");
    else if(qc->hasKey("dbname")) // try to guess from dbname
        dbty = qc->get("dbname");
    if(qc->hasKey("dbport") && qc->getInt("dbport", &ok) && ok)
        port = qc->getInt("dbport", &ok);
    if(dbty == "snapdbmysql" || dbty.empty()) {
        dbt = SNAPDBMYSQL;
        printf("calling connect with %s %s %s %s\n", qc->get("dbhost").c_str(),
               qc->get("dbname").c_str(), qc->get("dbuser").c_str(),
               qc->get("dbpass").c_str());
        return connect(dbt, qc->get("dbhost").c_str(),
                       qc->get("dbname").c_str(), qc->get("dbuser").c_str(),
                       qc->get("dbpass").c_str(), port);
    }
    else
        d->msg = "invalid database type " + dbty;
    return false;
}

void SnapCtxManager::disconnect()
{
    if(d->connection != NULL && d->connection->isConnected())
        d->connection->close();
}

std::string  SnapCtxManager::error() const {
    return d->msg;
}

std::string SnapCtxManager::warning() const {
    return d->dbschema != nullptr ? d->dbschema->warning() : "";
}

bool  SnapCtxManager::hasError() const {
    return d->msg.length() > 0;
}

int SnapCtxManager::register_context(const Context& c,
                                     const std::vector<std::string> &srcs) {
    int nrows = 0;
    if(!d->dbschema)
        return nrows;
    TgUtils tanu;
    std::vector<Ast> vast = tanu.get(srcs);
    if(tanu.err.length() > 0) {
        d->msg = tanu.err;
    }
    else {
        int id = d->dbschema->register_context(d->connection, c.name, c.author, c.reason, c.description);
        if(id > 0) {
            nrows++; // one row added to the context table
            // id stores the new context id
            nrows += d->dbschema->link_attributes(d->connection, id, vast);
        }
        d->msg = d->dbschema->error();
    }
    return nrows;
}

int SnapCtxManager::remove_from_ctx(const std::string &ctxnam, const std::vector<std::string> &srcs) {
    d->msg.clear();
    int r = -1;
    if(!d->dbschema)
        return r;
    r = d->dbschema->srcs_remove(d->connection, ctxnam, srcs);
    d->msg = d->dbschema->error();
    return r;
}

int SnapCtxManager::add_to_ctx(const std::string &ctxnam, const std::vector<std::string> &srcs) {
    int r = 0;
    if(!d->dbschema)
        return -1;
    TgUtils tanu;
    // get attribute properties for each src from Tango DB
    std::vector<Ast> vast = tanu.get(srcs);
    if(tanu.err.length() > 0) {
        d->msg = tanu.err;
    }
    else {
        int ctx = d->dbschema->ctx_id(d->connection, ctxnam);
        if(ctx > 0) { // context id for ctxnam > 0
            r += d->dbschema->link_attributes(d->connection, ctx, vast);
            d->msg = d->dbschema->error();
        }
    }
    return r;
}

int SnapCtxManager::remove_ctx(const std::string &ctxnam) {
    if(!d->dbschema)
        return -1;
    int r = d->dbschema->ctx_remove(d->connection, ctxnam);
    if(r <= 0)
        d->msg = d->dbschema->error();
    return r;
}

bool SnapCtxManager::get_context(const std::string& id_or_nam, Context &ctx, std::vector<Ast>& v) {
    d->msg.clear();
    bool ok = (d->dbschema != nullptr);
    if(d->dbschema) {
        ok = d->dbschema->get_context(d->connection, id_or_nam, ctx, v);
        d->msg = d->dbschema->error();
    }
    return ok;
}

int SnapCtxManager::search(const std::string &search, std::vector<Context> &ctxs) {
    d->msg.clear();
    int cnt = 0;
    if(d->dbschema) {
        cnt = d->dbschema->search(d->connection, search, ctxs);
        d->msg = d->dbschema->error();
    }
    return cnt;
}

std::vector<Context> SnapCtxManager::ctxlist() {
    std::vector<Context> ctxs;
    d->msg.clear();
    int cnt = 0;
    if(d->dbschema) {
        ctxs = d->dbschema->ctxlist(d->connection);
        d->msg = d->dbschema->error();
    }
    return ctxs;
}

bool SnapCtxManager::query(const char *query, Result *&result, double *elapsed) {
    bool success = false;
    d->msg.clear();
    if(d->connection != NULL && d->connection->isConnected()) {
    }
    return success;
}

void SnapCtxManager::setDbSchemaListener(SnapDbSchemaListener *l) {
    d->dbsl = l;
}

bool SnapCtxManager::isConnected() const {
    return d->connection != NULL;
}

/** \brief Implements ResultListener::onProgressUpdate interface
 *
 */
void SnapCtxManager::onProgress(double percent) {
    d->snactxman_listener->onProgress(percent);
}

/** \brief Implements ResultListener::onFinished interface
 *
 */
void SnapCtxManager::onFinished(double elapsed) {
    d->snactxman_listener->onCtxCreated(elapsed);
}


