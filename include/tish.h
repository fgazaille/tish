
#ifndef TISH_H
#define TISH_H

#define ROWS 30
#define COLS 53
#define SCROLL 40            /* max scrollback lines kept */

#define CAT_PER_ROW 15
#define CAT_ROWS 3
#define CAT_N (sizeof(cat_chars) - 1)

static char scr[ROWS][COLS];
static char prev[ROWS][COLS];

static char scrollback[SCROLL][COLS];
static int  n_lines = 0;     /* total lines printed (can exceed SCROLL) */

static char cmdline[COLS];
static int  cmdlen = 0;
static int  cursor = 0;    /* caret position in cmdline, 0..cmdlen */

static int force_full_redraw = 1;
static int quitting = 0;

static char cwd[300];
static char docs[300];       /* get_documents_dir() - where .tns live */

static char hist[16][COLS]; /*history size*/
static int hist_len = 0;
static int browse = -1;

/* ---- output sink: where print_line() sends its text ---- */
#define SINK_SCREEN 0        /* normal: the scrollback window */
#define SINK_FILE   1        /* "> file": write through nuc_fwrite */
#define SINK_PIPE   2        /* "cmd | cmd": collect into pipe_buf */

#define PIPE_CAP 4096        /* in-memory pipe between builtins */

static int   sink = SINK_SCREEN;
static NUC_FILE *out_file = 0;
static char  pipe_out[PIPE_CAP];   /* output collected by the current stage */
static int   pipe_out_len = 0;
static char  pipe_in[PIPE_CAP];    /* input handed down from the last stage */
static int   pipe_in_len = 0;
static int   pipe_ready = 0;       /* pipe_in holds data for this stage */
static int   pipe_overflow = 0;    /* a stage exceeded PIPE_CAP: output dropped */
static int   io_error = 0;          /* deferred native file I/O failure */

#define STATUS_SUCCESS 0
#define STATUS_FAIL 1
#define STATUS_MISUSE 2 // eg. invalid arguments
#define STATUS_CMD_NOT_FOUND 127
#define STATUS_SIGNAL_TERMINATED 128 // for when we impletemnt keyboard interrupts and stuff
/* code 128+N command terminated by signal N
ex. 128 + 2 = 130 for CTRL+C (CTRL+C is signal 2)*/
static int last_status = 0;

/* ---- functions shared between modules (single-TU build) ---- */
void init_fs(void);
int  resolve_path(const char *arg, char *out, int out_size);
int  normalize_path(char *dst, int dsz, const char *src);
void print_line(const char *line);
void print_multiline(const char *text);
void full_redraw(void);
void build_screen(void);
void render(void);
char wait_key(void);
int  handleinput(void);
int run_command(const char *line);
int is_ancestor_of(const char *ancestor, const char *path);
bool valid_path(const char* path);


#endif /* TISH_H */
