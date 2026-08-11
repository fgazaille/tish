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
    argc++;// to bypass the annoying unused parameter warnings
    char buf[512];
    char tmp[300];
    NUC_DIR *probe;
    const char *target = build_target(argv[1], buf, sizeof(buf));

    if (chdir(target) != 0) {
        print_line("cd: no such directory");
        return;
    }

    /* The OS can normalize a partial path to "/" (its un-listable root).
     * Only accept a result we can actually list; otherwise undo and fail. */
    if (!getcwd(tmp, sizeof(tmp)) || !(probe = nuc_opendir(tmp))) {
        chdir(cwd);                      /* put ourselves back */
        print_line("cd: no such directory");
        return;
    }
    nuc_closedir(probe);
    path_norm_to(cwd, sizeof(cwd), tmp);
}

static void cmd_ls(int argc, char* argv[]) {
    char dir[512];
    NUC_DIR *dp;
    struct nuc_dirent *ep;
    struct nuc_stat st;
    static const char* ls_help = R"(List directory contents.
Ignore files and directories starting with a '.' by default

Usage: ls [OPTION]... [FILE]...

Arguments:
  [paths]...

Options:
      --help                                     Print help information.
)";

    if (argc > 1) {
        if (!path_to_abs(argv[1], dir, sizeof(dir))) {
        }
    } else if (argc == 1){
        snprintf(dir, sizeof(dir), "%s", cwd);
    } else { // too many arguments
        print_line(ls_help);
    }

    dp = nuc_opendir(dir);
    if (!dp) {
        print_line("ls: cannot open");
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

static void cmd_help(int argc, char* argv[]) {
    argc++;argv++;// to bypass the annoying unused parameter warnings
    print_line("builtins:");
    print_line("  cd <dir>   change directory");
    print_line("  pwd        print working directory");
    print_line("  ls <path>  list directory");
    print_line("  clear      clear screen");
    print_line("  help       this list");
    print_line("  exit       quit tish");
    print_line("anything else tries to run <name>.tns");
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

    if (token == NULL)
        return;

    strcpy(argv[argc], token);
    argc++;

    while (argc < 64) {
        token = strtok(NULL, " ");

        if (token == NULL)
            break;

        strcpy(argv[argc], token);
        argc++;
    }

    if (!argc)

        return;                                /* empty line */

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

    } else {
        char path[512];
        if (find_program(argv[0], path, sizeof(path)))
            launch(path, argv[0]);
        else{
            print_line(argv[0]);
            print_line("<name>: not found");
        }
    }
}
