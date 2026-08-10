
/* ---------------- scrollback output ---------------- */

void print_line(const char *line) {
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

void render(void) {
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
void full_redraw(void) {
    force_full_redraw = 1;
}

/* ---------------- screen layout ---------------- */

/* rows 0..27 = scrollback window, row 28 = prompt, row 29 = status bar */
static void build_prompt(char *out, int out_size) {
    snprintf(out, out_size, "%.*s", out_size - 1, cwd);   /* show cwd, e.g. "/" */
}

void build_screen(void) {
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