#include "app.h"

/* ================= 解压后端 ================= */

static bool tar_supported(const wchar_t *name) {
    return ends_with_ci(name, L".zip") || ends_with_ci(name, L".tar") ||
           ends_with_ci(name, L".tar.gz") || ends_with_ci(name, L".tgz");
}

static bool extract_with_7z(const wchar_t *arc, const wchar_t *dst,
                            const wchar_t *pwd, const wchar_t *exe) {
    wchar_t cmd[8192];
    if (pwd && pwd[0]) {
        swprintf(cmd, 8192, L"\"%ls\" x \"%ls\" \"-p%ls\" \"-o%ls\" -y -aoa -bso0 -bsp0",
                 exe, arc, pwd, dst);
    } else {
        swprintf(cmd, 8192, L"\"%ls\" x \"%ls\" \"-o%ls\" -y -aoa -bso0 -bsp0",
                 exe, arc, dst);
    }
    return run_cmdline(cmd, 180) == 0;
}

static bool extract_with_tar(const wchar_t *arc, const wchar_t *dst,
                             const wchar_t *pwd) {
    wchar_t cmd[8192];
    /* 先按无密码尝试：普通 zip/tar 即使给了密码也能直接解压 */
    swprintf(cmd, 8192, L"tar.exe -xf \"%ls\" -C \"%ls\"", arc, dst);
    if (run_cmdline(cmd, 120) == 0) return true;
    if (pwd && pwd[0]) {
        swprintf(cmd, 8192, L"tar.exe -xf \"%ls\" -C \"%ls\" --passphrase \"%ls\"",
                 arc, dst, pwd);
        if (run_cmdline(cmd, 120) == 0) return true;
    }
    return false;
}

static bool extract_one(const wchar_t *arc, const wchar_t *dst,
                        const wchar_t *pwd, const AppConfig *cfg,
                        wchar_t **engine_out) {
    wchar_t *exe = find_7z(cfg->sevenz);
    bool can_tar = tar_supported(arc);
    bool can_7z = exe != NULL;
    int order[2] = { 0, 1 };
    if (cfg->engine == 1) { order[0] = 1; order[1] = 0; }
    for (int i = 0; i < 2; i++) {
        if (order[i] == 0 && can_tar) {
            if (extract_with_tar(arc, dst, pwd)) {
                if (engine_out) *engine_out = xwcsdup(L"tar");
                free(exe);
                return true;
            }
        } else if (order[i] == 1 && can_7z) {
            if (extract_with_7z(arc, dst, pwd, exe)) {
                if (engine_out) *engine_out = xwcsdup(L"7z");
                free(exe);
                return true;
            }
        }
    }
    free(exe);
    return false;
}

static bool extract_with_pwds(const wchar_t *arc, const wchar_t *dst,
                              const wchar_t **pwds, int npwds,
                              const AppConfig *cfg, wchar_t **engine_out,
                              wchar_t **pwd_out, LogFn logfn, void *ctx) {
    CreateDirectoryW(dst, NULL);
    for (int i = 0; i < npwds; i++) {
        wchar_t *engine = NULL;
        if (extract_one(arc, dst, pwds[i], cfg, &engine)) {
            if (engine_out) *engine_out = engine; else free(engine);
            if (pwd_out) *pwd_out = xwcsdup(pwds[i]);
            return true;
        }
        free(engine);
        if (npwds > 1 && logfn) {
            wchar_t msg[1024];
            swprintf(msg, 1024, L"  密码 %ls 未通过，尝试下一个…", pwds[i]);
            logfn(ctx, LOG_INFO, msg);
        }
    }
    return false;
}

/* ================= 递归解压 ================= */

static void scan_archives(const wchar_t *dir, int depth,
                          const wchar_t **pwds, int npwds,
                          const AppConfig *cfg, WorkerStats *st,
                          LogFn logfn, CancelFn cancel, void *ctx,
                          bool *found_new) {
    if (depth > 64 || (cancel && cancel(ctx))) return;
    wchar_t pat[MAX_PATH_BUF];
    path_join(pat, MAX_PATH_BUF, dir, L"*");
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW(pat, &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0)
            continue;
        wchar_t full[MAX_PATH_BUF];
        path_join(full, MAX_PATH_BUF, dir, fd.cFileName);
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            scan_archives(full, depth + 1, pwds, npwds, cfg, st,
                          logfn, cancel, ctx, found_new);
            continue;
        }
        if (!supported_archive(fd.cFileName)) continue;
        wchar_t unpack[MAX_PATH_BUF];
        swprintf(unpack, MAX_PATH_BUF, L"%ls_unpacked", full);
        if (dir_exists(unpack)) continue;
        wchar_t *engine = NULL, *pwd = NULL;
        if (extract_with_pwds(full, unpack, pwds, npwds, cfg,
                              &engine, &pwd, logfn, ctx)) {
            st->archives++;
            if (logfn) {
                wchar_t msg[1024];
                swprintf(msg, 1024, L"子包 %ls 解压成功（%ls · 密码 %ls）",
                         fd.cFileName, engine ? engine : L"?",
                         pwd ? pwd : L"");
                logfn(ctx, LOG_OK, msg);
            }
            *found_new = true;
        } else if (logfn) {
            wchar_t msg[1024];
            swprintf(msg, 1024, L"子包 %ls 解压失败", fd.cFileName);
            logfn(ctx, LOG_ERR, msg);
        }
        free(engine);
        free(pwd);
    } while (FindNextFileW(h, &fd));
    FindClose(h);
}

static void recursive_unpack(const wchar_t *root,
                             const wchar_t **pwds, int npwds,
                             const AppConfig *cfg, WorkerStats *st,
                             LogFn logfn, CancelFn cancel, void *ctx) {
    bool found_new = true;
    while (found_new && !(cancel && cancel(ctx))) {
        found_new = false;
        scan_archives(root, 0, pwds, npwds, cfg, st, logfn, cancel, ctx,
                      &found_new);
    }
}

/* ================= 收集文件 ================= */

typedef struct {
    char **items;
    int n;
} DedupSet;

static bool dedup_check(DedupSet *s, const wchar_t *path) {
    char hex[33];
    md5_hex(path, hex);
    if (!hex[0]) return false;
    for (int i = 0; i < s->n; i++) {
        if (strcmp(s->items[i], hex) == 0) return true;
    }
    s->items = (char **)xrealloc(s->items, (s->n + 1) * sizeof(char *));
    s->items[s->n] = (char *)xmalloc(33);
    memcpy(s->items[s->n], hex, 33);
    s->n++;
    return false;
}

static void dedup_free(DedupSet *s) {
    for (int i = 0; i < s->n; i++) free(s->items[i]);
    free(s->items);
}

typedef struct {
    const wchar_t *root;
    const wchar_t *dest_root;
    const AppConfig *cfg;
    WorkerStats *st;
    DedupSet *seen;
    int total;
    int copied;
} CollectCtx;

static void collect_count(const wchar_t *dir, CollectCtx *c) {
    wchar_t pat[MAX_PATH_BUF];
    path_join(pat, MAX_PATH_BUF, dir, L"*");
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW(pat, &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0)
            continue;
        wchar_t full[MAX_PATH_BUF];
        path_join(full, MAX_PATH_BUF, dir, fd.cFileName);
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            collect_count(full, c);
        } else if (c->cfg->mode == 0) {
            if (image_name_ok(fd.cFileName, c->cfg)) c->total++;
        } else {
            if (!supported_archive(fd.cFileName)) c->total++;
        }
    } while (FindNextFileW(h, &fd));
    FindClose(h);
}

static void unique_dest(wchar_t *out, size_t cap, const wchar_t *base,
                        const wchar_t *rel) {
    path_join(out, cap, base, rel);
    const wchar_t *bn = path_basename(rel);
    wchar_t dirpart[MAX_PATH_BUF];
    dirpart[0] = 0;
    const wchar_t *slash = wcsrchr(rel, L'\\');
    if (slash && slash != rel) {
        size_t l = (size_t)(slash - rel);
        wcsncpy(dirpart, rel, l);
        dirpart[l] = 0;
    }
    wchar_t stem[MAX_PATH_BUF], ext[MAX_PATH_BUF];
    const wchar_t *dot = wcsrchr(bn, L'.');
    if (dot && dot != bn) {
        size_t l = (size_t)(dot - bn);
        wcsncpy(stem, bn, l);
        stem[l] = 0;
        wcscpy(ext, dot);
    } else {
        wcscpy(stem, bn);
        ext[0] = 0;
    }
    for (int k = 1; file_exists(out) || dir_exists(out); k++) {
        wchar_t sub[MAX_PATH_BUF];
        swprintf(sub, MAX_PATH_BUF, L"%ls_%d%ls", stem, k, ext);
        if (dirpart[0]) {
            wchar_t full[MAX_PATH_BUF];
            path_join(full, cap, base, dirpart);
            path_join(out, cap, full, sub);
        } else {
            path_join(out, cap, base, sub);
        }
    }
}

static void collect_copy(const wchar_t *dir, CollectCtx *c) {
    if (c->cfg->mode == 0 && c->copied >= c->total) return;
    wchar_t pat[MAX_PATH_BUF];
    path_join(pat, MAX_PATH_BUF, dir, L"*");
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW(pat, &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0)
            continue;
        wchar_t full[MAX_PATH_BUF];
        path_join(full, MAX_PATH_BUF, dir, fd.cFileName);
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            collect_copy(full, c);
            continue;
        }
        bool want = false;
        if (c->cfg->mode == 0) want = image_name_ok(fd.cFileName, c->cfg);
        else want = !supported_archive(fd.cFileName);
        if (!want) continue;
        if (c->seen && dedup_check(c->seen, full)) {
            c->st->skipped++;
            continue;
        }
        wchar_t dest[MAX_PATH_BUF];
        if (c->cfg->mode == 0) {
            unique_dest(dest, MAX_PATH_BUF, c->dest_root, fd.cFileName);
        } else {
            /* 相对路径 */
            const wchar_t *rel = full + wcslen(c->root);
            while (*rel == L'\\' || *rel == L'/') rel++;
            wchar_t full_rel[MAX_PATH_BUF];
            wcsncpy(full_rel, rel, MAX_PATH_BUF - 1);
            full_rel[MAX_PATH_BUF - 1] = 0;
            unique_dest(dest, MAX_PATH_BUF, c->dest_root, full_rel);
            /* 确保父目录存在 */
            wchar_t parent[MAX_PATH_BUF];
            wcscpy(parent, dest);
            wchar_t *slash = wcsrchr(parent, L'\\');
            if (slash) {
                *slash = 0;
                wchar_t cur[MAX_PATH_BUF];
                wcsncpy(cur, c->dest_root, MAX_PATH_BUF - 1);
                cur[MAX_PATH_BUF - 1] = 0;
                const wchar_t *p = parent + wcslen(c->dest_root);
                while (*p == L'\\') p++;
                wchar_t part[MAX_PATH_BUF];
                while (*p) {
                    const wchar_t *q = wcschr(p, L'\\');
                    size_t l = q ? (size_t)(q - p) : wcslen(p);
                    wcsncpy(part, p, l);
                    part[l] = 0;
                    path_join(cur, MAX_PATH_BUF, cur, part);
                    CreateDirectoryW(cur, NULL);
                    if (!q) break;
                    p = q + 1;
                }
            }
        }
        if (copy_file(full, dest)) {
            c->copied++;
            c->st->images++;
            HANDLE hf = CreateFileW(dest, GENERIC_READ, FILE_SHARE_READ, NULL,
                                    OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
            if (hf != INVALID_HANDLE_VALUE) {
                LARGE_INTEGER sz;
                if (GetFileSizeEx(hf, &sz)) c->st->size += (ULONGLONG)sz.QuadPart;
                CloseHandle(hf);
            }
        }
    } while (FindNextFileW(h, &fd));
    FindClose(h);
}

/* ================= 核心流程 ================= */

static void sanitize_stem(const wchar_t *name, wchar_t *out, size_t cap) {
    size_t n = wcslen(name);
    size_t k = 0;
    for (size_t i = 0; i < n && k + 1 < cap; i++) {
        wchar_t c = name[i];
        if (c == L'\\' || c == L'/' || c == L':' || c == L'*' ||
            c == L'?' || c == L'"' || c == L'<' || c == L'>' || c == L'|')
            continue;
        out[k++] = c;
    }
    out[k] = 0;
    while (k > 0 && out[k - 1] == L' ') out[--k] = 0;
    if (!out[0]) wcscpy(out, L"archive");
}

bool core_run(const wchar_t **files, int nfiles, const wchar_t *first,
              const wchar_t **seconds, int nsec, const wchar_t *out_dir,
              const AppConfig *cfg, WorkerStats *st,
              LogFn logfn, ProgressFn prog, CancelFn cancel, void *ctx) {
    memset(st, 0, sizeof(*st));
    st->files_total = nfiles;
    st->mode = cfg->mode;
    wcsncpy(st->out_dir, out_dir, MAX_PATH_BUF - 1);
    st->out_dir[MAX_PATH_BUF - 1] = 0;
    CreateDirectoryW(out_dir, NULL);

    float weight = nfiles ? 100.0f / nfiles : 0.0f;
    DedupSet seen = { NULL, 0 };
    LARGE_INTEGER freq, t0;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&t0);

    for (int i = 0; i < nfiles; i++) {
        if (cancel && cancel(ctx)) { st->cancelled = 1; break; }
        float base = i * weight;
        const wchar_t *arc = files[i];
        const wchar_t *name = path_basename(arc);

        wchar_t tmp[MAX_PATH_BUF];
        if (!make_temp_dir(tmp, MAX_PATH_BUF)) {
            st->files_failed++;
            if (logfn) {
                wchar_t m[1024];
                swprintf(m, 1024, L"无法创建临时目录：%ls", name);
                logfn(ctx, LOG_ERR, m);
            }
            continue;
        }

        if (logfn) {
            wchar_t m[1024];
            swprintf(m, 1024, L"〔%d/%d〕解第一层：%ls", i + 1, nfiles, name);
            logfn(ctx, LOG_STEP, m);
        }

        const wchar_t *first_list[1] = { first };
        wchar_t *engine = NULL, *pwd = NULL;
        bool ok = extract_with_pwds(arc, tmp, first_list, 1, cfg,
                                    &engine, &pwd, logfn, ctx);
        if (!ok) {
            st->files_failed++;
            if (logfn) logfn(ctx, LOG_ERR, L"第一层解压失败（检查一级密码或文件是否损坏）");
            if (prog) prog(ctx, (int)(base + weight));
            if (!cfg->keep_temp) remove_tree(tmp);
            free(engine);
            free(pwd);
            continue;
        }
        st->files_ok++;
        if (logfn) {
            wchar_t m[1024];
            swprintf(m, 1024, L"第一层成功（%ls · 密码 %ls）",
                     engine ? engine : L"?", pwd ? pwd : L"");
            logfn(ctx, LOG_OK, m);
        }
        free(engine);
        free(pwd);
        if (prog) prog(ctx, (int)(base + weight * 0.12f));

        if (logfn) logfn(ctx, LOG_STEP, L"递归解压子包…");
        recursive_unpack(tmp, seconds, nsec, cfg, st, logfn, cancel, ctx);
        if (cancel && cancel(ctx)) {
            st->cancelled = 1;
            if (!cfg->keep_temp) remove_tree(tmp);
            break;
        }

        wchar_t dest_root[MAX_PATH_BUF];
        wcsncpy(dest_root, out_dir, MAX_PATH_BUF - 1);
        dest_root[MAX_PATH_BUF - 1] = 0;
        wchar_t subname[MAX_PATH_BUF];
        if (cfg->subdir) {
            wchar_t stem[MAX_PATH_BUF];
            const wchar_t *dot = wcsrchr(name, L'.');
            size_t l = dot ? (size_t)(dot - name) : wcslen(name);
            wcsncpy(stem, name, l);
            stem[l] = 0;
            sanitize_stem(stem, subname, MAX_PATH_BUF);
            path_join(dest_root, MAX_PATH_BUF, dest_root, subname);
            CreateDirectoryW(dest_root, NULL);
        }

        CollectCtx cc;
        memset(&cc, 0, sizeof(cc));
        cc.root = tmp;
        cc.dest_root = dest_root;
        cc.cfg = cfg;
        cc.st = st;
        cc.seen = cfg->dedup ? &seen : NULL;
        collect_count(tmp, &cc);
        collect_copy(tmp, &cc);

        if (logfn) {
            wchar_t m[1024];
            swprintf(m, 1024, L"本包收集 %d %ls%ls", cc.copied,
                     cfg->mode == 0 ? L"张图片" : L"个文件",
                     cc.copied && st->skipped ? L"，去重跳过部分重复文件" : L"");
            logfn(ctx, LOG_OK, m);
        }

        if (!cfg->keep_temp) {
            remove_tree(tmp);
            if (logfn) logfn(ctx, LOG_INFO, L"临时缓存已清理");
        }
        if (prog) prog(ctx, (int)(base + weight));
    }

    LARGE_INTEGER t1;
    QueryPerformanceCounter(&t1);
    st->elapsed = (double)(t1.QuadPart - t0.QuadPart) / (double)freq.QuadPart;
    dedup_free(&seen);

    bool success = st->files_ok > 0 && !st->cancelled;
    if (logfn) {
        wchar_t m[1024];
        swprintf(m, 1024,
                 L"用时 %.1f 秒 · 成功 %d/%d 个文件 · 子包 %d 个 · %ls %d%ls%d",
                 st->elapsed, st->files_ok, st->files_total, st->archives,
                 cfg->mode == 0 ? L"图片" : L"文件", st->images,
                 st->skipped ? L" · 去重跳过 " : L"",
                 st->skipped);
        if (st->size) {
            wchar_t sz[128];
            swprintf(sz, 128, L" · 共 %.1f MB", (double)st->size / 1048576.0);
            wcscat(m, sz);
        }
        logfn(ctx, success ? LOG_OK : LOG_ERR, m);
    }
    return success;
}

/* ================= 后台线程 ================= */

struct Worker {
    HWND hwnd;
    volatile LONG cancel;
    HANDLE thread;
    wchar_t **files;
    int nfiles;
    wchar_t *first;
    wchar_t **seconds;
    int nsec;
    wchar_t *out;
    AppConfig cfg;
};

static void worker_log(void *ctx, int level, const wchar_t *msg) {
    Worker *w = (Worker *)ctx;
    PostMessageW(w->hwnd, WM_APP_LOG, (WPARAM)level, (LPARAM)xwcsdup(msg));
}

static void worker_progress(void *ctx, int percent) {
    Worker *w = (Worker *)ctx;
    PostMessageW(w->hwnd, WM_APP_PROGRESS, (WPARAM)percent, 0);
}

static bool worker_is_cancelled(void *ctx) {
    Worker *w = (Worker *)ctx;
    return InterlockedCompareExchange(&w->cancel, 0, 0) != 0;
}

static DWORD WINAPI worker_thread(LPVOID param) {
    Worker *w = (Worker *)param;
    WorkerStats st;
    const wchar_t **files = (const wchar_t **)w->files;
    const wchar_t **seconds = (const wchar_t **)w->seconds;
    bool ok = core_run(files, w->nfiles, w->first, seconds, w->nsec,
                       w->out, &w->cfg, &st,
                       worker_log, worker_progress, worker_is_cancelled, w);
    (void)ok;
    WorkerStats *copy = (WorkerStats *)xmalloc(sizeof(WorkerStats));
    memcpy(copy, &st, sizeof(WorkerStats));
    PostMessageW(w->hwnd, WM_APP_FINISHED, 0, (LPARAM)copy);
    return 0;
}

Worker *worker_create(HWND hwnd, const wchar_t **files, int nfiles,
                      const wchar_t *first, const wchar_t **seconds, int nsec,
                      const wchar_t *out_dir, const AppConfig *cfg) {
    Worker *w = (Worker *)xcalloc(1, sizeof(Worker));
    w->hwnd = hwnd;
    w->nfiles = nfiles;
    w->nsec = nsec;
    w->first = xwcsdup(first);
    w->out = xwcsdup(out_dir);
    w->cfg = *cfg;
    w->files = (wchar_t **)xcalloc(nfiles, sizeof(wchar_t *));
    for (int i = 0; i < nfiles; i++) w->files[i] = xwcsdup(files[i]);
    w->seconds = (wchar_t **)xcalloc(nsec ? nsec : 1, sizeof(wchar_t *));
    for (int i = 0; i < nsec; i++) w->seconds[i] = xwcsdup(seconds[i]);
    return w;
}

void worker_start(Worker *w) {
    w->thread = CreateThread(NULL, 0, worker_thread, w, 0, NULL);
}

void worker_cancel(Worker *w) {
    InterlockedExchange(&w->cancel, 1);
}

void worker_free(Worker *w) {
    if (!w) return;
    if (w->thread) {
        WaitForSingleObject(w->thread, INFINITE);
        CloseHandle(w->thread);
    }
    for (int i = 0; i < w->nfiles; i++) free(w->files[i]);
    free(w->files);
    for (int i = 0; i < w->nsec; i++) free(w->seconds[i]);
    free(w->seconds);
    free(w->first);
    free(w->out);
    free(w);
}
