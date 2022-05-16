#include "snapctxmanager.h"
#include "snapctxmanager_p.h"
#include "snapctxman-listener.h"
#include "snapdbschema.h"
#include "ctxfileloader.h"
#include <mysqlconnection.h>
#include <result.h>
#include <timeinterval.h>
#include <hdbxmacros.h>
#include <hdbxsettings.h>
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
    const HdbXSettings *qc = d->hdbxSettings;
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

bool  SnapCtxManager::hasError() const {
    return d->msg.length() > 0;
}

int SnapCtxManager::register_context(const Context& c,
                                     const std::vector<std::string> &srcs) {
    int nrows = 0;
    if(!d->dbschema)  {
        d->dbschema = new SnapDbSchema();
    }
    int id = d->dbschema->register_context(d->connection, c.name, c.author, c.reason, c.description);
    if(id > 0) {
        // id stores the new context id
        for(const std::string& s : srcs) {

        }
    }
    else
        d->msg = "error registering context \"" + c.name + "\": " + std::string(d->connection->getError());
    return nrows;
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


