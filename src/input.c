
#include <os.h>
#include "tish.h"

/* ---------------- input ---------------- */

typedef struct { t_key key; char ch; } KeyDef;

static const KeyDef keys[] = {
    {KEY_NSPIRE_A, 'a'},       {KEY_NSPIRE_B, 'b'},       {KEY_NSPIRE_C, 'c'},
    {KEY_NSPIRE_D, 'd'},       {KEY_NSPIRE_E, 'e'},       {KEY_NSPIRE_F, 'f'},
    {KEY_NSPIRE_G, 'g'},       {KEY_NSPIRE_H, 'h'},       {KEY_NSPIRE_I, 'i'},
    {KEY_NSPIRE_J, 'j'},       {KEY_NSPIRE_K, 'k'},       {KEY_NSPIRE_L, 'l'},
    {KEY_NSPIRE_M, 'm'},       {KEY_NSPIRE_N, 'n'},       {KEY_NSPIRE_O, 'o'},
    {KEY_NSPIRE_P, 'p'},       {KEY_NSPIRE_Q, 'q'},       {KEY_NSPIRE_R, 'r'},
    {KEY_NSPIRE_S, 's'},       {KEY_NSPIRE_T, 't'},       {KEY_NSPIRE_U, 'u'},
    {KEY_NSPIRE_V, 'v'},       {KEY_NSPIRE_W, 'w'},       {KEY_NSPIRE_X, 'x'},
    {KEY_NSPIRE_Y, 'y'},       {KEY_NSPIRE_Z, 'z'},       {KEY_NSPIRE_SPACE, ' '},
    {KEY_NSPIRE_0, '0'},       {KEY_NSPIRE_1, '1'},       {KEY_NSPIRE_2, '2'},
    {KEY_NSPIRE_3, '3'},       {KEY_NSPIRE_4, '4'},       {KEY_NSPIRE_5, '5'},
    {KEY_NSPIRE_6, '6'},       {KEY_NSPIRE_7, '7'},       {KEY_NSPIRE_8, '8'},
    {KEY_NSPIRE_9, '9'},       {KEY_NSPIRE_PERIOD, '.'},  {KEY_NSPIRE_NEGATIVE, '-'},
    {KEY_NSPIRE_PLUS, '+'},    {KEY_NSPIRE_MINUS, '-'},   {KEY_NSPIRE_DIVIDE, '/'},
    {KEY_NSPIRE_MULTIPLY, '*'}, {KEY_NSPIRE_EE, '&'},
    {KEY_NSPIRE_GTHAN, '>'},   {KEY_NSPIRE_BAR, '|'},
    {KEY_NSPIRE_CAT, '\x1c'},
    {KEY_NSPIRE_eEXP, '\x12'}, {KEY_NSPIRE_TENX, '\x13'},
    {KEY_NSPIRE_UP, '\x10'},   {KEY_NSPIRE_DOWN, '\x11'}, {KEY_NSPIRE_LEFT, '\x12'},
    {KEY_NSPIRE_RIGHT, '\x13'},{KEY_NSPIRE_DEL, '\b'},    {KEY_NSPIRE_ESC, '\x1b'},
    {KEY_NSPIRE_RET, '\n'},    {KEY_NSPIRE_ENTER, '\n'}
};

/* Shifted symbols, PC-keyboard style.  ">" has no key of its own on touchpad
 * models (KEY_NSPIRE_GTHAN only exists on the clickpad), so shift+"." types it
 * and redirection stays usable on a CX II. */
static const KeyDef shift_keys[] = {
    {KEY_NSPIRE_PERIOD, '>'},
    {KEY_NSPIRE_DIVIDE, '|'},
    {KEY_NSPIRE_FRAC, '&'}
};

/* returns the pressed key as a simple char, or 0 if nothing is pressed.
 * shift is read live and uppercases letters (same idea as TexEdit). */
static char read_key(void) {
    unsigned i;
    int shift = isKeyPressed(KEY_NSPIRE_SHIFT);
    if (shift) {
        for (i = 0; i < sizeof(shift_keys) / sizeof(shift_keys[0]); i++)
            if (isKeyPressed(shift_keys[i].key))
                return shift_keys[i].ch;
    }
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
char wait_key(void) {
    while (1) {
        idle();
        char c = read_key();
        if (c)
            return c;
    }
}

/* ---------------- CAT key: special-character picker ---------------- */

static const char cat_chars[] = ">|<&;$*?~\\\"'_=+@#%^!:/.()[]{},";
#define CAT_PER_ROW 15
#define CAT_N (sizeof(cat_chars) - 1)              /* 2 rows x 15 */

/* overlays the bottom of the screen: rows 26-27 show the chars, row 28 a
 * hint.  build_screen() redraws over it once the picker closes. */
static void draw_cat_menu(int sel) {
    int row, i;
    for (row = 0; row < 2; row++) {
        char line[COLS];
        int x = 0;
        for (i = 0; i < CAT_PER_ROW; i++) {
            int idx = row * CAT_PER_ROW + i;
            int here = (idx == sel);
            line[x++] = here ? '[' : ' ';
            line[x++] = cat_chars[idx];
            line[x++] = here ? ']' : ' ';
        }
        line[x] = '\0';
        set_row(26 + row, line);
    }
    set_row(28, "arrows: move | enter: use | esc: cancel");
}

/* opens the picker; returns 1 with the chosen char in *out, or 0 on esc. */
static int cat_menu(char *out) {
    int sel = 0;
    draw_cat_menu(sel);
    render();
    wait_no_key_pressed();                         /* release cat first */
    for (;;) {
        idle();
        if (isKeyPressed(KEY_NSPIRE_LEFT))
            sel = (sel + CAT_N - 1) % CAT_N;
        else if (isKeyPressed(KEY_NSPIRE_RIGHT))
            sel = (sel + 1) % CAT_N;
        else if (isKeyPressed(KEY_NSPIRE_DOWN))
            sel = (sel + CAT_PER_ROW) % CAT_N;
        else if (isKeyPressed(KEY_NSPIRE_UP))
            sel = (sel + CAT_N - CAT_PER_ROW) % CAT_N;
        else if (isKeyPressed(KEY_NSPIRE_RET) || isKeyPressed(KEY_NSPIRE_ENTER) ||
                 isKeyPressed(KEY_NSPIRE_CLICK)) {
            *out = cat_chars[sel];
            return 1;
        } else if (isKeyPressed(KEY_NSPIRE_ESC)) {
            return 0;
        } else {
            continue;                              /* nothing pressed yet */
        }
        draw_cat_menu(sel);                        /* moved: redraw + debounce */
        render();
        wait_no_key_pressed();
    }
}

/* insert one character into the command line at the cursor */
static void insert_char(char c) {
    if (cmdlen < COLS - 3) {
        if (cursor < cmdlen)
            memmove(&cmdline[cursor+1], &cmdline[cursor], cmdlen - cursor);
        cmdline[cursor] = c;
        cursor++;
        cmdlen++;
    }
    browse = -1;
}

int handleinput(void){// returns 0: no action needed, 1: quit.
    char c = wait_key();
    if (c == '\x1b') {
        return 1;
        /* ESC quits */
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
            char buf[COLS], prompt[COLS];
            int i = 0, j;
            build_prompt(prompt, sizeof(prompt));
            for (j = 0; prompt[j] && i < COLS - 1; i++, j++)
                buf[i] = prompt[j];
            for (j = 0; cmdline[j] && i < COLS - 1; i++, j++)
                buf[i] = cmdline[j];
            buf[i] = '\0';
            print_line(buf);
            memmove(&hist[1][0], &hist[0][0], 15 * COLS);
            snprintf(hist[0] , COLS, "%s", cmdline);
            if (hist_len < 16) {hist_len++;}
        }
        browse = -1;
        run_command(cmdline);
        if (quitting)
            return 1;
        cmdlen = 0;
        cursor = 0;
    } else if (c == '\x1c') {                 /* CAT: character picker */
        char ch;
        if (cat_menu(&ch))
            insert_char(ch);
    } else if (c) {
        insert_char(c);
    }
    return 0;
}
