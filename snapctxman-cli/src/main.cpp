#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dbsettings.h>
#include <snapctxmanager.h>
#include <tgutils.h>
#include <dbmacros.h>

#include <getopt.h>
#include <sstream>
#include <iostream>
#include <map>
#include <chrono>

#include "utils.h"

using namespace std;

auto t1 = std::chrono::high_resolution_clock::now();

long duration() {
    auto t2 = std::chrono::high_resolution_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count();
}

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
    DbSettings *qc = nullptr;
    int ch;
    std::string dbc, snapc; // db conf snap conf file names
    std::string name, author, reason, description, search, renatts;
    bool remove = false, configure = false, add = false;
    bool op = false;
    bool ctxlist = false; // list contexts
    Utils uti;
    while ((ch = getopt(argc, argv, "n:a:r:d:c:f:s:R:ADilh")) != -1) {
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
        case 'l':
            ctxlist = true;
            break;
        case 'A':
            add = true;
            break;
        case 'R':
            renatts = std::string(optarg);
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
        printf("\033[1;32;4mhint\033[0m: call %s -i to configure the application\n", argv[0]);
    }
    else if(configure) {
        op = true;
        int r = uti.configure();
        if(r == 0)
            printf("configuration written under \033[1;37;2m%s\033[0m\n", uti.conf_file_path().c_str());
        else if(r < 0)
            perr("configuration unchanged due to invalid parameters");
        else
            perr("error writing configuration into \033[1;37;2m%s\033[0m\n", uti.conf_file_path().c_str());
    }
    else {
        dbc = uti.conf_file_path();
        qc = new DbSettings();
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
                    op = true;
                    perr("%s: error connecting to database %s on %s user %s: %s",
                         argv[0], qc->get("dbname").c_str(), qc->get("dbhost").c_str(),
                            qc->get("dbuser").c_str(), scm->error().c_str());
                }
                else {
                    if(srcs.size() > 0 && !remove && !add && name.length() > 0 && author.length() > 0 && (reason.length() > 0 || description.length() > 0)) {
                        op = true;
                        Context ctx(name, author, reason, description);
                        int r = scm->register_context(ctx, srcs);
                        if(r <= 0)
                            perr("failed to register context '%s': %s", name.c_str(), scm->error().c_str());
                        printf("%d database rows affected registering context \033[1;32m'%s' with %ld sources\n",
                               r, name.c_str(), srcs.size());
                        if(r > 0)
                            printf("\033[1;32;4mhint\033[0m: call %s -n \"%s\" to see context details \033[0;36m[took %ldms]\033[0m\n",
                                   argv[0], name.c_str(), duration());

                    }
                    else if(!add && remove && srcs.size() > 0 && name.length()) {
                        op = true;
                        int r = scm->remove_from_ctx(name, srcs);
                        if(r <= 0)
                            perr("failed to remove %ld attributes from context '%s': %s",  srcs.size(), name.c_str(), scm->error().c_str());
                        printf("%d database rows affected removing \033[1;31m %ld srcs from %s \033[0;36m[took %ldms]\033[0m\n",
                               r, srcs.size(), name.c_str(), duration());
                    }
                    else if(!add && remove && name.length() > 0) {
                        op = true;
                        int r = scm->remove_ctx(name);
                        if(r <= 0)
                            perr("failed to remove context '%s': %s", name.c_str(), scm->error().c_str());
                        printf("%d database rows affected removing \033[1;31m%s \033[0;36m[took %ldms]\033[0m\n",
                               r, name.c_str(), duration());
                    }
                    else if(!add && !remove && name.length()) {
                        op = true;
                        Context c;
                        std::vector<Ast> va;
                        bool found = scm->get_context(name, c, va);
                        if(scm->error().length() > 0)
                            perr("%s", scm->error().c_str());
                        else if(found) {
                            uti.out_ctx(c, va);
                            printf("-\ncontext \033[1;36;3m%s\033[0m has %ld attributes \033[0;36m[took %ldms]\033[0m\n",
                                   name.c_str(), va.size(), duration());
                        }
                        else
                            printf("\033[1;35m!\033[0m context '%s' not found.\n", name.c_str());
                    }
                    else if(!add && !remove && search.length() > 0) {
                        op = true;
                        std::vector<Context> ctxs;
                        int found = scm->search(search, ctxs);
                        if(scm->error().length() > 0)
                            perr("%s", scm->error().c_str());
                        else if(found > 0) {
                            uti.out_ctxs(ctxs);
                            printf("\033[1;32m%d\033[0m contexts matched search '%s'\n", found, search.c_str());
                            printf("\033[1;32;4mhint\033[0m: call %s -n \"%s\" to see context details \033[0;36m[took %ldms]\033[0m\n",
                                   argv[0], ctxs[0].name.c_str(), duration());
                        }
                        else {
                            printf("\033[1;35m!\033[0m no contexts matching '\033[1;35;3m%s\033[0m' \033[0;36m[took %ldms]\033[0m\n",
                                   search.c_str(), duration());
                        }
                    }
                    else if(ctxlist && !add && !remove && search.length() == 0 && name.length() == 0) {
                        op = true;
                        std::vector<Context> ctxs = scm->ctxlist();
                        if(scm->error().length() > 0)
                            perr("%s", scm->error().c_str());
                        else {
                            uti.out_ctxs(ctxs);
                            printf("\033[1;32m%ld\033[0m context info fetched in %ldms\n", ctxs.size(), duration());
                        }
                    }
                    else if(add && srcs.size() > 0 && name.length() > 0) {
                        op = true;
                        size_t r = scm->add_to_ctx(name, srcs);
                        if(scm->error().length() > 0)
                            perr("%s", scm->error().c_str());
                        else if(r >= 0) {
                            printf("%ld sources added to context \033[1;32;3m%s\033[0m over %ld requested\n",
                                   r, name.c_str(), srcs.size());
                            printf("\033[1;32;4mhint\033[0m: call %s -n \"%s\" to see context details \033[0;36m[took %ldms]\033[0m\n",
                                   argv[0], name.c_str(), duration());
                        }
                        if(scm->warning().length() > 0)
                            printf("\033[1;33;2mWARNING\033[0m: %s\n", scm->warning().c_str());
                    }
                    else if(renatts.length() > 0 && srcs.size() > 0 && name.length() > 0) {
                        std::vector<std::string> ra = split(renatts, ',');
                        scm->rename(name, srcs, ra);
                        if(srcs.size() == ra.size()) {

                        }
                        else
                            perr("list of old and new attributes differ in size");
                    }
                }
            }
        }
    }

    if(!op) {
        uti.usage(argv[0]);
    }
}

