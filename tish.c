/*
 * tish - a Unix-like shell (option A) for the TI-Nspire CX II CAS
 *
 * Builtins live inside tish (cd/ls/pwd/whoami/clear/help/exit). Anything else
 * is treated as a program name: we look for "<name>.tns" and nl_exec() it.
 * This lets you "install" real CALC programs (like you do with ntop or a
 * later vim port) and launch them by typing their name.
 *
 * File API used (all provided by the Ndless SDK):
 *   - get_documents_dir() -> the documents root, where your .tns live
 *   - chdir()/getcwd() + opendir()/readdir()/closedir() + stat()
 *     -> navigate the REAL OS filesystem.  These understand forward-slash
 *        "/documents/..." paths (pattern proven by the nTxt editor's file
 *        browser, which uses exactly this stack and works).
 *   - nl_exec(path,0,NULL) -> run a PRG; blocks until it exits, then Ndless
 *     restores our screen (so we force a full redraw afterwards).
 *
 * There is NO reachable "/" on the Nspire: opendir("/") returns NULL, so the
 * OS's directory tree tops out at the documents area.  We therefore start at
 * whatever getcwd() hands back for a freshly-launched program and navigate
 * from there - the same way nTxt does.  cwd is tracked by calling chdir()
 * and re-reading getcwd(), never by hand-built path strings.
 */

#include <os.h>
#include <sys/stat.h>
#include <nspireio/nspireio.h>

#define ROWS 30
#define COLS 53
#define SCROLL 40            /* max scrollback lines kept */

static char scr[ROWS][COLS];
static char prev[ROWS][COLS];

static char scrollback[SCROLL][COLS];
static int  n_lines = 0;     /* total lines printed (can exceed SCROLL) */

static char cmdline[COLS];
static int  cmdlen = 0;
static int  cursor = 0;    /* caret position in cmdline, 0..cmdlen */

static int force_full_redraw = 1;
static int quitting = 0;

static char cwd[300];        /* current directory (kept in sync via chdir/getcwd) */
static char docs[300];       /* get_documents_dir() - where .tns live */

static char hist[16][COLS]; /*history size*/
static int hist_len = 0;
static int browse = -1;

/* copy a directory path into dst, dropping any trailing slash */
static void path_norm_to(char *dst, int dsz, const char *src) {
    int l;
    snprintf(dst, dsz, "%s", src);
    l = strlen(dst);
    while (l > 1 && dst[l - 1] == '/')
        dst[--l] = '\0';
}

/* Build the path to hand to chdir().  The user sees "/" as the "documents
 * root", but the OS's own absolute "/" is a different, un-listable namespace
 * (opendir("/") fails).  So an absolute path like "/ndless" is interpreted
 * relative to the documents root -> "/documents/ndless".  Returns a pointer
 * into `arg`, `docs`, or the local `buf` the caller passes in. */
static const char *build_target(const char *arg, char *buf, int buf_size) {
    size_t dl;
    if (!*arg)
        return docs;
    if (arg[0] == '/') {
        dl = strlen(docs);
        if (strncmp(arg, docs, dl) == 0 &&
            (arg[dl] == '\0' || arg[dl] == '/'))
            return arg;                    /* already a full documents path */
        snprintf(buf, buf_size, "%s%s", docs, arg);
        return buf;
    }
    return arg;                            /* relative - chdir() resolves it */
}

/* ---------------- filesystem init ---------------- */

/* Start in the OS's current directory for a launched program, but only if it
 * is a listable directory; otherwise use the documents root. */
static void init_fs(void) {
    const char *d = get_documents_dir();
    NUC_DIR *probe;
    path_norm_to(docs, sizeof(docs), d ? d : "/");
    if (getcwd(cwd, sizeof(cwd)) && (probe = nuc_opendir(cwd))) {
        nuc_closedir(probe);
        path_norm_to(cwd, sizeof(cwd), cwd);
    } else {
        snprintf(cwd, sizeof(cwd), "%s", docs);
    }
}

/* ---------------- scrollback output ---------------- */

static void print_line(const char *line) {
    int slot = n_lines % SCROLL;
    int i;
    for (i = 0; i < COLS - 1 && line[i]; i++)
        scrollback[slot][i] = line[i];
    scrollback[slot][i] = '\0';
    n_lines++;
}

/* ---------------- rendering (base plumbing) ---------------- */

static void set_row(int y, const char *s) {
    int x;
    for (x = 0; x < COLS && s[x]; x++)
        scr[y][x] = s[x];
    for (; x < COLS; x++)
        scr[y][x] = ' ';
}

static void render(void) {
    nio_console *csl = nio_get_default();
    int y, x;
    for (y = 0; y < ROWS; y++)
        for (x = 0; x < COLS; x++) {
            if (!force_full_redraw && scr[y][x] == prev[y][x])
                continue;
            nio_csl_savechar(csl, scr[y][x], x, y);
            nio_vram_csl_drawchar(csl, x, y);
            prev[y][x] = scr[y][x];
        }
    force_full_redraw = 0;
    nio_vram_draw();
}

/* after a launched program exits, our prev[] no longer matches the screen */
static void full_redraw(void) {
    force_full_redraw = 1;
}

/* ---------------- screen layout ---------------- */

/* rows 0..27 = scrollback window, row 28 = prompt, row 29 = status bar */
static void build_prompt(char *out, int out_size) {
    snprintf(out, out_size, "%.*s", out_size - 1, cwd);   /* show cwd, e.g. "/" */
}

static void build_screen(void) {
    char line[COLS];
    char prompt[COLS];
    int y, i, x;

    for (y = 0; y < ROWS - 2; y++) {
        int idx = n_lines - (ROWS - 2) + y;   /* last ROWS-2 lines */
        set_row(y, (idx >= 0 && idx < n_lines) ? scrollback[idx] : "");
    }

    build_prompt(prompt, sizeof(prompt));
    x = 0;
    line[x++] = '$';
    line[x++] = ' ';
    for (i = 0; prompt[i] && x < COLS - 3; i++)
        line[x++] = prompt[i];
    line[x++] = ' ';
    for (i = 0; i < cmdlen && x < COLS - 1; i++) {
        if (i == cursor)
            line[x++] = '|';                  /* cursor before the char */
        line[x++] = cmdline[i];
    }
    if (cursor >= cmdlen && x < COLS - 1)
        line[x++] = '|';                      /* cursor at end of line */
    line[x] = '\0';
    set_row(ROWS - 2, line);

    set_row(ROWS - 1, "tish  |  help = commands, exit = quit");
}

/* ---------------- input ---------------- */

typedef struct { t_key key; char ch; } KeyDef;

static const KeyDef keys[] = {
    {KEY_NSPIRE_A, 'a'}, {KEY_NSPIRE_B, 'b'}, {KEY_NSPIRE_C, 'c'},
    {KEY_NSPIRE_D, 'd'}, {KEY_NSPIRE_E, 'e'}, {KEY_NSPIRE_F, 'f'},
    {KEY_NSPIRE_G, 'g'}, {KEY_NSPIRE_H, 'h'}, {KEY_NSPIRE_I, 'i'},
    {KEY_NSPIRE_J, 'j'}, {KEY_NSPIRE_K, 'k'}, {KEY_NSPIRE_L, 'l'},
    {KEY_NSPIRE_M, 'm'}, {KEY_NSPIRE_N, 'n'}, {KEY_NSPIRE_O, 'o'},
    {KEY_NSPIRE_P, 'p'}, {KEY_NSPIRE_Q, 'q'}, {KEY_NSPIRE_R, 'r'},
    {KEY_NSPIRE_S, 's'}, {KEY_NSPIRE_T, 't'}, {KEY_NSPIRE_U, 'u'},
    {KEY_NSPIRE_V, 'v'}, {KEY_NSPIRE_W, 'w'}, {KEY_NSPIRE_X, 'x'},
    {KEY_NSPIRE_Y, 'y'}, {KEY_NSPIRE_Z, 'z'}, {KEY_NSPIRE_SPACE, ' '},
    {KEY_NSPIRE_0, '0'}, {KEY_NSPIRE_1, '1'}, {KEY_NSPIRE_2, '2'},
    {KEY_NSPIRE_3, '3'}, {KEY_NSPIRE_4, '4'}, {KEY_NSPIRE_5, '5'},
    {KEY_NSPIRE_6, '6'}, {KEY_NSPIRE_7, '7'}, {KEY_NSPIRE_8, '8'},
    {KEY_NSPIRE_9, '9'}, 
    {KEY_NSPIRE_PLUS, '\x10'}, {KEY_NSPIRE_MINUS, '\x11'}, {KEY_NSPIRE_LEFT, '\x12'}, {KEY_NSPIRE_RIGHT, '\x13'},
    {KEY_NSPIRE_eEXP, '\x12'}, {KEY_NSPIRE_TENX, '\x13'},
    {KEY_NSPIRE_DIVIDE, '/'}, {KEY_NSPIRE_PERIOD, '.'},
    {KEY_NSPIRE_DEL, '\b'},
    {KEY_NSPIRE_RET, '\n'}, {KEY_NSPIRE_ENTER, '\n'},
    {KEY_NSPIRE_ESC, '\x1b'},
};

/* returns the pressed key as a simple char, or 0 if nothing is pressed.
 * shift is read live and uppercases letters (same idea as TexEdit). */
static char read_key(void) {
    unsigned i;
    int shift = isKeyPressed(KEY_NSPIRE_SHIFT);
    for (i = 0; i < sizeof(keys) / sizeof(keys[0]); i++) {
        if (isKeyPressed(keys[i].key)) {
            char c = keys[i].ch;
            if (shift && c >= 'a' && c <= 'z')
                c -= 32;
            return c;
        }
    }
    return 0;
}

/* blocks until a key is pressed, returns it. */
static char wait_key(void) {
    while (1) {
        idle();
        char c = read_key();
        if (c)
            return c;
    }
}

/* ---------------- path helpers ---------------- */

/* Resolve a user path (absolute, relative, ".", "..") to an absolute path by
 * letting the OS do it for us: chdir() there, read getcwd(), chdir back.
 * Returns 1 if the path exists, 0 otherwise.  "out" is untouched on failure. */
static int path_to_abs(const char *in, char *out, int out_size) {
    char buf[512];
    const char *target = build_target(in, buf, sizeof(buf));
    if (chdir(target) == 0) {
        if (!getcwd(out, out_size))
            snprintf(out, out_size, "%s", cwd);
        chdir(cwd);                      /* put ourselves back in our dir */
        return 1;
    }
    return 0;
}

/* ---------------- builtins ---------------- */

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

/* ---------------- command dispatch ---------------- */

static void run_command(const char *line) {
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

/* ---------------- main loop ---------------- */

int main(void) {
    init_fs();
    while (1) {
        char c = wait_key();
        if (c == '\x1b') {
            break;
            /* ESC clears the line */
        } else if (c == '\b') {
            if (cursor > 0) {
                memmove(&cmdline[cursor-1], &cmdline[cursor], cmdlen - cursor);
                cursor--;
                cmdlen--;
            }
            browse = -1;
        } else if (c == '\x10'){ //up through history
            if (browse < hist_len-1){
                browse++;
                snprintf(cmdline, COLS, "%s", hist[browse]);
                cmdlen = strlen(cmdline);
                cursor = cmdlen;
            }
        } else if(c == '\x11'){ //down through history
            if (browse > 0){
                browse--;
                snprintf(cmdline, COLS, "%s", hist[browse]);
                cmdlen = strlen(cmdline);
                cursor = cmdlen;
            } else if (browse == 0){
                browse--;
                cmdline[0] = '\0';
                cmdlen = 0;
                cursor = 0;
            }
        } else if (c == '\x12') { //left through text
            if (cursor > 0){cursor--;}
        } else if (c == '\x13') { //right through text
            if (cursor < cmdlen){cursor++;}
        } else if (c == '\n') {
            cmdline[cmdlen] = '\0';
            if (cmdlen > 0){
                print_line(cmdline);
                memmove(&hist[1][0], &hist[0][0], 15 * COLS);
                snprintf(hist[0] , COLS, "%s", cmdline);
                if (hist_len < 16) {hist_len++;}
            }
            browse = -1;
            run_command(cmdline);
            if (quitting)
                break;
            cmdlen = 0;
            cursor = 0;
        } else if (c) {
            if (cmdlen < COLS - 3) {
                if (cursor < cmdlen)
                    memmove(&cmdline[cursor+1], &cmdline[cursor], cmdlen - cursor);
                cmdline[cursor] = c;
                cursor++;
                cmdlen++;
            }
            browse = -1;
        }

        build_screen();
        render();
        wait_no_key_pressed();                 /* debounce: one key = one step */
    }
    wait_key_pressed();
    return 0;
}