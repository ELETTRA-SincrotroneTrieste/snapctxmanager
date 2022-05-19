#ifndef SNAPCTXMAN_P_H
#define SNAPCTXMAN_P_H

#include "snapctxmanager.h" /* for db and schema type enum values */

#define MAXERRORLEN 512

class Connection;
class SnapDbSchema;
class SnapCtxManListener;
class DbSettings;
class SnapDbSchemaListener;

class SnapCtxMan_P
{
public:

    SnapCtxManager::DbType dbType;

    Connection * connection;
    SnapCtxManListener* snactxman_listener;

    DbSettings *hdbxSettings;
    SnapDbSchema *dbschema;
    SnapDbSchemaListener *dbsl;

    std::string msg;
};

#endif // HDBEXTRACTORPRIVATE_H
