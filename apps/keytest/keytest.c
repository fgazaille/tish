/*
 * keytest - identify which physical key maps to which KEY_NSPIRE_* constant.
 *
 * Press any key: its constant name is printed. Esc exits.
 *
 * Notes:
 *  - keys with no physical counterpart on your model (e.g. SIN/COS on the
 *    CX II touchpad keypad) simply never trigger; that is also useful info.
 *  - the touchpad arrows are read through the touchpad, so on the Firebird
 *    emulator they will not respond (use the on-screen keypad).
 */

#include <os.h>
#include <nspireio/nspireio.h>

#define ENTRY(k) { k, #k }

static const struct {
    t_key key;
    const char *name;
} keys[] = {
    ENTRY(KEY_NSPIRE_RET),        ENTRY(KEY_NSPIRE_ENTER),
    ENTRY(KEY_NSPIRE_SPACE),      ENTRY(KEY_NSPIRE_NEGATIVE),
    ENTRY(KEY_NSPIRE_Z),          ENTRY(KEY_NSPIRE_PERIOD),
    ENTRY(KEY_NSPIRE_Y),          ENTRY(KEY_NSPIRE_0),
    ENTRY(KEY_NSPIRE_X),          ENTRY(KEY_NSPIRE_THETA),
    ENTRY(KEY_NSPIRE_COMMA),      ENTRY(KEY_NSPIRE_PLUS),
    ENTRY(KEY_NSPIRE_W),          ENTRY(KEY_NSPIRE_3),
    ENTRY(KEY_NSPIRE_V),          ENTRY(KEY_NSPIRE_2),
    ENTRY(KEY_NSPIRE_U),          ENTRY(KEY_NSPIRE_1),
    ENTRY(KEY_NSPIRE_T),          ENTRY(KEY_NSPIRE_eEXP),
    ENTRY(KEY_NSPIRE_PI),         ENTRY(KEY_NSPIRE_QUES),
    ENTRY(KEY_NSPIRE_QUESEXCL),   ENTRY(KEY_NSPIRE_MINUS),
    ENTRY(KEY_NSPIRE_S),          ENTRY(KEY_NSPIRE_6),
    ENTRY(KEY_NSPIRE_R),          ENTRY(KEY_NSPIRE_5),
    ENTRY(KEY_NSPIRE_Q),          ENTRY(KEY_NSPIRE_4),
    ENTRY(KEY_NSPIRE_P),          ENTRY(KEY_NSPIRE_TENX),
    ENTRY(KEY_NSPIRE_EE),         ENTRY(KEY_NSPIRE_COLON),
    ENTRY(KEY_NSPIRE_MULTIPLY),   ENTRY(KEY_NSPIRE_O),
    ENTRY(KEY_NSPIRE_9),          ENTRY(KEY_NSPIRE_N),
    ENTRY(KEY_NSPIRE_8),          ENTRY(KEY_NSPIRE_M),
    ENTRY(KEY_NSPIRE_7),          ENTRY(KEY_NSPIRE_L),
    ENTRY(KEY_NSPIRE_SQU),        ENTRY(KEY_NSPIRE_II),
    ENTRY(KEY_NSPIRE_QUOTE),      ENTRY(KEY_NSPIRE_DIVIDE),
    ENTRY(KEY_NSPIRE_K),          ENTRY(KEY_NSPIRE_TAN),
    ENTRY(KEY_NSPIRE_J),          ENTRY(KEY_NSPIRE_COS),
    ENTRY(KEY_NSPIRE_I),          ENTRY(KEY_NSPIRE_SIN),
    ENTRY(KEY_NSPIRE_H),          ENTRY(KEY_NSPIRE_EXP),
    ENTRY(KEY_NSPIRE_GTHAN),      ENTRY(KEY_NSPIRE_APOSTROPHE),
    ENTRY(KEY_NSPIRE_CAT),        ENTRY(KEY_NSPIRE_FRAC),
    ENTRY(KEY_NSPIRE_G),          ENTRY(KEY_NSPIRE_RP),
    ENTRY(KEY_NSPIRE_F),          ENTRY(KEY_NSPIRE_LP),
    ENTRY(KEY_NSPIRE_E),          ENTRY(KEY_NSPIRE_VAR),
    ENTRY(KEY_NSPIRE_D),          ENTRY(KEY_NSPIRE_DEL),
    ENTRY(KEY_NSPIRE_LTHAN),      ENTRY(KEY_NSPIRE_FLAG),
    ENTRY(KEY_NSPIRE_CLICK),      ENTRY(KEY_NSPIRE_C),
    ENTRY(KEY_NSPIRE_HOME),       ENTRY(KEY_NSPIRE_B),
    ENTRY(KEY_NSPIRE_MENU),       ENTRY(KEY_NSPIRE_A),
    ENTRY(KEY_NSPIRE_ESC),        ENTRY(KEY_NSPIRE_BAR),
    ENTRY(KEY_NSPIRE_TAB),        ENTRY(KEY_NSPIRE_EQU),
    ENTRY(KEY_NSPIRE_UP),         ENTRY(KEY_NSPIRE_UPRIGHT),
    ENTRY(KEY_NSPIRE_RIGHT),      ENTRY(KEY_NSPIRE_RIGHTDOWN),
    ENTRY(KEY_NSPIRE_DOWN),       ENTRY(KEY_NSPIRE_DOWNLEFT),
    ENTRY(KEY_NSPIRE_LEFT),       ENTRY(KEY_NSPIRE_LEFTUP),
    ENTRY(KEY_NSPIRE_SHIFT),      ENTRY(KEY_NSPIRE_CTRL),
    ENTRY(KEY_NSPIRE_DOC),        ENTRY(KEY_NSPIRE_TRIG),
    ENTRY(KEY_NSPIRE_SCRATCHPAD),
};

#define NKEYS (sizeof(keys) / sizeof(keys[0]))

static unsigned char was[NKEYS];

int main(void) {
    nio_console *csl = nio_get_default();
    unsigned i;

    nio_clear(csl);
    nio_puts("keytest: press a key to see its KEY_NSPIRE_* name.\n");
    nio_puts("esc exits.\n");

    for (;;) {
        for (i = 0; i < NKEYS; i++) {
            if (isKeyPressed(keys[i].key)) {
                if (!was[i]) {
                    was[i] = 1;
                    nio_printf("%s\n", keys[i].name);
                    if (keys[i].key.row == KEY_NSPIRE_ESC.row &&
                        keys[i].key.col == KEY_NSPIRE_ESC.col) {
                        nio_free(csl);
                        return 0;
                    }
                }
            } else {
                was[i] = 0;
            }
        }
        idle();
    }
}
