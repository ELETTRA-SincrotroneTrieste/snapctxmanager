#ifndef SNAPCTXMANAGER_H
#define SNAPCTXMANAGER_H

#include <xvariantlist.h>
#include <snapdbschema.h>

#include <vector>
#include <string>
#include <list>
#include <map>

class SnapCtxMan_P;
class SnapCtxManListener;
class HdbXSettings;
class TimeInterval;
class Result;

class SnapCtxManager :  public SnapDbSchemaListener
{
public:

    enum DbType { DBUNDEFINED = -1, SNAPDBMYSQL };

    SnapCtxManager(SnapDbSchemaListener *sl = nullptr);
    virtual ~SnapCtxManager();

    DbType dbType() const;

    /** \brief Try to establish a database connection with the specified host, user, password and database name
     *
     * @param dbType A type of database as defined in the DbType enum
     * @param host the internet host name of the database server
     * @param user the user name of the database
     * @param passwd the password for that username
     * @param port the database server port (default 3306, thought for mysql)
     *
     * @return false upon failure, true otherwise
     */
    bool connect(DbType dbType, const char* host, const char *db, const char* user,
                 const char* passwd, unsigned short port = 3306);
    std::vector<std::string> ctx_load(const char *filenam) const;

    bool connect();
    void disconnect();
    bool isConnected() const;

    bool query(const char *query, Result* &result, double *elapsed = NULL);

    void setDbSchemaListener(SnapDbSchemaListener *l);

    virtual void onProgress(double percent);
    virtual void onFinished(double elapsed);

    std::string error() const;
    bool hasError() const;

    int register_context(const Context &c, const std::vector<std::string> & srcs);

private:
    SnapCtxMan_P *d;
};

#endif // HDBEXTRACTOR_H
