#include "utils.h"
#include <string>
#include <snapctxmanager.h>
#include <tgutils.h>
#include <string.h> // strlen

#include <sys/stat.h>
#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>

#include <iostream>
#include <fstream>

void Utils::usage(const char *appnam) {
    printf("\e[1;31mUsage\e[0m \"%s -n ctxname -a author -r \"a reason\" -c dbconf -f ctxfile [-d \"a description\"] \n"
           "At least one of \e[1;37;4mreason\e[0m or \e[1;37;4mdescription\e[0m must be specified\n\n", appnam);
}

std::string Utils::conf_dir() const {
    std::string confd(CONF_DIR);
    if(confd.find("$HOME") != std::string::npos) {
        struct passwd *pw = getpwuid(getuid());
        confd.replace(0, strlen("$HOME"), pw->pw_dir);
    }
    return confd;
}

std::string Utils::conf_file_path() const {
    return conf_dir() + "/" + std::string(CONF_FILE);
}

bool Utils::conf_file_exists() const {
    const std::string conf_f (CONF_FILE);
    const std::string& fp = conf_dir() + "/" + conf_f;
    return access(fp.c_str(), F_OK) == 0;
}

bool Utils::configure()
{
    int r = 0;
    std::string confd(CONF_DIR);
    const std::string conf_f (CONF_FILE);
    if(confd.find("$HOME") != std::string::npos) {
        struct passwd *pw = getpwuid(getuid());
        confd.replace(0, strlen("$HOME"), pw->pw_dir);
    }

    struct stat st = {0};
    if (stat(confd.c_str(), &st) == -1) {
        r = mkdir(confd.c_str(), 0700);
    }
    if(r == 0) {
        char in[1024];
        std::string f = confd + "/" + conf_f;
        std::ofstream of;
        of.open(f.c_str());
        if(of) {
            std::string str;
            printf("database host: ");
            of << "dbhost = ";
            getline(std::cin, str);
            of << str << std::endl;


            printf("database user: ");
            of << "dbuser = ";
            getline(std::cin, str);
            of << str << std::endl;


            printf("database password: ");
            of << "dbpass = ";
            getline(std::cin, str);
            of << str << std::endl;


            printf("database name: ");
            of << "dbname = ";
            getline(std::cin, str);
            if(str.length() > 0)
                of << str << std::endl;
            else
                of << "snap" << std::endl;

            of.close();
            return true;
        }
    }
    return false;
}

void Utils::out_ctxs(const std::vector<Context> &v) const {
    int i = 0;
    for(const Context& c : v)
        printf("%d. Name: \e[1;32;4m%s\e[0m\n   Author: %s\n   Reason: %s\n   Description: %s\n   ID: %d\n", ++i, c.name.c_str(), c.author.c_str(), c.reason.c_str(), c.description.c_str(), c.id);
}

void Utils::out_ctx(const Context& c, const std::vector<Ast> &v) const {
    int i = 0;
    bool has_facility = false;
    const int maxsrclen = 35;
    out_ctxs(std::vector<Context> { c } );
    TgUtils u;
    size_t srcM = 0, dtM = 0, facM = strlen("facility");
    for(const Ast& a : v) {
        if(a.full_name.length() > srcM)
            srcM = a.full_name.length();
        const size_t dtl = u.data_type(a.data_type).length();
        if(dtl > dtM)
            dtM = dtl;
        if((a.facility != "HOST:port") && facM < a.facility.length()) {
            facM = a.facility.length();
            has_facility |= true;
        }
    }

    // header
    printf("-\n");
    printf("source"); // prints 9 chars
    for(size_t s = 0; s < srcM - strlen("source"); s++)
        printf(" ");
    printf("| type");
    for(size_t s = 0; s < strlen(" type"); s++)
        printf(" ");
    printf("| fmt | wri| max dim x| max dim y");
    if(has_facility)
        printf("| facility ");
    for(size_t s = 0; s < facM - strlen(" facility"); s++)
        printf(" ");
    printf("| ID");
    printf("\n");

    for(const Ast& a : v) {
        const std::string &df = u.data_format(a.data_format);
        std::string src (a.full_name);

        // list number / src name
        printf("\e[1;32m%s\e[0m", src.c_str());
        for(size_t s = 0; s < srcM - src.length(); s++)
            printf(" ");
        // data type
        printf("| %s", u.data_type(a.data_type).c_str());
        for(size_t s = 0; s < dtM - u.data_type(a.data_type).length(); s++)
            printf(" ");

        //        dt     color +df
        printf("| %s%s\e[0m ",
               df == "SCA" ? "\e[0;32m" : ( df == "SPE" ? "\e[0;36m" : "\e[0;35m"), df.c_str()); // data format with color
        printf("| %s", u.writable(a.writable).c_str());
        for(size_t s = 0; s < strlen("RWW") - u.writable(a.writable).length(); s++)
            printf(" ");
        std::string X = std::to_string(a.max_dim_x), Y = std::to_string(a.max_dim_y);
        printf("| %s", X.c_str());
        for(size_t s = 0; s < strlen("max dim x") - X.length(); s++)
            printf(" ");
        printf("| %s", Y.c_str());
        for(size_t s = 0; s < strlen("max dim y") - X.length(); s++)
            printf(" ");

        // facility
        size_t flen = a.facility != "HOST:port" ? a.facility.length() : 0;
        printf("| ");
        if(a.facility != "HOST:port")
            printf("\e[0;33m%s\e[0m", a.facility.c_str());
        for(size_t s = 0; s < facM - flen; s++)
            printf(" ");

        printf("| \e[0;34m%d\e[0m", a.id);

        printf("\n");
    }
}
