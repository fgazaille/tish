/*
 * hello - a tiny .tns that tish can launch, to prove the launch pipeline.
 * Run it inside tish by typing "hello".
 */

#include <os.h>
#include <nspireio/nspireio.h>

int main(void) {
    nio_console *csl = nio_get_default();
    nio_clear(csl);
    nio_puts("hello from hello.tns!\n");
    nio_puts("press any key to return to tish\n");
    wait_key_pressed();
    nio_free(csl);
    return 0;
}