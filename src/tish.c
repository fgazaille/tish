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
#include "input.c"
#include "cmd.c"

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