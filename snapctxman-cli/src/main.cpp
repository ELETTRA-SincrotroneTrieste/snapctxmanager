#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <hdbxsettings.h>
#include <snapctxmanager.h>
#include <tgutils.h>
#include <hdbxmacros.h>

#include <getopt.h>
#include <sstream>
#include <iostream>
#include <map>

#include "utils.h"

using namespace std;


std::vector<std::string> split(const std::string& s, char sep) {
    std::vector<std::string> v;
    std::istringstream f(s);
    string x;
    while (getline(f, x, sep)) {
        v.push_back(x);
    }
    return v;
}

int main(int argc, char **argv)
{
    HdbXSettings *qc = nullptr;
    int ch;
    std::string dbc, snapc; // db conf snap conf file names
    std::string name, author, reason, description, search;
    bool remove = false, configure = false;
    bool ok = true;

    Utils uti;
    if(ok) {
        while ((ch = getopt(argc, argv, "n:a:r:d:c:f:s:Dih")) != -1) {
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
            case 'D':
                remove = true;
                break;
            case 's':
                search = std::string(optarg);
                break;
            case 'i':
                configure  = true;
                break;
            case '?':
            case 'h':
            default:
                uti.usage(argv[0]);
                break;
            }
        }

        if(!uti.conf_file_exists() && !configure) {
            perr("-c conf_file not specified and %s does not exist", uti.conf_dir().c_str());
            printf("\e[1;32;4mhint\e[0m: call %s -i to configure the application\n", argv[0]);
        }
        else if(configure && uti.configure()) {
            printf("configuration written under \e[1;37;2m%s\e[0m\n", uti.conf_file_path().c_str());
        }
        else if(configure)  { // error
            perr("error writing configuration into \e[1;37;2m%s\e[0m\n", uti.conf_file_path().c_str());
        }
        else {

            ok = snapc.length() > 0;
            if(!ok)
                ok = name.length() && remove && dbc.length();
            if(!ok)
                ok = search.length() > 0 && dbc.length();
            if(!ok)
                ok = name.length() > 0 && search.length() == 0 && author.length() == 0 && reason.length() == 0 && description.length() == 0;
            if(dbc.length() == 0)
                perr("-c db-conf-file is mandatory to connect to the database");
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
                    std::vector <std::string> srcs;
                    if(snapc.length() > 0 && snapc.find(",") == std::string::npos)
                        srcs = scm->ctx_load(snapc.c_str());
                    else if(snapc.length() > 0)
                        srcs = split(snapc, ',');

                    if(scm->error().length() > 0) {
                        perr("%s: failed to load context from configuration file \"%s\": %s",
                             argv[0], snapc.c_str(), scm->error().c_str());
                    }
                    else {
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
                            if(srcs.size() > 0 && !remove && name.length() > 0 && author.length() > 0 && (reason.length() > 0 || description.length() > 0)) {
                                Context ctx(name, author, reason, description);
                                int r = scm->register_context(ctx, srcs);
                                if(r <= 0)
                                    perr("failed to register context '%s': %s", name.c_str(), scm->error().c_str());
                                printf("%d database rows affected registering context \e[1;32m'%s' with %ld sources\e[0m\n",
                                       r, name.c_str(), srcs.size());
                            }
                            else if(remove && srcs.size() > 0 && name.length()) {
                                int r = scm->remove_from_ctx(name, srcs);
                                if(r <= 0)
                                    perr("failed to remove %d attributes from context '%s': %s",  srcs.size(), name.c_str(), scm->error().c_str());
                                printf("%d database rows affected removing \e[1;31m %ld srcs from %s\e[0m\n", r, srcs.size(), name.c_str());
                            }
                            else if(remove && name.length() > 0) {
                                int r = scm->remove_ctx(name);
                                if(r <= 0)
                                    perr("failed to remove context '%s': %s", name.c_str(), scm->error().c_str());
                                printf("%d database rows affected removing \e[1;31m%s\e[0m\n", r, name.c_str());
                            }
                            else if(!remove && name.length()) {
                                Context c;
                                std::vector<Ast> va;
                                bool found = scm->get_context(name, c, va);
                                if(scm->error().length() > 0)
                                    perr(scm->error().c_str());
                                else if(found)
                                    uti.out_ctx(c, va);
                                else
                                    printf("\e[1;35m!\e[0m context '%s' not found.\n", name.c_str());
                            }
                            else if(search.length() > 0) {
                                std::vector<Context> ctxs;
                                int found = scm->search(search, ctxs);
                                if(scm->error().length() > 0)
                                    perr(scm->error().c_str());
                                else if(found > 0)
                                    uti.out_ctxs(ctxs);
                                printf("\e[0;36m%d contexts matched search '%s'\e[0m\n", found, search.c_str());
                            }
                        }
                    }

                }
            }
        }
    }
    if(!ok) {
        uti.usage(argv[0]);
    }
}

