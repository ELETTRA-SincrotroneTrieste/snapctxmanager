#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <hdbxsettings.h>
#include <snapctxmanager.h>
#include <hdbxmacros.h>

#include <getopt.h>
#include <map>

using namespace std;

void print_usage(const char *appnam) {
    printf("\e[1;31mUsage\e[0m \"%s -n ctxname -a author -r \"a reason\" -c dbconf -f ctxfile [-d \"a description\"] \n"
           "At least one of \e[1;37;4mreason\e[0m or \e[1;37;4mdescription\e[0m must be specified", appnam);
}

int main(int argc, char **argv)
{
    HdbXSettings *qc = nullptr;
    int ch;
    std::string dbc, snapc; // db conf snap conf file names
    std::string name, author, reason, description;
    bool ok = true;
    if(ok) {
        while ((ch = getopt(argc, argv, "n:a:r:d:c:f:h")) != -1) {
            switch (ch) {
            case 'c':
                dbc = std::string(optarg);
                break;
            case 'f':
                snapc = std::string(optarg);
                break;
            case 'n':
                name = std::string(optarg);
                break;
            case 'a':
                author = std::string(optarg);
                break;
            case 'r':
                reason = std::string(optarg);
                break;
            case 'd':
                description = std::string(optarg);
                break;
            case '?':
            case 'h':
            default:
                print_usage(argv[0]);
                break;
            }
        }
        ok = name.length() > 0 && author.length() > 0 && (reason.length() > 0 || description.length()> 0) && dbc.length() > 0 && snapc.length() > 0;
        if(ok) {
            qc = new HdbXSettings();
            qc->loadFromFile(dbc.c_str());
            if(qc->hasError())
                perr("%s", qc->getError().c_str());
            else {
                SnapCtxManager *scm = new SnapCtxManager();
                // either reason or description or both
                if(reason.length() == 0) reason = description;
                if(description.length() == 0) description = reason;
                //
                std::vector <std::string> srcs = scm->ctx_load(snapc.c_str());
                if(scm->error().length() > 0) {
                    perr("%s: failed to load context from configuration file \"%s\": %s",
                         argv[0], snapc.c_str(), scm->error().c_str());
                }
                else {
                    printf("\e[1;32m*\e[0m context information loaded from %s\n", snapc.c_str());
                    for(size_t i = 0; i < srcs.size(); i++)
                        printf(" %ld. %s\n", i+1, srcs[i].c_str());
                    bool conn = scm->connect(SnapCtxManager::SNAPDBMYSQL,
                                             qc->get("dbhost").c_str(),
                                             qc->get("dbname").c_str(),
                                             qc->get("dbuser").c_str(),
                                             qc->get("dbpass").c_str());
                    if(!conn) {
                        perr("%s: error connecting to database %s on %s user %s: %s",
                             argv[0], qc->get("dbname").c_str(), qc->get("dbhost").c_str(),
                                qc->get("dbuser").c_str(), scm->error().c_str());
                    }
                    else {
                        printf("\e[1;32m* \e[0mconnected to %s user %s db %s\n",
                               qc->get("dbname").c_str(), qc->get("dbhost").c_str(),
                               qc->get("dbuser").c_str());
                        if(srcs.size() > 0) {
                            Context ctx(name, author, reason, description);
                            scm->register_context(ctx, srcs);
                        }
                    }
                }

            }
        }
    }
    if(!ok) {
        print_usage(argv[0]);
    }
}

