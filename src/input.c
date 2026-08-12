
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
    {KEY_NSPIRE_9, '9'},       {KEY_NSPIRE_PLUS, '\x10'}, {KEY_NSPIRE_MINUS, '\x11'},
    {KEY_NSPIRE_eEXP, '\x12'}, {KEY_NSPIRE_TENX, '\x13'}, {KEY_NSPIRE_UP, '\x10'},
    {KEY_NSPIRE_DOWN, '\x11'}, {KEY_NSPIRE_LEFT, '\x12'}, {KEY_NSPIRE_RIGHT, '\x13'},
    {KEY_NSPIRE_DIVIDE, '/'},  {KEY_NSPIRE_PERIOD, '.'},  {KEY_NSPIRE_DEL, '\b'},
    {KEY_NSPIRE_RET, '\n'},    {KEY_NSPIRE_ENTER, '\n'},  {KEY_NSPIRE_ESC, '\x1b'},
    {KEY_NSPIRE_NEGATIVE, '-'}
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
char wait_key(void) {
    while (1) {
        idle();
        char c = read_key();
        if (c)
            return c;
    }
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
            print_line(cmdline);
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
    return 0;
}