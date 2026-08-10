
/* ---------------- builtins ---------------- */
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

static void cmd_pwd(void) {
    print_line(cwd);
}

static void cmd_cd(const char *arg) {
    char buf[512];
    char tmp[300];
    NUC_DIR *probe;
    const char *target = build_target(arg, buf, sizeof(buf));

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

static void cmd_ls(const char *arg) {
    char dir[512];
    NUC_DIR *dp;
    struct nuc_dirent *ep;
    struct nuc_stat st;

    if (*arg) {
        if (!path_to_abs(arg, dir, sizeof(dir))) {
            print_line("ls: cannot open");
            return;
        }
    } else {
        snprintf(dir, sizeof(dir), "%s", cwd);
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

static void cmd_hist(void) {
        char line[COLS + 16];   /* "%d: " prefix + longest entry */
        int i;
        for (i = 0; i < hist_len; i++) {
            snprintf(line, sizeof(line), "%d: %.*s", i, COLS - 1, hist[i]);
            print_line(line);
        }
        snprintf(line, COLS, "len=%d browse=%d", hist_len, browse);
        print_line(line);
}

static void cmd_help(void) {
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
    char *cmd;
    char *arg = "";

    snprintf(buf, sizeof(buf), "%s", line);
    cmd = strtok(buf, " ");                    /* first token = command */
    arg = strtok(NULL, "");                    /* the rest = arguments, may be NULL */
    if (arg == NULL)
        arg = "";
    while (*arg == ' ')
        arg++;

    if (!cmd || !*cmd)
        return;                                /* empty line */

    if (strcmp(cmd, "help") == 0) {
        cmd_help();
    } else if (strcmp(cmd, "clear") == 0) {
        n_lines = 0;
        full_redraw();
    } else if (strcmp(cmd, "exit") == 0) {
        /* tell main() to quit */
        quitting = 1;
    } else if (strcmp(cmd, "pwd") == 0) {
        cmd_pwd();
    } else if (strcmp(cmd, "cd") == 0) {
        cmd_cd(arg);
    } else if (strcmp(cmd, "hist") == 0) {
        cmd_hist();
    } else if (strcmp(cmd, "ls") == 0) {
        cmd_ls(arg);
    } else if (strcmp(cmd, "whoami") == 0) {
        print_line("root");
    } else {
        char path[512];
        if (find_program(cmd, path, sizeof(path)))
            launch(path, cmd);
        else
            print_line("<name>: not found");
    }
}
