#include "app.h"

/* ---------------- 内存 / 字符串 ---------------- */

wchar_t *xwcsdup(const wchar_t *s) {
    if (!s) return NULL;
    size_t n = (wcslen(s) + 1) * sizeof(wchar_t);
    wchar_t *p = (wchar_t *)xmalloc(n);
    memcpy(p, s, n);
    return p;
}

void *xmalloc(size_t n) {
    void *p = malloc(n ? n : 1);
    if (!p) { MessageBoxW(NULL, L"内存不足", APP_NAME, MB_ICONERROR); exit(1); }
    return p;
}

void *xcalloc(size_t n, size_t sz) {
    void *p = calloc(n ? n : 1, sz ? sz : 1);
    if (!p) { MessageBoxW(NULL, L"内存不足", APP_NAME, MB_ICONERROR); exit(1); }
    return p;
}

void *xrealloc(void *p, size_t n) {
    void *q = realloc(p, n ? n : 1);
    if (!q) { MessageBoxW(NULL, L"内存不足", APP_NAME, MB_ICONERROR); exit(1); }
    return q;
}

/* ---------------- 路径 ---------------- */

bool file_exists(const wchar_t *p) {
    DWORD a = GetFileAttributesW(p);
    return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
}

bool dir_exists(const wchar_t *p) {
    DWORD a = GetFileAttributesW(p);
    return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY);
}

void path_join(wchar_t *out, size_t cap, const wchar_t *a, const wchar_t *b) {
    if (!a || !a[0]) { wcsncpy(out, b ? b : L"", cap - 1); out[cap - 1] = 0; return; }
    size_t n = wcslen(a);
    if (cap <= n + 1) { wcsncpy(out, a, cap - 1); out[cap - 1] = 0; return; }
    wcscpy(out, a);
    if (out[n - 1] != L'\\' && out[n - 1] != L'/') {
        if (n + 1 < cap) { out[n] = L'\\'; out[n + 1] = 0; }
    }
    wcsncat(out, b ? b : L"", cap - wcslen(out) - 1);
    out[cap - 1] = 0;
}

const wchar_t *path_basename(const wchar_t *p) {
    const wchar_t *s = p;
    for (const wchar_t *q = p; *q; q++) {
        if (*q == L'\\' || *q == L'/') s = q + 1;
    }
    return s;
}

bool ends_with_ci(const wchar_t *s, const wchar_t *suffix) {
    size_t ls = wcslen(s), lf = wcslen(suffix);
    if (ls < lf) return false;
    return _wcsicmp(s + ls - lf, suffix) == 0;
}

bool supported_archive(const wchar_t *name) {
    return ends_with_ci(name, L".7z") || ends_with_ci(name, L".zip") ||
           ends_with_ci(name, L".rar") || ends_with_ci(name, L".tar") ||
           ends_with_ci(name, L".tar.gz") || ends_with_ci(name, L".tgz");
}

bool image_name_ok(const wchar_t *name, const AppConfig *cfg) {
    static const wchar_t *exts[] = { L".png", L".jpg", L".jpeg", L".webp", L".bmp", L".gif" };
    for (int i = 0; i < 6; i++) {
        if (cfg->exts[i] && ends_with_ci(name, exts[i])) return true;
    }
    return false;
}

/* ---------------- 进程 ---------------- */

int run_cmdline(const wchar_t *cmdline, int timeout_secs) {
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    memset(&si, 0, sizeof(si));
    memset(&pi, 0, sizeof(pi));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    wchar_t *cmd = xwcsdup(cmdline);
    BOOL ok = CreateProcessW(NULL, cmd, NULL, NULL, FALSE,
                             CREATE_NO_WINDOW, NULL, NULL, &si, &pi);
    free(cmd);
    if (!ok) return -1;
    DWORD code = 0;
    DWORD ms = (timeout_secs > 0) ? (DWORD)timeout_secs * 1000 : INFINITE;
    DWORD wait = WaitForSingleObject(pi.hProcess, ms);
    if (wait == WAIT_TIMEOUT) {
        TerminateProcess(pi.hProcess, 1);
        code = 1;
    } else {
        GetExitCodeProcess(pi.hProcess, &code);
    }
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return (int)code;
}

wchar_t *find_7z(const wchar_t *custom) {
    if (custom && custom[0] && file_exists(custom)) return xwcsdup(custom);
    static const wchar_t *names[] = { L"7z.exe", L"7za.exe", L"7zr.exe" };
    wchar_t buf[MAX_PATH_BUF];
    for (int i = 0; i < 3; i++) {
        DWORD n = SearchPathW(NULL, names[i], NULL, MAX_PATH_BUF, buf, NULL);
        if (n && n < MAX_PATH_BUF) return xwcsdup(buf);
    }
    /* 常见安装目录 */
    const wchar_t *dirs[] = {
        L"C:\\Program Files\\7-Zip", L"C:\\Program Files (x86)\\7-Zip",
        L"%ProgramFiles%\\7-Zip", L"%ProgramFiles(x86)%\\7-Zip"
    };
    for (int i = 0; i < 4; i++) {
        const wchar_t *d = dirs[i];
        wchar_t path[MAX_PATH_BUF];
        if (d[0] == L'%') {
            DWORD n = ExpandEnvironmentStringsW(d, path, MAX_PATH_BUF);
            if (!n || n >= MAX_PATH_BUF) continue;
        } else {
            wcsncpy(path, d, MAX_PATH_BUF - 1);
            path[MAX_PATH_BUF - 1] = 0;
        }
        for (int j = 0; j < 3; j++) {
            wchar_t full[MAX_PATH_BUF];
            path_join(full, MAX_PATH_BUF, path, names[j]);
            if (file_exists(full)) return xwcsdup(full);
        }
    }
    return NULL;
}

/* ---------------- 临时目录 / 文件操作 ---------------- */

bool make_temp_dir(wchar_t *out, size_t cap) {
    wchar_t base[MAX_PATH_BUF];
    DWORD n = GetTempPathW(MAX_PATH_BUF, base);
    if (!n || n >= MAX_PATH_BUF) return false;
    for (int i = 0; i < 10000; i++) {
        swprintf(out, cap, L"%lszipit_tmp_%08x", base, (unsigned)(GetTickCount() + i));
        if (!dir_exists(out) && CreateDirectoryW(out, NULL)) return true;
    }
    return false;
}

static void delete_tree_rec(const wchar_t *path) {
    WIN32_FIND_DATAW fd;
    wchar_t pat[MAX_PATH_BUF], sub[MAX_PATH_BUF];
    path_join(pat, MAX_PATH_BUF, path, L"*");
    HANDLE h = FindFirstFileW(pat, &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0)
            continue;
        path_join(sub, MAX_PATH_BUF, path, fd.cFileName);
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            delete_tree_rec(sub);
        } else {
            SetFileAttributesW(sub, FILE_ATTRIBUTE_NORMAL);
            DeleteFileW(sub);
        }
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    SetFileAttributesW(path, FILE_ATTRIBUTE_NORMAL);
    RemoveDirectoryW(path);
}

void remove_tree(const wchar_t *path) {
    if (dir_exists(path)) delete_tree_rec(path);
}

bool copy_file(const wchar_t *src, const wchar_t *dst) {
    return CopyFileW(src, dst, FALSE) != 0;
}

/* ---------------- MD5（BCrypt） ---------------- */

void md5_hex(const wchar_t *path, char out[33]) {
    BCRYPT_ALG_HANDLE alg = NULL;
    BCRYPT_HASH_HANDLE hash = NULL;
    out[0] = 0;
    if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_MD5_ALGORITHM, NULL, 0) != 0) return;
    if (BCryptCreateHash(alg, &hash, NULL, 0, NULL, 0, 0) != 0) {
        BCryptCloseAlgorithmProvider(alg, 0);
        return;
    }
    HANDLE f = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL,
                           OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (f != INVALID_HANDLE_VALUE) {
        BYTE buf[1 << 16];
        DWORD rd;
        while (ReadFile(f, buf, sizeof(buf), &rd, NULL) && rd > 0) {
            BCryptHashData(hash, buf, rd, 0);
        }
        CloseHandle(f);
    }
    BYTE digest[16];
    if (BCryptFinishHash(hash, digest, 16, 0) == 0) {
        for (int i = 0; i < 16; i++) sprintf(out + i * 2, "%02x", digest[i]);
        out[32] = 0;
    }
    BCryptDestroyHash(hash);
    BCryptCloseAlgorithmProvider(alg, 0);
}

/* ---------------- 配置 / 历史 ---------------- */

void get_data_dir(wchar_t *buf, size_t cap) {
    wchar_t env[MAX_PATH_BUF];
    DWORD n = GetEnvironmentVariableW(L"ZIPEDIT_DATA", env, MAX_PATH_BUF);
    const wchar_t *base = (n > 0) ? env : NULL;
    if (!base || !base[0]) {
        n = GetEnvironmentVariableW(L"APPDATA", env, MAX_PATH_BUF);
        base = (n > 0) ? env : NULL;
    }
    if (base && base[0]) {
        path_join(buf, cap, base, L"ZIPedIT");
        CreateDirectoryW(base, NULL);
    } else {
        wchar_t home[MAX_PATH_BUF];
        n = GetEnvironmentVariableW(L"USERPROFILE", home, MAX_PATH_BUF);
        if (!n) {
            wcsncpy(buf, L".zipedit", cap - 1);
            buf[cap - 1] = 0;
            CreateDirectoryW(buf, NULL);
            return;
        }
        path_join(buf, cap, home, L".zipedit");
    }
    CreateDirectoryW(buf, NULL);
}

static void ini_path(wchar_t *buf, size_t cap) {
    wchar_t dir[MAX_PATH_BUF];
    get_data_dir(dir, MAX_PATH_BUF);
    path_join(buf, cap, dir, L"config.ini");
}

static int ini_get_int(const wchar_t *key, int def) {
    wchar_t p[MAX_PATH_BUF], v[64];
    ini_path(p, MAX_PATH_BUF);
    DWORD n = GetPrivateProfileStringW(L"ZIPedIT", key, NULL, v, 64, p);
    if (!n) return def;
    return _wtoi(v);
}

static void ini_get_str(const wchar_t *key, wchar_t *out, size_t cap,
                        const wchar_t *def) {
    wchar_t p[MAX_PATH_BUF];
    ini_path(p, MAX_PATH_BUF);
    DWORD n = GetPrivateProfileStringW(L"ZIPedIT", key, def, out, (DWORD)cap, p);
    out[cap - 1] = 0;
    (void)n;
}

static void ini_set_int(const wchar_t *key, int v) {
    wchar_t p[MAX_PATH_BUF], s[32];
    ini_path(p, MAX_PATH_BUF);
    swprintf(s, 32, L"%d", v);
    WritePrivateProfileStringW(L"ZIPedIT", key, s, p);
}

static void ini_set_str(const wchar_t *key, const wchar_t *v) {
    wchar_t p[MAX_PATH_BUF];
    ini_path(p, MAX_PATH_BUF);
    WritePrivateProfileStringW(L"ZIPedIT", key, v ? v : L"", p);
}

void load_config(AppConfig *cfg) {
    memset(cfg, 0, sizeof(*cfg));
    cfg->engine = 0;
    cfg->mode = 0;
    cfg->auto_open = 1;
    cfg->remember = 1;
    for (int i = 0; i < 6; i++) cfg->exts[i] = 1;
    cfg->engine = ini_get_int(L"engine", 0);
    cfg->mode = ini_get_int(L"mode", 0);
    cfg->subdir = ini_get_int(L"subdir", 0);
    cfg->dedup = ini_get_int(L"dedup", 0);
    cfg->keep_temp = ini_get_int(L"keep_temp", 0);
    cfg->auto_open = ini_get_int(L"auto_open", 1);
    cfg->remember = ini_get_int(L"remember", 1);
    ini_get_str(L"sevenz", cfg->sevenz, MAX_PATH_BUF, L"");
    wchar_t exts[128];
    ini_get_str(L"exts", exts, 128, L"1 1 1 1 1 1");
    int v[6] = {1, 1, 1, 1, 1, 1};
    swscanf(exts, L"%d %d %d %d %d %d", &v[0], &v[1], &v[2], &v[3], &v[4], &v[5]);
    for (int i = 0; i < 6; i++) cfg->exts[i] = v[i] ? 1 : 0;
}

void save_config_all(const AppConfig *cfg, const wchar_t *first,
                     const wchar_t **seconds, int nseconds,
                     const wchar_t **files, int nfiles, const wchar_t *out) {
    ini_set_int(L"remember", cfg->remember);
    if (!cfg->remember) return;
    ini_set_int(L"engine", cfg->engine);
    ini_set_int(L"mode", cfg->mode);
    ini_set_int(L"subdir", cfg->subdir);
    ini_set_int(L"dedup", cfg->dedup);
    ini_set_int(L"keep_temp", cfg->keep_temp);
    ini_set_int(L"auto_open", cfg->auto_open);
    ini_set_str(L"sevenz", cfg->sevenz);
    wchar_t exts[128];
    swprintf(exts, 128, L"%d %d %d %d %d %d",
             cfg->exts[0], cfg->exts[1], cfg->exts[2],
             cfg->exts[3], cfg->exts[4], cfg->exts[5]);
    ini_set_str(L"exts", exts);
    ini_set_str(L"first_pwd", first ? first : L"");
    ini_set_str(L"out_dir", out ? out : L"");
    wchar_t joined[4096];
    joined[0] = 0;
    for (int i = 0; i < nseconds; i++) {
        if (i) wcscat(joined, L"|");
        wcsncat(joined, seconds[i], 4000 - (int)wcslen(joined));
    }
    ini_set_str(L"second_pwds", joined);
    joined[0] = 0;
    for (int i = 0; i < nfiles; i++) {
        if (i) wcscat(joined, L"|");
        wcsncat(joined, files[i], 4000 - (int)wcslen(joined));
    }
    ini_set_str(L"files", joined);
}

static wchar_t **split_pipe(const wchar_t *s, int *n) {
    *n = 0;
    if (!s || !s[0]) return NULL;
    int count = 1;
    for (const wchar_t *p = s; *p; p++) if (*p == L'|') count++;
    wchar_t **arr = (wchar_t **)xcalloc(count, sizeof(wchar_t *));
    const wchar_t *start = s;
    int k = 0;
    for (const wchar_t *p = s;; p++) {
        if (*p == L'|' || *p == 0) {
            size_t len = (size_t)(p - start);
            arr[k] = (wchar_t *)xmalloc((len + 1) * sizeof(wchar_t));
            wcsncpy(arr[k], start, len);
            arr[k][len] = 0;
            k++;
            if (*p == 0) break;
            start = p + 1;
        }
    }
    *n = k;
    return arr;
}

void load_ui_fields(wchar_t *first, size_t fcap, wchar_t ***seconds, int *ns,
                    wchar_t ***files, int *nf, wchar_t *out, size_t ocap) {
    AppConfig cfg;
    load_config(&cfg);
    if (!cfg.remember) {
        first[0] = 0;
        out[0] = 0;
        *seconds = NULL;
        *ns = 0;
        *files = NULL;
        *nf = 0;
        return;
    }
    wchar_t buf[4096];
    ini_get_str(L"first_pwd", first, fcap, L"bilibili.com");
    ini_get_str(L"out_dir", out, ocap, L"extracted_imgs");
    ini_get_str(L"second_pwds", buf, 4096, L"123ab|123AB|ab123");
    *seconds = split_pipe(buf, ns);
    ini_get_str(L"files", buf, 4096, L"");
    *files = split_pipe(buf, nf);
}

/* ---------------- 历史记录 ---------------- */

static void hist_path(wchar_t *buf, size_t cap) {
    wchar_t dir[MAX_PATH_BUF];
    get_data_dir(dir, MAX_PATH_BUF);
    path_join(buf, cap, dir, L"history.txt");
}

static wchar_t *next_field(wchar_t **pos, wchar_t delim) {
    wchar_t *s = *pos;
    if (!s || !*s) return NULL;
    wchar_t *q = wcschr(s, delim);
    if (q) { *q = 0; *pos = q + 1; }
    else *pos = s + wcslen(s);
    return s;
}

static void append_record(const HistoryRec *r) {
    wchar_t p[MAX_PATH_BUF];
    hist_path(p, MAX_PATH_BUF);
    HistoryRec **recs = NULL;
    int n = 0;
    history_load(&recs, &n);
    FILE *w = _wfopen(p, L"w, ccs=UTF-8");
    if (!w) { history_free(recs, n); return; }
    fwprintf(w, L"%ls\t%d\t%d\t%.1f\t%d\t%d\t%ls\t",
             r->ts, r->images, r->archives, r->elapsed, r->success,
             r->mode, r->out);
    for (int i = 0; i < r->nfiles; i++) {
        if (i) fputwc(L'|', w);
        fwprintf(w, L"%ls", r->files[i]);
    }
    fputwc(L'\n', w);
    for (int i = 0; i < n && i < 9; i++) {
        HistoryRec *x = recs[i];
        fwprintf(w, L"%ls\t%d\t%d\t%.1f\t%d\t%d\t%ls\t",
                 x->ts, x->images, x->archives, x->elapsed,
                 x->success, x->mode, x->out);
        for (int j = 0; j < x->nfiles; j++) {
            if (j) fputwc(L'|', w);
            fwprintf(w, L"%ls", x->files[j]);
        }
        fputwc(L'\n', w);
    }
    fclose(w);
    history_free(recs, n);
}

void history_load(HistoryRec ***out, int *n) {
    *out = NULL;
    *n = 0;
    wchar_t p[MAX_PATH_BUF];
    hist_path(p, MAX_PATH_BUF);
    FILE *f = _wfopen(p, L"r, ccs=UTF-8");
    if (!f) return;
    wchar_t line[8192];
    HistoryRec **arr = NULL;
    int cnt = 0;
    while (fgetws(line, 8192, f)) {
        size_t len = wcslen(line);
        while (len && (line[len - 1] == L'\n' || line[len - 1] == L'\r'))
            line[--len] = 0;
        if (!len) continue;
        wchar_t *tab[8];
        int k = 0;
        wchar_t *pos = line;
        while (k < 8) {
            wchar_t *tok = next_field(&pos, L'\t');
            if (!tok) break;
            tab[k++] = tok;
        }
        if (k < 7) continue;
        HistoryRec *r = (HistoryRec *)xcalloc(1, sizeof(HistoryRec));
        wcsncpy(r->ts, tab[0], 63);
        r->images = _wtoi(tab[1]);
        r->archives = _wtoi(tab[2]);
        r->elapsed = _wtof(tab[3]);
        r->success = _wtoi(tab[4]);
        r->mode = _wtoi(tab[5]);
        wcsncpy(r->out, tab[6], MAX_PATH_BUF - 1);
        if (k >= 8) {
            wchar_t *fs = tab[7];
            int fcnt = 1;
            for (wchar_t *q = fs; *q; q++) if (*q == L'|') fcnt++;
            r->files = (wchar_t **)xcalloc(fcnt, sizeof(wchar_t *));
            wchar_t *start = fs;
            for (wchar_t *q = fs;; q++) {
                if (*q == L'|' || *q == 0) {
                    size_t l = (size_t)(q - start);
                    r->files[r->nfiles] = (wchar_t *)xmalloc((l + 1) * sizeof(wchar_t));
                    wcsncpy(r->files[r->nfiles], start, l);
                    r->files[r->nfiles][l] = 0;
                    r->nfiles++;
                    if (*q == 0) break;
                    start = q + 1;
                }
            }
        }
        arr = (HistoryRec **)xrealloc(arr, (cnt + 1) * sizeof(HistoryRec *));
        arr[cnt++] = r;
    }
    fclose(f);
    *out = arr;
    *n = cnt;
}

void history_add(const HistoryRec *r) {
    append_record(r);
}

void history_clear(void) {
    wchar_t p[MAX_PATH_BUF];
    hist_path(p, MAX_PATH_BUF);
    DeleteFileW(p);
}

void history_free(HistoryRec **recs, int n) {
    if (!recs) return;
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < recs[i]->nfiles; j++) free(recs[i]->files[j]);
        free(recs[i]->files);
        free(recs[i]);
    }
    free(recs);
}

/* ---------------- DIB 自绘工具 ---------------- */

struct Dib {
    int w, h;
    HBITMAP bmp;
    HDC dc;
    uint32_t *px;
};

Dib *dib_create(int w, int h) {
    if (w <= 0 || h <= 0) return NULL;
    Dib *d = (Dib *)xcalloc(1, sizeof(Dib));
    d->w = w;
    d->h = h;
    BITMAPINFO bmi;
    memset(&bmi, 0, sizeof(bmi));
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = w;
    bmi.bmiHeader.biHeight = -h;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    d->bmp = CreateDIBSection(NULL, &bmi, DIB_RGB_COLORS, (void **)&d->px, NULL, 0);
    if (!d->bmp) { free(d); return NULL; }
    d->dc = CreateCompatibleDC(NULL);
    SelectObject(d->dc, d->bmp);
    return d;
}

void dib_free(Dib *d) {
    if (!d) return;
    if (d->dc) DeleteDC(d->dc);
    if (d->bmp) DeleteObject(d->bmp);
    free(d);
}

void dib_premultiply(Dib *d) {
    for (int i = 0; i < d->w * d->h; i++) {
        uint32_t c = d->px[i];
        uint32_t a = (c >> 24) & 0xFF;
        uint32_t r = ((c >> 16) & 0xFF) * a / 255;
        uint32_t g = ((c >> 8) & 0xFF) * a / 255;
        uint32_t b = (c & 0xFF) * a / 255;
        d->px[i] = (a << 24) | (r << 16) | (g << 8) | b;
    }
}

void dib_blend(Dib *d, HDC dc, int x, int y, int w, int h) {
    if (!d) return;
    BLENDFUNCTION bf;
    bf.BlendOp = AC_SRC_OVER;
    bf.BlendFlags = 0;
    bf.SourceConstantAlpha = 255;
    bf.AlphaFormat = AC_SRC_ALPHA;
    AlphaBlend(dc, x, y, w, h, d->dc, 0, 0, d->w, d->h, bf);
}

uint32_t lerp_color(uint32_t a, uint32_t b, float t) {
    if (t < 0) t = 0;
    if (t > 1) t = 1;
    uint32_t ra = (a >> 24) & 0xFF, rb = (b >> 24) & 0xFF;
    uint32_t rr = (a >> 16) & 0xFF, r2 = (b >> 16) & 0xFF;
    uint32_t rg = (a >> 8) & 0xFF, g2 = (b >> 8) & 0xFF;
    uint32_t rb2 = a & 0xFF, b2 = b & 0xFF;
    return ((uint32_t)(ra + (rb - ra) * t) << 24) |
           ((uint32_t)(rr + (r2 - rr) * t) << 16) |
           ((uint32_t)(rg + (g2 - rg) * t) << 8) |
           (uint32_t)(rb2 + (b2 - rb2) * t);
}

static float clampf(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

static float round_cov(float x, float y, int w, int h, float rad) {
    /* 圆角矩形内部覆盖率 */
    float cx = clampf(x, rad, (float)(w - 1) - rad);
    float cy = clampf(y, rad, (float)(h - 1) - rad);
    float dx = x - cx, dy = y - cy;
    float dist = sqrtf(dx * dx + dy * dy);
    return clampf(rad + 0.5f - dist, 0.0f, 1.0f);
}

void dib_fill_round_gradient(Dib *d, int rad, uint32_t c0, uint32_t c1,
                             uint32_t bg) {
    for (int y = 0; y < d->h; y++) {
        for (int x = 0; x < d->w; x++) {
            float cov = round_cov((float)x + 0.5f, (float)y + 0.5f,
                                  d->w, d->h, (float)rad);
            uint32_t c = lerp_color(c0, c1, (float)y / (float)(d->h - 1));
            uint32_t mix = lerp_color(bg, c, cov);
            d->px[y * d->w + x] = (mix & 0x00FFFFFF) | 0xFF000000;
        }
    }
}

void dib_fill_ring(Dib *d, int cx, int cy, int r_outer, int pen, int value,
                   int indeterminate, float phase, uint32_t bg) {
    int r_mid = r_outer - pen / 2;
    float start = -90.0f;
    float sweep = value / 1000.0f * 360.0f;
    float seg_start = start, seg_sweep = sweep;
    if (indeterminate) {
        seg_start = phase * 360.0f - 90.0f;
        seg_sweep = 72.0f;
    }
    int lo = r_mid - pen / 2 - 1, hi = r_mid + pen / 2 + 1;
    for (int y = 0; y < d->h; y++) {
        for (int x = 0; x < d->w; x++) {
            int hit = 0, prog = 0;
            for (int sy = 0; sy < 2; sy++) {
                for (int sx = 0; sx < 2; sx++) {
                    float fx = x + 0.25f + sx * 0.5f - cx;
                    float fy = y + 0.25f + sy * 0.5f - cy;
                    float dist = sqrtf(fx * fx + fy * fy);
                    if (dist < lo || dist > hi) continue;
                    hit++;
                    float ang = atan2f(fy, fx) * 180.0f / 3.14159265f;
                    float rel = fmodf(ang - seg_start + 720.0f, 360.0f);
                    if (rel <= seg_sweep) prog++;
                }
            }
            uint32_t col = C_TRACK;
            if (prog > 0) {
                float ang = atan2f((float)(y + 0.5f - cy), (float)(x + 0.5f - cx))
                            * 180.0f / 3.14159265f;
                float rel = fmodf(ang - start + 720.0f, 360.0f);
                float t = sweep > 0 ? clampf(rel / sweep, 0.0f, 1.0f) : 0.0f;
                col = lerp_color(C_ACCENT_A, C_ACCENT_B, t);
            }
            float cov = (float)hit / 4.0f;
            uint32_t mix = lerp_color(bg, col, cov);
            d->px[y * d->w + x] = (mix & 0x00FFFFFF) | 0xFF000000;
        }
    }
}

void dib_fill_bar(Dib *d, int rad, uint32_t c0, uint32_t c1, int value,
                  int active, float phase, uint32_t bg) {
    int fill_w = (int)((float)(d->w - 2) * value / 1000.0f);
    for (int y = 0; y < d->h; y++) {
        for (int x = 0; x < d->w; x++) {
            float cov = round_cov((float)x + 0.5f, (float)y + 0.5f,
                                  d->w, d->h, (float)rad);
            uint32_t col = (x < fill_w) ? lerp_color(c0, c1, (float)x / (float)(d->w - 1))
                                        : C_TRACK;
            if (active && x < fill_w) {
                float p = phase * 0.9f;
                float xf = (float)x / (float)(d->w - 1);
                float shine = 1.0f - fabsf(xf - p) / 0.25f;
                if (shine > 0) {
                    col = argb_color(255,
                        (int)(((col >> 16) & 0xFF) * 0.85f + 38.0f * shine),
                        (int)(((col >> 8) & 0xFF) * 0.85f + 38.0f * shine),
                        (int)((col & 0xFF) * 0.85f + 38.0f * shine));
                }
            }
            uint32_t mix = lerp_color(bg, col, cov);
            d->px[y * d->w + x] = (mix & 0x00FFFFFF) | 0xFF000000;
        }
    }
}

Dib *dib_soft_blob(int size, uint32_t rgb, int alpha) {
    Dib *d = dib_create(size, size);
    if (!d) return NULL;
    float r = size / 2.0f;
    uint32_t ra = (rgb >> 16) & 0xFF, ga = (rgb >> 8) & 0xFF, ba = rgb & 0xFF;
    float peak = alpha / 255.0f;
    for (int y = 0; y < size; y++) {
        for (int x = 0; x < size; x++) {
            float dx = x + 0.5f - r, dy = y + 0.5f - r;
            float dist = sqrtf(dx * dx + dy * dy) / r;
            float a = clampf(1.0f - dist, 0.0f, 1.0f);
            a = a * a;
            d->px[y * size + x] = argb_color((int)(a * peak * 255.0f),
                                             (int)ra, (int)ga, (int)ba);
        }
    }
    dib_premultiply(d);
    return d;
}
