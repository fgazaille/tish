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

#include "tish.h"

#include "fs.c"
#include "render.c"
#include "cmd.c"
#include "input.c"

/* ---------------- main loop ---------------- */

int main(void) {
    init_fs();
    while (1) {
        build_screen();
        render();
        wait_no_key_pressed();                 /* debounce: one key = one step */
        if (handleinput()) break;

    }
    wait_key_pressed();
    return 0;
}