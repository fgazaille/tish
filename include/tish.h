
#ifndef TISH_H
#define TISH_H

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
static int   io_error = 0;          /* deferred native file I/O failure */

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
void run_command(const char *line);

#endif /* TISH_H */
