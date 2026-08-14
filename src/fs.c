

#include <os.h>
#include "tish.h"

/* copy a directory path into dst, dropping any trailing slash */
int normalize_path(char *dst, int dsz, const char *src) {
    int l;
    size_t src_len;

    if (!dst || !src || dsz < 2)
        return 0;
    src_len = strlen(src);
    if (src_len >= (size_t)dsz)
        return 0;
    memcpy(dst, src, src_len + 1);
    l = strlen(dst);
    while (l > 1 && dst[l - 1] == '/')
        dst[--l] = '\0';
    return 1;
}

/* Resolve a user-supplied path ("", ".", "..", relative, or absolute) to an
 * absolute path in the nuc filesystem namespace (where "/" is the real root,
 * listable on Firebird; the documents area lives at "/documents").
 *
 * The OS's chdir/getcwd are too unreliable to drive navigation (chdir returns
 * failure for valid targets, succeeds for invalid ones, and getcwd reports
 * the wrong directory), so we resolve paths ourselves and confirm them with
 * nuc_opendir() alone.  "." / ".." components are collapsed by hand, with
 * ".." never climbing above the real root. */
int resolve_path(const char *arg, char *out, int out_size) {
    char tmp[512];
    char *tok;
    int depth = 0, outl;
    int cl, al;

    if (!arg || !out || out_size < 2)
        return 0;

    if (arg[0] == '/') {
        al = strlen(arg);
        if (al >= (int)sizeof(tmp))
            return 0;
        memcpy(tmp, arg, al + 1);                  /* absolute: real root */
    } else {                                     /*if arg is not from root*/
        cl = strlen(cwd);
        al = strlen(arg);
        if (cl >= (int)sizeof(tmp) - 2 ||
            al > (int)sizeof(tmp) - cl - 2)
            return 0;
        memcpy(tmp, cwd, cl);
        tmp[cl] = '/';
        memcpy(tmp + cl + 1, arg, al);
        tmp[cl + 1 + al] = '\0';
    }

    out[0] = '\0';
    for (tok = strtok(tmp, "/"); tok; tok = strtok(NULL, "/")) {
        if (!strcmp(tok, ".") || !*tok)
            continue;                            /* ignore "." and doubles */
        if (!strcmp(tok, "..")) {
            if (depth > 0) {
                char *s = strrchr(out, '/');     /* pop last component */
                if (s == out){
                    out[1]= '\0';
                } else if (s) {
                    *s = '\0';
                }
                depth--;

            }
            continue;                            /* can't climb above "/" */
        }
        outl = strlen(out);
        if (outl + (int)strlen(tok) + 2 > out_size)
            return 0;
        snprintf(out + outl, out_size - outl, "/%s", tok);
        depth++;
    }
    if (!*out)
        snprintf(out, out_size, "/");
    return 1;
}

/* ---------------- filesystem init ---------------- */

/* Start in the documents root: the OS's getcwd() is unreliable, so we don't
 * trust it.  cwd is kept purely logical from here on (see resolve_path). */
void init_fs(void) {
    const char *d = get_documents_dir();
    if (!normalize_path(docs, sizeof(docs), d ? d : "/"))
        snprintf(docs, sizeof(docs), "/");
    snprintf(cwd, sizeof(cwd), "%s", docs);
}
