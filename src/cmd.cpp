#include <os.h>
#include "tish.h"

/* ---------------- program launch ---------------- */

/* look for "<name>.tns" in the current dir, then the documents root. */
static int find_program(const char *name, char *path_buf, int buf_size) {
    const char *roots[2];
    int r;
    roots[0] = cwd;
    roots[1] = docs;
    for (r = 0; r < 2; r++) {
        char want[80];
        NUC_DIR *dp;
        struct nuc_dirent *ep;
        snprintf(want, sizeof(want), "%s.tns", name);
        dp = nuc_opendir(roots[r]);
        if (!dp)
            continue;
        while ((ep = nuc_readdir(dp))) {
            if (strcmp(ep->d_name, want) == 0) {
                snprintf(path_buf, buf_size, "%s/%s", roots[r], want);
                nuc_closedir(dp);
                return 1;
            }
        }
        nuc_closedir(dp);
    }
    return 0;
}

/* run the program, then fix the screen (prev[] is stale afterwards). */
static void launch(const char *path, const char *name) {
    char msg[COLS];
    snprintf(msg, sizeof(msg), "running %s", name);
    print_line(msg);
    nl_exec(path, 0, NULL);
    full_redraw();
}

static void cmd_pwd(int argc, char* argv[]) {
    argc++;argv++;// to bypass the annoying unused parameter warnings
    print_line(cwd);
}

static void cmd_cd(int argc, char* argv[]) {
    char target[300];
    NUC_DIR *probe;

    if (argc == 1) {
        snprintf(target, sizeof(target), "%s", docs);
    } else if (argc > 2){
        print_line("cd: too many arguments");
        return;
    } else {
        resolve_path(argv[1], target, sizeof(target));
    }

    probe = nuc_opendir(target);
    if (!probe) {
        print_line("cd: no such directory");
        return;
    }
    nuc_closedir(probe);
    normalize_path(cwd, sizeof(cwd), target);
}

static void cmd_ls(int argc, char* argv[]) {
    char dir[512];
    NUC_DIR *dp;
    struct nuc_dirent *ep;
    struct nuc_stat st;
    static const char* ls_help = R"(List directory contents.
Usage: ls [OPTION]... [FILE]...

Arguments:
  [paths]...

Options:
      --help              Print help information.
)";

    if (argc == 2) { // has arguments
        if (!strcmp(argv[1], (char*)"--help")){
            print_multiline((char*)ls_help);
            return;
        }
        resolve_path(argv[1], dir, sizeof(dir));
    } else if (argc == 1){
        snprintf(dir, sizeof(dir), "%s", cwd);
    } else { // too many arguments
        print_multiline((char*)ls_help);
        return;
    }

    dp = nuc_opendir(dir);
    if (!dp) {
        char msg[600];
        snprintf(msg, sizeof(msg), "ls: cannot open %s", dir);
        print_line(msg);
        return;
    }
    while ((ep = nuc_readdir(dp))) {
        char f[600];
        char line[COLS];
        const char *mark = "";
        if (strcmp(ep->d_name, ".") != 0 && strcmp(ep->d_name, "..") != 0) {
            snprintf(f, sizeof(f), "%s/%s", dir, ep->d_name);
            if (nuc_stat(f, &st) == 0 && S_ISDIR(st.st_mode))
                mark = "/";
        } else {
            mark = "/";        /* "." and ".." are always directories (nTxt: stat
                                * fails on ".." at the top-level directory) */
        }
        snprintf(line, sizeof(line), "%s%s", ep->d_name, mark);
        print_line(line);
    }
    nuc_closedir(dp);
}

static void cmd_hist(int argc, char* argv[]) {
    argc++;argv++;// to bypass the annoying unused parameter warnings
    char line[COLS + 16];   /* "%d: " prefix + longest entry */
    int i;
    for (i = 0; i < hist_len; i++) {
        snprintf(line, sizeof(line), "%d: %.*s", i, COLS - 1, hist[i]);
        print_line(line);
    }
    snprintf(line, COLS, "len=%d browse=%d", hist_len, browse);
    print_line(line);
}

static void cmd_dbg(int argc, char* argv[]) {
    argc++;argv++;
    char t[300], line[700];
    NUC_DIR *p;
    const char *paths[] = {"/", "/ndless", docs, "/documents", "/documents/ndless"};
    int i;

    snprintf(line, sizeof(line), "docs=%s cwd=%s", docs, cwd);
    print_line(line);

    for (i = 0; i < 5; i++) {
        p = nuc_opendir(paths[i]);
        snprintf(line, sizeof(line), "opendir(%s)=%s", paths[i], p ? "OK" : "FAIL");
        print_line(line);
        if (p) nuc_closedir(p);
    }

    const char *tests[] = {"ndless", "..", "../", "/ndless", "/documents/ndless", "/"};
    for (i = 0; i < 6; i++) {
        resolve_path(tests[i], t, sizeof(t));
        p = nuc_opendir(t);
        snprintf(line, sizeof(line), "resolve(%s)=%s list=%s",
                 tests[i], t, p ? "OK" : "FAIL");
        print_line(line);
        if (p) nuc_closedir(p);
    }
}

static void cmd_help(int argc, char* argv[]) {
    argc++;argv++;// to bypass the annoying unused parameter warnings
    print_line("builtins:");
    print_line("  cd <dir>   change directory");
    print_line("  pwd        print working directory");
    print_line("  ls <path>  list directory");
    print_line("  clear      clear screen");
    print_line("  whoami     shows the current user");
    print_line("  hist       show the last 16 commands");
    print_line("  help       this list");
    print_line("  exit       quit tish");
    print_line("./<name> runs a .tns program");
}

/* ---------------- command dispatch ---------------- */

void run_command(const char *line) {
    char buf[64];
    int argc = 0;
    char **argv = new char*[64];
    for (int i = 0; i < 64; i++){
        argv[i] = new char[64];
    }
    char* token;

    snprintf(buf, sizeof(buf), "%s", line);
    token = strtok(buf, " ");

    if (token == NULL) {
        for (int i = 0; i < 64; i++)
            delete[] argv[i];
        delete[] argv;
        return;
    }

    strcpy(argv[argc], token);
    argc++;

    while (argc < 64) {
        token = strtok(NULL, " ");

        if (token == NULL)
            break;

        strcpy(argv[argc], token);
        argc++;
    }

    if (strcmp(argv[0], "help") == 0) {

        cmd_help(argc, argv);

    } else if (strcmp(argv[0], "clear") == 0) {

        n_lines = 0;
        full_redraw();

    } else if (strcmp(argv[0], "exit") == 0) {

        /* tell main() to quit */
        quitting = 1;

    } else if (strcmp(argv[0], "pwd") == 0) {

        cmd_pwd(argc, argv);

    } else if (strcmp(argv[0], "cd") == 0) {

        cmd_cd(argc, argv);

    } else if (strcmp(argv[0], "hist") == 0) {

        cmd_hist(argc, argv);

    } else if (strcmp(argv[0], "ls") == 0) {

        cmd_ls(argc, argv);

    } else if (strcmp(argv[0], "whoami") == 0) {

        print_line("root");

    } else if (strcmp(argv[0], "dbg") == 0) {

        cmd_dbg(argc, argv);

    } else {
        char path[512];
        char msg[COLS];
        const char *name = argv[0];
        int want_launch = (name[0] == '.' && name[1] == '/');
        if (want_launch)
            name += 2;                     /* "./hello" -> "hello" */
        if (want_launch && find_program(name, path, sizeof(path)))
            launch(path, name);
        else {
            snprintf(msg, sizeof(msg), "%s: not found", argv[0]);
            print_line(msg);
        }
    }
    for (int i = 0; i < 64; i++){
        delete[] argv[i];
    }
    delete[] argv;
}
