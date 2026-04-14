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
#include <termios.h>
#include <iostream>
#include <fstream>
#include <cstdlib>


void Utils::usage(const char *appnam) {
    (void) appnam;
    std::string line;
    std::string path = DATADIR + std::string("/usage.txt");
    std::ifstream usagef(path);

    if(!usagef.is_open()) {
        const char *install_root = std::getenv("CUMBIA_INSTALL_ROOT");
        if(install_root != nullptr && install_root[0] != '\0') {
            std::string root_path(install_root);
            if(!root_path.empty() && root_path.back() == '/')
                root_path.pop_back();
            path = root_path + "/share/snapctxmanager-cli/usage.txt";
            usagef.clear();
            usagef.open(path);
        }
    }

    if(usagef.is_open()) {
        while(std::getline(usagef, line)) {
            std::cout << line << '\n';
        }
        usagef.close();
    } else {
        printf("failed to load %s in read mode\n", path.c_str());
    }
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

int Utils::configure()
{
    int r = 0;
    std::string confd(CONF_DIR);
    const std::string conf_f (CONF_FILE);
    if(confd.find("$HOME") != std::string::npos) {
        struct passwd *pw = getpwuid(getuid());
        confd.replace(0, strlen("$HOME"), pw->pw_dir);
    }

    struct stat st;
    memset(&st, 0, sizeof(st));
    if (stat(confd.c_str(), &st) == -1) {
        r = mkdir(confd.c_str(), 0700);
    }
    if(r == 0) {
        bool ok = false;
        char in[1024];

        std::map<std::string, std::string> par;
        std::string str;
        printf("database host: ");
        getline(std::cin, str);
        ok = str.length() > 0;
        if(ok){
            par["dbhost"] = str;
            printf("database user: ");
            getline(std::cin, str);
            ok = str.length() > 0;
        }
        if(ok) {
            par["dbuser"] = str;
            printf("database password (echo disabled): ");
            struct termios tty;
            tcgetattr(STDIN_FILENO, &tty);
            tty.c_lflag &= ~ECHO;
            tcsetattr(STDIN_FILENO, TCSANOW, &tty);
            getline(std::cin, str);
            tty.c_lflag |= ECHO;
            tcsetattr(STDIN_FILENO, TCSANOW, &tty);
            if(str.length() == 0)
                printf("warning: empty password\n");
            par["dbpass"] = str;
            printf("\ndatabase name (default if blank: \"snap\"): ");
            getline(std::cin, str);
            str.length() > 0 ? par["dbname"] = str : par["dbname"] = "snap";
        }
        if(ok) {
            std::string f = confd + "/" + conf_f;
            std::ofstream of;
            of.open(f.c_str());
            if(of) {
                for(std::map<std::string, std::string>::const_iterator it = par.begin(); it != par.end() ; ++it)
                    of << it->first << " = " << it->second << std::endl;
                of.close();
                return 0;
            }
            else
                return 1;  // 1: error writing file
        }
        else
            return -1; // configuration not complete
    }
    return 1; // 1: error creating dir
}

void Utils::out_snapshots(const Context &c, const std::vector<Snapshot> &v) const {
    if(v.empty()) {
        printf("\033[1;35m!\033[0m no snapshots for context \033[1;32;4m%s\033[0m (id %d)\n",
               c.name.c_str(), c.id);
        return;
    }
    printf("Snapshots for context \033[1;32;4m%s\033[0m (id \033[1;34m%d\033[0m):\n\n",
           c.name.c_str(), c.id);
    // compute column widths
    size_t cmtW = strlen("comment");
    size_t timW = strlen("time");
    for(const Snapshot &s : v) {
        if(s.comment.length() > cmtW) cmtW = s.comment.length();
        if(s.time.length()    > timW) timW = s.time.length();
    }
    // header
    printf(" %-6s | %-*s | %-*s\n", "snap_id",
           (int)timW, "time", (int)cmtW, "comment");
    printf(" %s-+-%s-+-%s\n",
           std::string(6,'-').c_str(),
           std::string(timW,'-').c_str(),
           std::string(cmtW,'-').c_str());
    int i = 0;
    for(const Snapshot &s : v) {
        printf(" \033[1;34m%-6d\033[0m | \033[0;36m%-*s\033[0m | %s\n",
               s.id_snap, (int)timW, s.time.c_str(), s.comment.c_str());
        i++;
    }
    printf("\n\033[1;32m%d\033[0m snapshot(s) total\n", i);
}

void Utils::out_att_snaps(const std::vector<AttSnapRecord> &v) const {
    if(v.empty()) {
        printf("\033[1;35m!\033[0m no snapshot records found for the given attribute(s)\n");
        return;
    }
    // compute column widths
    size_t attW = strlen("attribute");
    size_t ctxW = strlen("context");
    size_t timW = strlen("time");
    size_t cmtW = strlen("comment");
    size_t valW = strlen("value");
    for(const AttSnapRecord &r : v) {
        if(r.full_name.length()    > attW) attW = r.full_name.length();
        if(r.ctx_name.length()     > ctxW) ctxW = r.ctx_name.length();
        if(r.snap_time.length()    > timW) timW = r.snap_time.length();
        if(r.snap_comment.length() > cmtW) cmtW = r.snap_comment.length();
        size_t vl = r.value.length() + (r.setpoint.empty() ? 0 : r.setpoint.length() + 3); // " / setpoint"
        if(vl > valW) valW = vl;
    }
    if(valW > 60) valW = 60; // cap

    // header
    printf(" %-*s | %-*s | %-6s | %-*s | %-*s | %-*s\n",
           (int)attW, "attribute",
           (int)ctxW, "context",
           "snap_id",
           (int)timW, "time",
           (int)cmtW, "comment",
           (int)valW, "value");
    auto sep = [](size_t w){ return std::string(w, '-'); };
    printf(" %s-+-%s-+-%-6s-+-%s-+-%s-+-%s\n",
           sep(attW).c_str(), sep(ctxW).c_str(), "------",
           sep(timW).c_str(), sep(cmtW).c_str(), sep(valW).c_str());

    for(const AttSnapRecord &r : v) {
        std::string vdisp = r.value;
        if(!r.setpoint.empty())
            vdisp += " / " + r.setpoint;
        if(vdisp.length() > valW)
            vdisp = vdisp.substr(0, valW - 3) + "...";
        printf(" \033[1;32m%-*s\033[0m | \033[0;33m%-*s\033[0m | \033[1;34m%-6d\033[0m"
               " | \033[0;36m%-*s\033[0m | %-*s | \033[0;35m%-*s\033[0m\n",
               (int)attW, r.full_name.c_str(),
               (int)ctxW, r.ctx_name.c_str(),
               r.snap_id,
               (int)timW, r.snap_time.c_str(),
               (int)cmtW, r.snap_comment.c_str(),
               (int)valW, vdisp.c_str());
    }
    printf("\n\033[1;32m%ld\033[0m record(s) total\n", v.size());
}

void Utils::out_ctxs(const std::vector<Context> &v) const {
    int i = 0;
    for(const Context& c : v)
        printf("%d. Name: \033[1;32;4m%s\033[0m\n   Author: %s\n   Reason: %s\n   Description: %s\n   ID: %d\n", ++i, c.name.c_str(), c.author.c_str(), c.reason.c_str(), c.description.c_str(), c.id);
}

void Utils::out_ctx(const Context& c, const std::vector<Ast> &v) const {
    int i = 0;
    bool has_facility = false;
    const int maxsrclen = 35;
    out_ctxs(std::vector<Context> { c } );
    TgUtils u;
    size_t srcM = 0, dtM = 0, facM = strlen(" facility");
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
    for(size_t s = 0; s < dtM - strlen("type"); s++)
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
        printf("\033[1;32m%s\033[0m", src.c_str());
        for(size_t s = 0; s < srcM - src.length(); s++)
            printf(" ");
        // data type
        printf("| %s", u.data_type(a.data_type).c_str());
        for(size_t s = 0; s < dtM - u.data_type(a.data_type).length(); s++)
            printf(" ");

        //        dt     color +df
        printf("| %s%s\033[0m ",
               df == "SCA" ? "\033[0;32m" : ( df == "SPE" ? "\033[0;36m" : "\033[0;35m"), df.c_str()); // data format with color
        printf("| %s", u.writable(a.writable).c_str());
        for(size_t s = 0; s < strlen("RWW") - u.writable(a.writable).length(); s++)
            printf(" ");
        std::string X = std::to_string(a.max_dim_x), Y = std::to_string(a.max_dim_y);
        printf("| %s", X.c_str());
        for(size_t s = 0; s < strlen("max dim x") - X.length(); s++)
            printf(" ");
        printf("| %s", Y.c_str());
        for(size_t s = 0; s < strlen("max dim y") - Y.length(); s++)
            printf(" ");

        // facility
        if(has_facility) { // at least one
            size_t flen = a.facility != "HOST:port" ? a.facility.length() : 0;
            printf("| ");
            if(a.facility != "HOST:port")
                printf("\033[0;33m%s\033[0m", a.facility.c_str());
            for(size_t s = 0; s < facM - flen; s++)
                printf(" ");
        }

        printf("| \033[0;34m%d\033[0m", a.id);

        printf("\n");
    }
}
