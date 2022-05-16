#ifndef HDBEXTRACTORLISTENER_H
#define HDBEXTRACTORLISTENER_H

class XVariantList;

/** \brief An interface defining methods to obtain results from the query as long as they are available,
 *         also partially. Its methods are thread safe.
 *
 */
class SnapCtxManListener
{
public:

    virtual void onCtxCreated(double elapsed) = 0;
    virtual void onProgress(double percent) = 0;

    virtual ~SnapCtxManListener() {}
};

#endif // HDBEXTRACTORLISTENER_H
