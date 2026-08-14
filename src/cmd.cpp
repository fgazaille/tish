#include <os.h>
#include <syscall.h>
#include "tish.h"

static int native_file_error(NUC_FILE *file) {
    if (!nl_hassyscall(ferror))
        return 0;
    return syscall<e_ferror, int>(file) != 0;
}

/* true if the first len chars of s are all spaces/tabs */
static int is_blank_seg(const char *s, int len) {
    int i;
    for (i = 0; i < len; i++)
        if (s[i] != ' ' && s[i] != '\t')
            return 0;
    return 1;
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
    int result;
    snprintf(msg, sizeof(msg), "running %s", name);
    print_line(msg);
    result = nl_exec(path, 0, NULL);
    if (result == 0xDEAD)
        print_line("launch failed");
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
        if (!resolve_path(argv[1], target, sizeof(target))) {
            print_line("cd: path too long");
            return;
        }
    }

    probe = nuc_opendir(target);
    if (!probe) {
        print_line("cd: no such directory");
        return;
    }
    nuc_closedir(probe);
    if (!normalize_path(cwd, sizeof(cwd), target))
        print_line("cd: path too long");
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
        if (!strcmp(argv[1], "--help")){
            print_multiline(ls_help);
            return;
        }
        if (!resolve_path(argv[1], dir, sizeof(dir))) {
            print_line("ls: path too long");
            return;
        }
    } else if (argc == 1){
        snprintf(dir, sizeof(dir), "%s", cwd);
    } else { // too many arguments
        print_multiline(ls_help);
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
        if (!resolve_path(tests[i], t, sizeof(t))) {
            snprintf(line, sizeof(line), "resolve(%s)=<too long> list=FAIL",
                     tests[i]);
            print_line(line);
            continue;
        }
        p = nuc_opendir(t);
        snprintf(line, sizeof(line), "resolve(%s)=%s list=%s",
                 tests[i], t, p ? "OK" : "FAIL");
        print_line(line);
        if (p) nuc_closedir(p);
    }
}

/* ---------------- file manipulation ---------------- */

/* print args joined by spaces: echo hello world */
static void cmd_echo(int argc, char* argv[]) {
    char line[COLS * 4];
    int i, l = 0;
    line[0] = '\0';
    for (i = 1; i < argc; i++) {
        int al = strlen(argv[i]);
        if (l + al + 2 > (int)sizeof(line))
            break;
        if (i > 1)
            line[l++] = ' ';
        memcpy(line + l, argv[i], al);
        l += al;
    }
    line[l] = '\0';
    print_line(line);
}

/* dump the piped input, one line at a time */
static void cat_pipe(void) {
    char line[COLS];
    int i, l = 0;
    for (i = 0; i < pipe_in_len; i++) {
        if (pipe_in[i] == '\n') {
            line[l] = '\0';
            print_line(line);
            l = 0;
            continue;
        }
        if (l == COLS - 1) {                 /* wrap over-long lines */
            line[l] = '\0';
            print_line(line);
            l = 0;
        }
        line[l++] = pipe_in[i];
    }
    if (l) {
        line[l] = '\0';
        print_line(line);
    }
}

/* read a file through the nuc API and print it line by line */
static void cat_file(const char *arg) {
    char target[300];
    char chunk[257];
    char line[COLS];
    NUC_FILE *f;
    int l = 0;
    size_t n, i;

    if (!resolve_path(arg, target, sizeof(target))) {
        print_line("cat: path too long");
        return;
    }
    f = nuc_fopen(target, "r");
    if (!f) {
        char msg[COLS];
        snprintf(msg, sizeof(msg), "cat: cannot open %s", arg);
        print_line(msg);
        return;
    }
    while ((n = nuc_fread(chunk, 1, sizeof(chunk) - 1, f)) > 0) {
        for (i = 0; i < n; i++) {
            char c = chunk[i];
            if (c == '\r')
                continue;                    /* tolerate CRLF files */
            if (c == '\n') {
                line[l] = '\0';
                print_line(line);
                l = 0;
                continue;
            }
            if (l == COLS - 1) {             /* wrap over-long lines */
                line[l] = '\0';
                print_line(line);
                l = 0;
            }
            line[l++] = c;
        }
    }
    if (l) {
        line[l] = '\0';
        print_line(line);
    }
    if (native_file_error(f) || nuc_fclose(f) != 0)
        io_error = 1;
}

static void cmd_cat(int argc, char* argv[]) {
    int i;
    if (argc == 1) {                     /* no args: read the pipe */
        if (pipe_ready)
            cat_pipe();
        else
            print_line("cat: no input");
        return;
    }
    for (i = 1; i < argc; i++)
        cat_file(argv[i]);
}

/* create an empty file (or leave an existing one alone) */
static void cmd_touch(int argc, char* argv[]) {
    char target[300];
    NUC_FILE *f;
    int i;

    if (argc < 2) {
        print_line("touch: missing file name");
        return;
    }
    for (i = 1; i < argc; i++) {
        if (!resolve_path(argv[i], target, sizeof(target))) {
            print_line("touch: path too long");
            continue;
        }
        f = nuc_fopen(target, "a");
        if (!f) {
            char msg[COLS];
            snprintf(msg, sizeof(msg), "touch: cannot create %s", argv[i]);
            print_line(msg);
            continue;
        }
        if (nuc_fclose(f) != 0)
            io_error = 1;
    }
}

static void cmd_cp(int argc, char* argv[]) {
    char src[300], dst[300], chunk[257];
    NUC_FILE *in, *out;
    size_t n;

    if (argc != 3) {
        print_line("usage: cp <src> <dst>");
        return;
    }
    if (!resolve_path(argv[1], src, sizeof(src)) ||
        !resolve_path(argv[2], dst, sizeof(dst))) {
        print_line("cp: path too long");
        return;
    }
    if (!strcmp(src, dst)) {
        print_line("cp: source and destination are the same file");
        return;
    }

    in = nuc_fopen(src, "r");
    if (!in) {
        print_line("cp: cannot read source");
        return;
    }
    out = nuc_fopen(dst, "w");
    if (!out) {
        if (nuc_fclose(in) != 0)
            io_error = 1;
        print_line("cp: cannot write destination");
        return;
    }
    while ((n = nuc_fread(chunk, 1, sizeof(chunk), in)) > 0) {
        if (nuc_fwrite(chunk, 1, n, out) != n) {
            io_error = 1;
            break;
        }
    }
    if (native_file_error(in) || nuc_fclose(in) != 0)
        io_error = 1;
    if (nuc_fclose(out) != 0)
        io_error = 1;
}

/* mkdir/rm/rmdir/mv use the legacy OS syscalls - the same family as the
 * unreliable chdir(), so they are the most likely to misbehave. */
static void cmd_mkdir(int argc, char* argv[]) {
    char target[300];
    if (argc != 2) {
        print_line("usage: mkdir <dir>");
        return;
    }
    if (!resolve_path(argv[1], target, sizeof(target))) {
        print_line("mkdir: path too long");
        return;
    }
    if (mkdir(target, 0777) != 0)
        print_line("mkdir: failed");
}

static void cmd_rm(int argc, char* argv[]) {
    char target[300];
    int i;
    if (argc < 2) {
        print_line("usage: rm <file>");
        return;
    }
    for (i = 1; i < argc; i++) {
        if (!resolve_path(argv[i], target, sizeof(target))) {
            print_line("rm: path too long");
            continue;
        }
        if (unlink(target) != 0) {
            char msg[COLS];
            snprintf(msg, sizeof(msg), "rm: cannot remove %s", argv[i]);
            print_line(msg);
        }
    }
}

static void cmd_rmdir(int argc, char* argv[]) {
    char target[300];
    if (argc != 2) {
        print_line("usage: rmdir <dir>");
        return;
    }
    if (!resolve_path(argv[1], target, sizeof(target))) {
        print_line("rmdir: path too long");
        return;
    }
    if (rmdir(target) != 0)
        print_line("rmdir: failed");
}

static void cmd_mv(int argc, char* argv[]) {
    char src[300], dst[300];
    if (argc != 3) {
        print_line("usage: mv <src> <dst>");
        return;
    }
    if (!resolve_path(argv[1], src, sizeof(src)) ||
        !resolve_path(argv[2], dst, sizeof(dst))) {
        print_line("mv: path too long");
        return;
    }
    if (rename(src, dst) != 0)
        print_line("mv: failed");
}

static void cmd_help(int argc, char* argv[]) {
    argc++;argv++;// to bypass the annoying unused parameter warnings
    print_line("builtins:");
    print_line("  cd <dir>   change directory");
    print_line("  pwd        print working directory");
    print_line("  ls <path>  list directory");
    print_line("  cat <file> print a file (no args: read a pipe)");
    print_line("  echo <...> print the arguments");
    print_line("  touch <f>  create an empty file");
    print_line("  cp <a> <b> copy a file");
    print_line("  mv <a> <b> rename/move");
    print_line("  rm <file>  delete a file");
    print_line("  mkdir <d>  create a directory");
    print_line("  rmdir <d>  delete an empty directory");
    print_line("  clear      clear screen");
    print_line("  whoami     shows the current user");
    print_line("  hist       show the last 16 commands");
    print_line("  help       this list");
    print_line("  exit       quit tish");
    print_line("cmd > file writes output to a file");
    print_line("cmd | cmd  pipes builtin output (e.g. ls | cat)");
    print_line("./<name> runs a .tns program");
}

/* ---------------- command dispatch ---------------- */

static void dispatch(int argc, char* argv[]) {
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

    } else if (strcmp(argv[0], "cat") == 0) {

        cmd_cat(argc, argv);

    } else if (strcmp(argv[0], "echo") == 0) {

        cmd_echo(argc, argv);

    } else if (strcmp(argv[0], "touch") == 0) {

        cmd_touch(argc, argv);

    } else if (strcmp(argv[0], "cp") == 0) {

        cmd_cp(argc, argv);

    } else if (strcmp(argv[0], "mv") == 0) {

        cmd_mv(argc, argv);

    } else if (strcmp(argv[0], "rm") == 0) {

        cmd_rm(argc, argv);

    } else if (strcmp(argv[0], "mkdir") == 0) {

        cmd_mkdir(argc, argv);

    } else if (strcmp(argv[0], "rmdir") == 0) {

        cmd_rmdir(argc, argv);

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
}

/* Run one pipeline stage: tokenize, peel off "> file" / ">> file", dispatch.
 * Output goes wherever the caller pointed the sink. */
static void run_segment(const char *seg) {
    char buf[64];
    int argc = 0, i;
    char **argv = new char*[64];
    for (i = 0; i < 64; i++){
        argv[i] = new char[64];
    }
    char* token;
    const char *redir = 0;
    const char *mode = "w";
    int saw_redir = 0;
    int redir_i = -1;

    snprintf(buf, sizeof(buf), "%s", seg);
    token = strtok(buf, " ");

    while (token && argc < 64) {
        strcpy(argv[argc], token);
        argc++;
        token = strtok(NULL, " ");
    }

    /* find a redirection and cut the command's arguments there */
    for (i = 0; i < argc; i++) {
        if (strcmp(argv[i], ">") == 0 || strcmp(argv[i], ">>") == 0) {
            if (strcmp(argv[i], ">>") == 0)
                mode = "a";
            saw_redir = 1;
            redir = (i + 1 < argc) ? argv[i + 1] : 0;
            redir_i = i;
            break;
        }
    }
    if (redir_i + 2 < argc){
        print_line("syntax error: extra redirect argument");
    }
    if (saw_redir && !redir) {
        print_line("syntax error: no file after >");
    } else if (argc > 0) {
        if (redir) {
            char target[300];
            int prev_sink = sink;
            NUC_FILE *prev_file = out_file;
            int path_ok = resolve_path(redir, target, sizeof(target));
            if (!path_ok) {
                print_line("cannot open output: path too long");
            } else {
                out_file = nuc_fopen(target, mode);
                if (!out_file) {
                    print_line("cannot open output file");
                } else {
                    sink = SINK_FILE;
                    dispatch(redir ? redir_i : argc, argv);
                    if (nuc_fclose(out_file) != 0)
                        io_error = 1;
                    out_file = prev_file;
                    sink = prev_sink;
                }
            }
        } else {
            dispatch(redir ? redir_i : argc, argv);
        }
    }

    for (i = 0; i < 64; i++){
        delete[] argv[i];
    }
    delete[] argv;
}

/* Split the line on '|' and run each stage, handing each stage's output to
 * the next one as input.  Pipes only work between builtins: launched .tns
 * programs own the screen and have no stdin/stdout we can capture. */
void run_command(const char *line) {
    char seg[COLS];
    const char *p = line;

    // make sure there are no empty segments
    const char *q = line;
    while (1) {
        const char *bar = strchr(q, '|');
        int len = bar ? (int)(bar - q) : (int)strlen(q);
        if (len == 0 || is_blank_seg(q, len)) {
            print_line("syntax error: empty segment");
            return;
        }
        if (!bar) break;
        q = bar + 1;
    }

    pipe_in_len = 0;
    pipe_out_len = 0;
    pipe_ready = 0;
    io_error = 0;

    while (1) {
        const char *bar = strchr(p, '|');
        int len = bar ? (int)(bar - p) : (int)strlen(p);
        if (len > (int)sizeof(seg) - 1)
            len = sizeof(seg) - 1;
        memcpy(seg, p, len);
        seg[len] = '\0';

        if (!bar) {
            run_segment(seg);
            break;
        }

        {   /* not the last stage: collect this stage's output */
            int prev_sink = sink;
            sink = SINK_PIPE;
            pipe_out_len = 0;
            run_segment(seg);
            sink = prev_sink;

            memcpy(pipe_in, pipe_out, pipe_out_len);
            pipe_in_len = pipe_out_len;
            pipe_out_len = 0;
            pipe_ready = 1;
        }
        p = bar + 1;
    }

    pipe_ready = 0;
    pipe_in_len = 0;
    if (io_error) {
        sink = SINK_SCREEN;
        out_file = 0;
        print_line("tish: file I/O error");
        io_error = 0;
    }
}
