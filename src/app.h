#ifndef ZIPEDIT_APP_H
#define ZIPEDIT_APP_H

#define WINVER 0x0A00
#define _WIN32_WINNT 0x0A00
#define _WIN32_IE 0x0A00
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <commdlg.h>
#include <shellapi.h>
#include <shlobj.h>
#include <bcrypt.h>
#include <wincodec.h>
#include <objbase.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <math.h>

#define APP_NAME      L"ZIPedIT"
#define APP_VERSION   L"v4.2 · Crystal (C)"
#define APP_TITLE     L"ZIPedIT"
#define APP_SUBTITLE  L"套娃解压 · 图片收集"

#define MAX_PWD       256
#define MAX_PATH_BUF  2048
#define MAX_LOG_MSG   4096

/* 主题色（AARRGGBB） */
#define C_ACCENT_A   0xFF38BDF8
#define C_ACCENT_B   0xFF6366F1
#define C_SUCCESS    0xFF34D399
#define C_WARNING    0xFFFBBF24
#define C_DANGER     0xFFF87171
#define C_TEXT_MAIN  0xFFE6EAF2
#define C_TEXT_SUB   0xFF98A2B3
#define C_TEXT_MUTE  0xFF5E6676
#define C_BG_TOP     0xFF0D1119
#define C_BG_BOTTOM  0xFF0A0D14
#define C_CARD_BG    0xFF10141E
#define C_CARD_LINE  0x14FFFFFF
#define C_TRACK      0xFF1A2233

static inline uint32_t argb_color(int a, int r, int g, int b) {
    return ((uint32_t)(a & 0xFF) << 24) | ((uint32_t)(r & 0xFF) << 16)
         | ((uint32_t)(g & 0xFF) << 8) | (uint32_t)(b & 0xFF);
}

/* 控件 ID */
enum {
    IDC_PICK = 1001,
    IDC_DROP,
    IDC_COUNT,
    IDC_PWD1,
    IDC_EYE,
    IDC_PWDLIST,
    IDC_PWDEDIT,
    IDC_PWDADD,
    IDC_PWDIMPORT,
    IDC_PWDREMOVE,
    IDC_OUT,
    IDC_BROWSE,
    IDC_RING,
    IDC_STATUSPILL,
    IDC_STATUS,
    IDC_STATS,
    IDC_BAR,
    IDC_START,
    IDC_LOG,
    IDC_CLEAR,
    IDC_EXPORT,
    IDC_SETTINGS,
    IDC_ABOUT,
    IDC_HISTORY,
    IDC_GALLERY,
    IDC_OPEN,
    IDC_SELFTEST,
    IDC_MIN,
    IDC_CLOSE,
    IDC_LOGO,
    IDC_VERSION,
    IDC_TITLE,
    IDC_SUBTITLE,
    IDC_CARDT1,
    IDC_CARDT2,
    IDC_CARDT3,
    IDC_CARDT4,
    IDC_CARDT5,
    IDC_LAB1,
    IDC_LAB2,
    IDC_LABOUT,

    IDD_SEVENZ = 2001,
    IDD_BROWSE7Z,
    IDD_ENGINE0,
    IDD_ENGINE1,
    IDD_MODE0,
    IDD_MODE1,
    IDD_SUBDIR,
    IDD_DEDUP,
    IDD_FMT_PNG,
    IDD_FMT_JPG,
    IDD_FMT_JPEG,
    IDD_FMT_WEBP,
    IDD_FMT_BMP,
    IDD_FMT_GIF,
    IDD_KEEPTEMP,
    IDD_AUTOOPEN,
    IDD_REMEMBER,
    IDD_SAVE,
    IDD_CANCEL,

    IDD_HIST_LIST = 2101,
    IDD_HIST_CLEAR,
    IDD_HIST_CLOSE,

    IDD_GAL_LIST = 2201,
    IDD_GAL_CLOSE,

    IDD_ABOUT_CLOSE = 2301
};

/* 自定义控件消息 */
#define ZNB_SET_KIND   (WM_USER + 10)  /* 按钮样式: 0 primary 1 ghost 2 danger */
#define ZDZ_SET_SUB    (WM_USER + 11)
#define ZR_SET_VALUE   (WM_USER + 12)  /* 0..1000 */
#define ZR_SET_INDET   (WM_USER + 13)
#define ZB_SET_VALUE   (WM_USER + 14)  /* 0..1000 */
#define ZB_SET_ACTIVE  (WM_USER + 15)
#define ZP_SET_COLOR   (WM_USER + 16)

/* 工作线程 -> UI 消息 */
#define WM_APP_LOG      (WM_APP + 1)   /* lParam=wchar_t* malloc, wParam=level */
#define WM_APP_PROGRESS (WM_APP + 2)   /* wParam=0..100 */
#define WM_APP_BUSY     (WM_APP + 3)   /* wParam=bool */
#define WM_APP_FINISHED (WM_APP + 4)   /* lParam=WorkerStats* malloc */

/* 日志级别 */
enum { LOG_STEP = 0, LOG_OK, LOG_WARN, LOG_ERR, LOG_INFO };

typedef struct {
    wchar_t sevenz[MAX_PATH_BUF];
    int engine;      /* 0=内置(tar)优先 1=7z 优先 */
    int mode;        /* 0=仅图片 1=完整解压 */
    int subdir;
    int dedup;
    int keep_temp;
    int auto_open;
    int remember;
    int exts[6];     /* png jpg jpeg webp bmp gif */
} AppConfig;

typedef struct Worker Worker;
typedef struct Dib Dib;

typedef struct {
    wchar_t **files;
    int nfiles;
    wchar_t first_pwd[MAX_PWD];
    wchar_t **seconds;
    int nseconds;
    wchar_t out_dir[MAX_PATH_BUF];
    AppConfig cfg;
    /* UI 运行态 */
    HWND hwnd;
    HINSTANCE inst;
    float k;
    HFONT f_ui, f_bold, f_small, f_title;
    HWND hDrop, hCount, hPwd1, hEye, hPwdList, hPwdEdit, hOut;
    HWND hPick, hBrowse;
    HWND hRing, hBar, hStatusPill, hStatusLabel, hStatsLabel;
    HWND hStart, hLog, hGallery, hOpen, hSelftest, hVersion, hLogo;
    Worker *worker;
    bool running;
    wchar_t *last_out;
    float bg_t;
    bool geek;
    int logo_clicks, ver_idx, last_joke;
    int konami[10];
    int konami_n;
    struct { float x, y, vx, vy, rot, vr; uint32_t color; int size; bool alive; } confetti[160];
    int n_confetti;
    Dib *blob1, *blob2, *blob3;
    Dib *bg;
    int bg_w, bg_h;
    bool rich_ok;
    HBRUSH edit_brush;
} MainData;

typedef struct {
    int files_total, files_ok, files_failed;
    int archives, images, skipped;
    int cancelled;
    ULONGLONG size;
    double elapsed;
    int mode;
    wchar_t out_dir[MAX_PATH_BUF];
} WorkerStats;

typedef void (*LogFn)(void *ctx, int level, const wchar_t *msg);
typedef void (*ProgressFn)(void *ctx, int percent);
typedef bool (*CancelFn)(void *ctx);

/* util.c */
wchar_t *xwcsdup(const wchar_t *s);
void *xmalloc(size_t n);
void *xcalloc(size_t n, size_t sz);
void *xrealloc(void *p, size_t n);
bool file_exists(const wchar_t *p);
bool dir_exists(const wchar_t *p);
void path_join(wchar_t *out, size_t cap, const wchar_t *a, const wchar_t *b);
const wchar_t *path_basename(const wchar_t *p);
bool ends_with_ci(const wchar_t *s, const wchar_t *suffix);
bool supported_archive(const wchar_t *name);
bool image_name_ok(const wchar_t *name, const AppConfig *cfg);
int run_cmdline(const wchar_t *cmdline, int timeout_secs);
wchar_t *find_7z(const wchar_t *custom);
bool make_temp_dir(wchar_t *out, size_t cap);
void remove_tree(const wchar_t *path);
bool copy_file(const wchar_t *src, const wchar_t *dst);
void md5_hex(const wchar_t *path, char out[33]);
void get_data_dir(wchar_t *buf, size_t cap);
void load_config(AppConfig *cfg);
void save_config_all(const AppConfig *cfg, const wchar_t *first,
                     const wchar_t **seconds, int nseconds,
                     const wchar_t **files, int nfiles, const wchar_t *out);
void load_ui_fields(wchar_t *first, size_t fcap, wchar_t ***seconds, int *ns,
                    wchar_t ***files, int *nf, wchar_t *out, size_t ocap);

typedef struct {
    wchar_t ts[64];
    int images, archives, success, mode;
    double elapsed;
    wchar_t out[MAX_PATH_BUF];
    wchar_t **files;
    int nfiles;
} HistoryRec;

void history_load(HistoryRec ***out, int *n);
void history_add(const HistoryRec *r);
void history_clear(void);
void history_free(HistoryRec **recs, int n);

Dib *dib_create(int w, int h);
void dib_free(Dib *d);
void dib_premultiply(Dib *d);
void dib_blend(Dib *d, HDC dc, int x, int y, int w, int h);
void dib_fill_round_gradient(Dib *d, int rad, uint32_t c0, uint32_t c1,
                             uint32_t bg);
void dib_fill_ring(Dib *d, int cx, int cy, int r_outer, int pen, int value,
                   int indeterminate, float phase, uint32_t bg);
void dib_fill_bar(Dib *d, int rad, uint32_t c0, uint32_t c1, int value,
                  int active, float phase, uint32_t bg);
Dib *dib_soft_blob(int size, uint32_t rgb, int alpha);
uint32_t lerp_color(uint32_t a, uint32_t b, float t);

/* worker.c */
bool core_run(const wchar_t **files, int nfiles, const wchar_t *first,
              const wchar_t **seconds, int nsec, const wchar_t *out_dir,
              const AppConfig *cfg, WorkerStats *st,
              LogFn logfn, ProgressFn prog, CancelFn cancel, void *ctx);
Worker *worker_create(HWND hwnd, const wchar_t **files, int nfiles,
                      const wchar_t *first, const wchar_t **seconds, int nsec,
                      const wchar_t *out_dir, const AppConfig *cfg);
void worker_start(Worker *w);
void worker_cancel(Worker *w);
void worker_free(Worker *w);

/* ui.c */
bool ui_register_classes(HINSTANCE hInst);
HWND create_main_window(HINSTANCE hInst);
void maindata_free(MainData *md);
void md_set_files(MainData *md, const wchar_t **paths, int n);
void md_set_out_dir(MainData *md, const wchar_t *p);
void md_append_log(MainData *md, int level, const wchar_t *msg);
void md_save_settings(MainData *md);

/* dialogs.c */
bool dialogs_register_classes(HINSTANCE hInst);
void dialog_open_settings(HWND owner, MainData *md);
void dialog_open_about(HWND owner);
void dialog_open_history(HWND owner, MainData *md);
void dialog_open_gallery(HWND owner, const wchar_t *folder);

/* selftest.c */
int run_selftest(void);

#endif
