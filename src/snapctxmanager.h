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
class DbSettings;
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
    std::string warning() const;
    bool hasError() const;

    int register_context(const Context &c, const std::vector<std::string> & srcs);
    int remove_from_ctx(const std::string& ctxnam, const std::vector<std::string> & srcs,
                        bool purge_data = false);
    int add_to_ctx(const std::string& ctxnam, const std::vector<std::string> & srcs);
    int remove_ctx(const std::string& ctxnam);
    bool get_context(const std::string &id_or_nam, Context &ctx, std::vector<Ast>& v);

    /** \brief get_context variant that orders attributes by section_order position (Elettra-specific). */
    bool get_context_sorted(const std::string &id_or_nam, Context &ctx, std::vector<Ast>& v);

    /** \brief Returns machine sections in display order from the section_order table. */
    std::vector<std::string> section_order_list();

    /** \brief Returns ordered attribute type names for a context (Elettra-specific selection/type_order tables). */
    std::vector<std::string> selection_type_order(int ctx_id);

    /** \brief Returns device info records for a context (Elettra-specific devices/selection tables). */
    std::vector<DeviceInfo> selection_devices(int ctx_id);

    int search(const std::string& search, std::vector<Context> &ctxs);
    std::map<std::string, std::vector<Context>> get_contexts_with_atts(const std::vector<std::string> &atts);
    int rename(const std::string& ctxnam, const std::vector<std::string>& olda, const std::vector<std::string> &newa);
    std::vector<Context> ctxlist();

    // --- snapshot operations ---
    int snap_list(int context_id, std::vector<Snapshot> &snaps);
    int snap_save(int context_id, const std::string &comment, const std::vector<SnapSaveRecord> &data);
    int snap_load(int snap_id, std::vector<SnapLoadRecord> &data);
    int snap_query_by_atts(const std::vector<std::string>& atts, std::vector<AttSnapRecord>& results);

private:
    SnapCtxMan_P *d;
};

#endif // SNAPCTXMANAGER_H
