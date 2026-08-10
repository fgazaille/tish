

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
void init_fs(void) {
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