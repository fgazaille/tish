
#include <os.h>
#include <nspireio/nspireio.h>
#include "tish.h"

/* ---------------- scrollback output ---------------- */

/* All shell output goes through here.  Depending on the active sink it lands
 * in the scrollback window, in a file ("> file") or in the pipe buffer
 * ("cmd | cmd").  Only builtins are affected: launched .tns programs draw on
 * the screen themselves. */
void print_line(const char *line) {
    int slot, i;

    if (sink == SINK_FILE && out_file) {
        size_t len = strlen(line);
        if (nuc_fwrite((void *)line, 1, len, out_file) != len ||
            nuc_fwrite((void *)"\n", 1, 1, out_file) != 1)
            io_error = 1;
        return;
    }

    if (sink == SINK_PIPE) {
        int l = strlen(line);
        if (pipe_out_len + l + 1 < PIPE_CAP) {
            memcpy(pipe_out + pipe_out_len, line, l);
            pipe_out_len += l;
            pipe_out[pipe_out_len++] = '\n';
        } else {
            pipe_overflow = 1;     /* drop the line, report after the pipeline */
        }
        return;
    }

    slot = n_lines % SCROLL;
    for (i = 0; i < COLS - 1 && line[i]; i++)
        scrollback[slot][i] = line[i];
    scrollback[slot][i] = '\0';
    n_lines++;
}

void print_multiline(const char *text) {
    char line[COLS];
    const char *p = text;

    while (p && *p) {
        int i = 0;
        while (*p && *p != '\n' && i < COLS - 1) {
            if (*p != '\r')
                line[i++] = *p;
            p++;
        }
        while (*p && *p != '\n')
            p++;
        line[i] = '\0';
        print_line(line);
        if (*p == '\n')
            p++;
    }
}
/* ---------------- rendering (base plumbing) ---------------- */

static void set_row(int y, const char *s) {
    int x;
    for (x = 0; x < COLS && s[x]; x++)
        scr[y][x] = s[x];
    for (; x < COLS; x++)
        scr[y][x] = ' ';
}

void render(void) {
    nio_console *csl = nio_get_default();
    int y, x;
    int changed = force_full_redraw;   /* stale prev[]: assume a blit is due */
    for (y = 0; y < ROWS; y++)
        for (x = 0; x < COLS; x++) {
            if (!force_full_redraw && scr[y][x] == prev[y][x])
                continue;
            nio_csl_savechar(csl, scr[y][x], x, y);
            nio_vram_csl_drawchar(csl, x, y);
            prev[y][x] = scr[y][x];
            changed = 1;
        }
    force_full_redraw = 0;
    if (changed)
        nio_vram_draw();               /* full-screen blit: skip if untouched */
}

/* after a launched program exits, our prev[] no longer matches the screen */
void full_redraw(void) {
    force_full_redraw = 1;
}

/* ---------------- screen layout ---------------- */

/* rows 0..27 = scrollback window, row 28 = prompt, row 29 = status bar */
char* build_prompt(char *out, int out_size) {
    //snprintf(out, out_size, "%.*s", out_size - 1, cwd);   // show cwd, e.g. "/"
    /* precision reserves room for "user@tinspire:" + "$ " + NUL */
    snprintf(out, out_size, "user@tinspire:%.*s$ ", out_size - 17, cwd);
    return out;
}

void build_screen(void) {
    char line[COLS];
    char prompt[COLS];
    int y, i, x;

    for (y = 0; y < ROWS - 2; y++) {
        int idx = n_lines - (ROWS - 2) + y;   /* last ROWS-2 lines */
        set_row(y, (idx >= 0 && idx < n_lines) ? scrollback[idx % SCROLL] : "");
    }

    build_prompt(prompt, sizeof(prompt));
    x = 0;

    for (i = 0; prompt[i] && x < COLS - 4; i++)
        line[x++] = prompt[i];

    for (i = 0; i < cmdlen; i++) {
        if (i == cursor) {
            if (x >= COLS - 1)
                break;
            line[x++] = '|';                  /* cursor before the char */
            continue;
        }
        if (x >= COLS - 1)
            break;
        line[x++] = cmdline[i];
    }

    if (cursor >= cmdlen && x < COLS - 1)
        line[x++] = '|';                      /* cursor at end of line */

    line[x] = '\0';
    set_row(ROWS - 2, line);

    set_row(ROWS - 1, "tish  |  help = commands, exit = quit");
}
