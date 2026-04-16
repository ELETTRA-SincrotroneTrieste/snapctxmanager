#ifndef SNAPDBSCHEMA_H
#define SNAPDBSCHEMA_H

#include <string>
#include <map>
#include <vector>

class Connection;

/** \brief Mirrors AttInfo::Type from saverestore without Qt/Tango dependency.
 *
 *  Values are kept equal to AttInfo::Type so that the two can be cast freely.
 *  Im1=1, Im2=2, scNum1=3, scNum2=4, scStr1=5, scStr2=6, sp1=7, sp2=8
 */
enum SnapType {
    stIm1 = 1, stIm2,
    stScNum1, stScNum2,
    stScStr1, stScStr2,
    stSp1,    stSp2,
    stInvalid
};

/** \brief Determine the snapshot table type from raw Tango integer codes stored in ast.
 *
 *  Tango constants used here:
 *    SCALAR=0, SPECTRUM=1, IMAGE=2
 *    READ=1, WRITE=2, READ_WRITE=3, READ_WITH_WRITE=4
 *    DEV_STRING=8, DEV_ENCODED=29
 */
inline SnapType snap_type_from_ast(int data_type, int data_format, int writable) {
    if(data_format == 1) // SPECTRUM
        return (writable == 3 || writable == 4) ? stSp2 : stSp1;
    else if(data_format == 2) // IMAGE
        return (writable == 3 || writable == 4) ? stIm2 : stIm1;
    else if(data_format == 0 && data_type == 8) // SCALAR, DEV_STRING
        return (writable == 3 || writable == 4) ? stScStr2 : stScStr1;
    else if(data_format == 0 && data_type > 0 && data_type < 29) // SCALAR, numeric
        return (writable == 3 || writable == 4) ? stScNum2 : stScNum1;
    return stInvalid;
}

/** \brief Snapshot row from the snapshot table. */
class Snapshot {
public:
    Snapshot() : id_snap(-1), id_context(-1) {}
    int id_snap;
    int id_context;
    std::string time;
    std::string comment;
};

/** \brief Record to be saved into one of the value tables (t_sc_num_1val, etc.).
 *
 *  Set \a value to the string "NULL" (or leave \a is_null=true) to skip the
 *  insert — this matches what the Qt saverestore app does for read errors.
 */
class SnapSaveRecord {
public:
    SnapSaveRecord() : id_att(-1), dim_x(0), snap_type(stInvalid) {}
    int         id_att;
    std::string value;      ///< numeric or text value; "NULL" = error/skip
    std::string setpoint;   ///< setpoint for *2 types; empty/NULL to skip
    int         dim_x;      ///< spectrum element count (sp1/sp2 only)
    SnapType    snap_type;
};

/** \brief Record loaded from the snapshot value tables, joined with ast. */
class SnapLoadRecord {
public:
    SnapLoadRecord() : id_snap(-1), id_att(-1), dim_x(0), snap_type(stInvalid),
        data_type(0), data_format(0), writable(0) {}
    int         id_snap;
    int         id_att;
    std::string full_name;
    std::string device;
    std::string value;
    std::string setpoint;
    int         dim_x;
    SnapType    snap_type;
    char        data_type;
    char        data_format;
    char        writable;
};

/** \brief Result row from a cross-context attribute snapshot query. */
class AttSnapRecord {
public:
    AttSnapRecord() : ctx_id(-1), snap_id(-1), dim_x(0), snap_type(stInvalid),
        data_type(0), data_format(0), writable(0) {}
    std::string full_name;
    int         ctx_id;
    std::string ctx_name;
    int         snap_id;
    std::string snap_time;
    std::string snap_comment;
    std::string value;
    std::string setpoint;
    int         dim_x;
    SnapType    snap_type;
    char        data_type;
    char        data_format;
    char        writable;
};

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


/** \brief Device info record for the Elettra-specific selection dialog. */
class DeviceInfo {
public:
    std::string device, section, type;
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
    int srcs_remove(Connection *conn, const std::string& ctxnam, const std::vector<std::string> & srcs,
                    bool purge_data = false);
    std::map<std::string, std::vector<Context> > get_contexts_with_atts(Connection *conn, const std::vector<std::string> &atts);
    int rename(Connection *conn, const std::vector<std::string>& olda, const std::vector<Ast> &v);
    bool get_context(Connection *conn, const std::string& id_or_nam, Context &ctx, std::vector<Ast>& v);

    /** \brief Fetch all attributes for a context by numeric id, including archivable, substitute and levelg.
     *  Unlike get_context(), this takes the numeric id directly and populates all Ast fields from the ast table.
     *  Returns true on success. */
    bool get_context_atts(Connection *conn, int ctx_id, std::vector<Ast> &atts);

    /** \brief Like get_context() but orders attributes by section_order position.
     *  Falls back to full_name ordering if section_order table does not exist. */
    bool get_context_sorted(Connection *conn, const std::string& id_or_nam, Context &ctx, std::vector<Ast>& v);

    /** \brief Returns machine sections in display order from the section_order table.
     *  Returns an empty vector if the table does not exist. */
    std::vector<std::string> section_order_list(Connection *conn);

    /** \brief Returns attribute type names for a context, ordered by type_order.position.
     *  Uses the Elettra-specific selection, type_order tables.
     *  Returns an empty vector if the tables do not exist. */
    std::vector<std::string> selection_type_order(Connection *conn, int ctx_id);

    /** \brief Returns device info (device, section, type) for a context.
     *  Uses Elettra-specific devices, selection, section_order tables.
     *  Returns an empty vector if the tables do not exist. */
    std::vector<DeviceInfo> selection_devices(Connection *conn, int ctx_id);

    int search(Connection *conn, const std::string &search, std::vector<Context> &ctxs);
    std::vector<Context> ctxlist(Connection *conn);

    // --- snapshot operations ---

    /** Fetch a single snapshot row by snap id. Returns true if found. */
    bool get_snapshot(Connection *conn, int snap_id, Snapshot &snap);

    /** List snapshots for the given context id, ordered by time DESC. */
    int snap_list(Connection *conn, int context_id, std::vector<Snapshot> &snaps);

    /** Create a snapshot row and insert all records into the value tables.
     *  Records whose value equals "NULL" are silently skipped (read-error behaviour).
     *  Returns the new id_snap on success, -1 on error. */
    int snap_save(Connection *conn, int context_id, const std::string &comment,
                  const std::vector<SnapSaveRecord> &data);

    /** Load all attribute values for snapshot \a snap_id from all value tables.
     *  Joins with ast to populate full_name/device/data_type/etc.
     *  Returns the number of records loaded. */
    int snap_load(Connection *conn, int snap_id, std::vector<SnapLoadRecord> &data);

    /** Query stored values for a list of attribute full names across all contexts and snapshots.
     *  Each result row includes context, snapshot id/time/comment, value, and setpoint.
     *  Returns the number of records found. */
    int snap_query_by_atts(Connection *conn, const std::vector<std::string>& atts,
                           std::vector<AttSnapRecord>& results);

    std::string error() const;
    std::string warning() const;


private:
    SnapDbSchemaP *d;
};

#endif // SNAPDBSCHEMA_H
