/*
 * tish - a Unix-like shell (option A) for the TI-Nspire CX II CAS
 *
 * Builtins live inside tish (cd/ls/pwd/whoami/clear/help/exit). Anything else
 * is treated as a program name: we look for "<name>.tns" and nl_exec() it.
 * This lets you "install" real CALC programs (like you do with ntop or a
 * later vim port) and launch them by typing their name.
 */

#include <os.h>
#include <sys/stat.h>
#include <nspireio/nspireio.h>

#include "tish.h"
#include "fs.c"
#include "render.c"
#include "cmd.cpp"
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