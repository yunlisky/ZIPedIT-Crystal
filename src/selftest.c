#include "app.h"

static void write_bytes(const wchar_t *path, const unsigned char *data, size_t n) {
    FILE *f = _wfopen(path, L"wb");
    if (!f) return;
    fwrite(data, 1, n, f);
    fclose(f);
}

static bool tar_create(const wchar_t *outarc, const wchar_t *cwd,
                       const wchar_t **names, int n) {
    wchar_t cmd[8192];
    int pos = swprintf(cmd, 8192, L"tar.exe -a -cf \"%ls\" -C \"%ls\"",
                       outarc, cwd);
    for (int i = 0; i < n && pos < 8000; i++)
        pos += swprintf(cmd + pos, 8192 - pos, L" \"%ls\"", names[i]);
    int rc = run_cmdline(cmd, 120);
    if (rc != 0) wprintf(L"  tar cmd failed rc=%d: %ls\n", rc, cmd);
    return rc == 0;
}

static void selftest_log(void *ctx, int level, const wchar_t *msg) {
    (void)level;
    (void)ctx;
    wprintf(L"  %ls\n", msg);
}

static int dir_file_count(const wchar_t *dir) {
    int n = 0;
    wchar_t pat[MAX_PATH_BUF];
    path_join(pat, MAX_PATH_BUF, dir, L"*");
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW(pat, &fd);
    if (h == INVALID_HANDLE_VALUE) return 0;
    do {
        if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0)
            continue;
        wchar_t full[MAX_PATH_BUF];
        path_join(full, MAX_PATH_BUF, dir, fd.cFileName);
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            n += dir_file_count(full);
        else
            n++;
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    return n;
}

int run_selftest(void) {
    wprintf(L"ZIPedIT C selftest\n");
    wchar_t base[MAX_PATH_BUF];
    if (!make_temp_dir(base, MAX_PATH_BUF)) {
        wprintf(L"SELFTEST FAIL: cannot create temp dir\n");
        return 1;
    }
    int rc = 0;
    do {
        wchar_t src[MAX_PATH_BUF], work[MAX_PATH_BUF];
        path_join(src, MAX_PATH_BUF, base, L"src");
        CreateDirectoryW(src, NULL);

        static const unsigned char png_head[] = { 0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n' };
        static const unsigned char jpg_head[] = { 0xFF, 0xD8, 0xFF, 0xE0 };
        unsigned char a[40], b[40], c[40], d[40];
        memcpy(a, png_head, 8);
        memset(a + 8, 'A', 32);
        memcpy(b, jpg_head, 4);
        memset(b + 4, 'B', 36);
        memcpy(c, png_head, 8);
        memset(c + 8, 'C', 32);
        memcpy(d, png_head, 8);
        memset(d + 8, 'D', 32);

        wchar_t p_a[MAX_PATH_BUF], p_b[MAX_PATH_BUF], p_c[MAX_PATH_BUF], p_d[MAX_PATH_BUF];
        path_join(p_a, MAX_PATH_BUF, src, L"a.png");
        path_join(p_b, MAX_PATH_BUF, src, L"b.jpg");
        path_join(p_c, MAX_PATH_BUF, src, L"c.png");
        path_join(p_d, MAX_PATH_BUF, src, L"d.png");
        write_bytes(p_a, a, sizeof(a));
        write_bytes(p_b, b, sizeof(b));
        write_bytes(p_c, c, sizeof(c));
        write_bytes(p_d, d, sizeof(d));
        wchar_t dup_dir[MAX_PATH_BUF];
        path_join(dup_dir, MAX_PATH_BUF, src, L"dup");
        CreateDirectoryW(dup_dir, NULL);
        wchar_t p_dup[MAX_PATH_BUF];
        path_join(p_dup, MAX_PATH_BUF, dup_dir, L"a.png");
        write_bytes(p_dup, a, sizeof(a));

        wchar_t inner[MAX_PATH_BUF], plain[MAX_PATH_BUF], bundle[MAX_PATH_BUF];
        path_join(inner, MAX_PATH_BUF, base, L"inner.zip");
        path_join(plain, MAX_PATH_BUF, base, L"plain.zip");
        path_join(bundle, MAX_PATH_BUF, base, L"bundle.tar.gz");
        const wchar_t *inner_files[] = { L"a.png", L"b.jpg" };
        const wchar_t *plain_files[] = { L"c.png" };
        const wchar_t *bundle_files[] = { L"dup/a.png", L"d.png" };
        if (!tar_create(inner, src, inner_files, 2) ||
            !tar_create(plain, src, plain_files, 1) ||
            !tar_create(bundle, src, bundle_files, 2)) {
            wprintf(L"SELFTEST FAIL: cannot create test archives\n");
            rc = 1;
            break;
        }

        wchar_t outer[MAX_PATH_BUF];
        path_join(outer, MAX_PATH_BUF, base, L"outer.zip");
        path_join(work, MAX_PATH_BUF, base, L"work");
        CreateDirectoryW(work, NULL);
        wchar_t inner_copy[MAX_PATH_BUF], plain_copy[MAX_PATH_BUF], bundle_copy[MAX_PATH_BUF];
        path_join(inner_copy, MAX_PATH_BUF, work, L"inner.zip");
        path_join(plain_copy, MAX_PATH_BUF, work, L"plain.zip");
        path_join(bundle_copy, MAX_PATH_BUF, work, L"bundle.tar.gz");
        CopyFileW(inner, inner_copy, FALSE);
        CopyFileW(plain, plain_copy, FALSE);
        CopyFileW(bundle, bundle_copy, FALSE);
        const wchar_t *outer_files[] = { L"inner.zip", L"plain.zip", L"bundle.tar.gz" };
        if (!tar_create(outer, work, outer_files, 3)) {
            wprintf(L"SELFTEST FAIL: cannot create outer.zip\n");
            rc = 1;
            break;
        }

        AppConfig cfg;
        memset(&cfg, 0, sizeof(cfg));
        for (int i = 0; i < 6; i++) cfg.exts[i] = 1;
        cfg.engine = 0;
        cfg.mode = 0;
        cfg.dedup = 1;
        cfg.keep_temp = 0;
        const wchar_t *files[] = { outer };
        const wchar_t *seconds[] = { L"123ab", L"wrong" };

        wchar_t out1[MAX_PATH_BUF];
        path_join(out1, MAX_PATH_BUF, base, L"out1");
        WorkerStats st1;
        bool ok1 = core_run(files, 1, L"bilibili.com", seconds, 2, out1,
                            &cfg, &st1, selftest_log, NULL, NULL, NULL);
        int got1 = dir_file_count(out1);
        if (!ok1 || got1 != 4 || st1.images != 4 || st1.skipped != 1) {
            wprintf(L"SELFTEST FAIL: images mode got files=%d images=%d skipped=%d\n",
                    got1, st1.images, st1.skipped);
            rc = 1;
            break;
        }
        wprintf(L"  [1/2] images mode + MD5 dedup: OK (%d files, %d skipped)\n",
                got1, st1.skipped);

        wchar_t out2[MAX_PATH_BUF];
        path_join(out2, MAX_PATH_BUF, base, L"out2");
        cfg.mode = 1;
        cfg.subdir = 1;
        WorkerStats st2;
        bool ok2 = core_run(files, 1, L"bilibili.com", seconds, 2, out2,
                            &cfg, &st2, selftest_log, NULL, NULL, NULL);
        int got2 = dir_file_count(out2);
        wchar_t sub[MAX_PATH_BUF];
        path_join(sub, MAX_PATH_BUF, out2, L"outer");
        if (!ok2 || got2 != 4 || !dir_exists(sub)) {
            wprintf(L"SELFTEST FAIL: all mode got=%d subdir=%d\n",
                    got2, dir_exists(sub));
            rc = 1;
            break;
        }
        wprintf(L"  [2/2] all mode + subdir: OK (%d files)\n", got2);

        /* 历史记录读写 */
        wchar_t data_dir[MAX_PATH_BUF];
        path_join(data_dir, MAX_PATH_BUF, base, L"data");
        CreateDirectoryW(data_dir, NULL);
        SetEnvironmentVariableW(L"ZIPEDIT_DATA", data_dir);
        history_clear();
        HistoryRec hr;
        memset(&hr, 0, sizeof(hr));
        wcsncpy(hr.ts, L"08-04 12:00", 63);
        hr.images = 4;
        hr.archives = 3;
        hr.elapsed = 0.5;
        hr.success = 1;
        hr.mode = 0;
        wcsncpy(hr.out, base, MAX_PATH_BUF - 1);
        wchar_t *hist_files[1] = { (wchar_t *)outer };
        hr.files = hist_files;
        hr.nfiles = 1;
        history_add(&hr);
        HistoryRec hr2;
        memset(&hr2, 0, sizeof(hr2));
        wcsncpy(hr2.ts, L"09-04 13:00", 63);
        hr2.images = 5;
        hr2.archives = 4;
        hr2.elapsed = 0.7;
        hr2.success = 1;
        hr2.mode = 0;
        wcsncpy(hr2.out, base, MAX_PATH_BUF - 1);
        hr2.files = hist_files;
        hr2.nfiles = 1;
        history_add(&hr2);
        HistoryRec **recs = NULL;
        int nrec = 0;
        history_load(&recs, &nrec);
        bool hist_ok = nrec == 2 && recs[0]->images == 5 &&
                       recs[0]->nfiles == 1 &&
                       wcscmp(recs[0]->ts, L"09-04 13:00") == 0 &&
                       recs[1]->images == 4 &&
                       wcscmp(recs[0]->files[0], outer) == 0;
        history_free(recs, nrec);
        history_clear();
        if (!hist_ok) {
            wprintf(L"SELFTEST FAIL: history read/write\n");
            rc = 1;
            break;
        }
        wprintf(L"  [3/3] history read/write: OK\n");
        wprintf(L"SELFTEST OK: dedup / subdir / all-mode / multi-format passed\n");
    } while (0);
    remove_tree(base);
    return rc;
}
