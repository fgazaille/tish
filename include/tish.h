
#define ROWS 30
#define COLS 53
#define SCROLL 40            /* max scrollback lines kept */

static char scr[ROWS][COLS];
static char prev[ROWS][COLS];

static char scrollback[SCROLL][COLS];
static int  n_lines = 0;     /* total lines printed (can exceed SCROLL) */

static char cmdline[COLS];
static int  cmdlen = 0;
static int  cursor = 0;    /* caret position in cmdline, 0..cmdlen */

static int force_full_redraw = 1;
static int quitting = 0;

static char cwd[300];        /* current directory (kept in sync via chdir/getcwd) */
static char docs[300];       /* get_documents_dir() - where .tns live */

static char hist[16][COLS]; /*history size*/
static int hist_len = 0;
static int browse = -1;