#include "app.h"
#include <richedit.h>

/* ---------------- 通用小工具 ---------------- */

static float dlg_scale(HWND owner) {
    HDC dc = GetDC(owner ? owner : NULL);
    int dpi = GetDeviceCaps(dc, LOGPIXELSY);
    ReleaseDC(owner ? owner : NULL, dc);
    float s = dpi / 96.0f;
    return s < 1.0f ? 1.0f : s;
}

static HFONT dlg_font(int size, int bold) {
    HDC dc = GetDC(NULL);
    int dpi = GetDeviceCaps(dc, LOGPIXELSY);
    ReleaseDC(NULL, dc);
    return CreateFontW(-MulDiv(size, dpi, 72), 0, 0, 0,
                       bold ? FW_SEMIBOLD : FW_NORMAL, 0, 0, 0,
                       DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                       CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                       DEFAULT_PITCH, L"Microsoft YaHei UI");
}

static HWND dlg_mk(HWND parent, const wchar_t *cls, const wchar_t *text,
                   int id, int x, int y, int w, int h, DWORD style, DWORD ex) {
    return CreateWindowExW(ex, cls, text, WS_CHILD | WS_VISIBLE | style,
                           x, y, w, h, parent, (HMENU)(intptr_t)id,
                           GetModuleHandleW(NULL), NULL);
}

static void paint_dlg_bg(HWND h) {
    PAINTSTRUCT ps;
    HDC dc = BeginPaint(h, &ps);
    RECT rc;
    GetClientRect(h, &rc);
    int w = rc.right, hh = rc.bottom;
    if (w > 0 && hh > 0) {
        Dib *d = dib_create(w, hh);
        if (d) {
            dib_fill_round_gradient(d, 10, C_BG_TOP, C_BG_BOTTOM,
                                    C_BG_BOTTOM);
            dib_premultiply(d);
            dib_blend(d, dc, 0, 0, w, hh);
            dib_free(d);
        }
    }
    EndPaint(h, &ps);
}

static int run_modal(HWND owner, HWND dlg, const bool *done) {
    EnableWindow(owner, FALSE);
    MSG msg;
    while (!*done && GetMessageW(&msg, NULL, 0, 0)) {
        if (msg.message == WM_QUIT) {
            PostQuitMessage((int)msg.wParam);
            break;
        }
        if (!IsDialogMessageW(dlg, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }
    EnableWindow(owner, TRUE);
    SetForegroundWindow(owner);
    return 0;
}

static HWND center_dialog(HWND owner, const wchar_t *cls, const wchar_t *title,
                          int w, int h, void *data) {
    RECT wr = { 0, 0, 0, 0 };
    if (owner) GetWindowRect(owner, &wr);
    int x = wr.left + (wr.right - wr.left - w) / 2;
    int y = wr.top + (wr.bottom - wr.top - h) / 2;
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    return CreateWindowExW(0, cls, title,
                           WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_CLIPCHILDREN,
                           x, y, w, h, owner, NULL,
                           GetModuleHandleW(NULL), data);
}

/* ================= 设置 ================= */

typedef struct {
    MainData *md;
    float k;
    HFONT f_ui, f_bold;
    HBRUSH bg_brush;
    HWND hSevenz, hEngine0, hEngine1, hMode0, hMode1;
    HWND hSubdir, hDedup, hFmt[6], hKeep, hAuto, hRemember;
    bool done;
} SettingsData;

static LRESULT CALLBACK SettingsProc(HWND h, UINT m, WPARAM wp, LPARAM lp) {
    SettingsData *d = (SettingsData *)GetWindowLongPtrW(h, GWLP_USERDATA);
    if (m == WM_NCCREATE) {
        d = (SettingsData *)((CREATESTRUCTW *)lp)->lpCreateParams;
        SetWindowLongPtrW(h, GWLP_USERDATA, (LONG_PTR)d);
        return TRUE;
    }
    if (!d) return DefWindowProcW(h, m, wp, lp);
    switch (m) {
    case WM_CREATE: {
        d->f_ui = dlg_font(10, 0);
        d->f_bold = dlg_font(14, 1);
        d->bg_brush = CreateSolidBrush(RGB(0x10, 0x14, 0x1E));
        float k = d->k;
#define K(v) ((int)((v) * k))
        int w = K(560);
        HWND title = dlg_mk(h, L"STATIC", L"⚙️ 设置", 0, K(24), K(16), K(200), K(30), 0, 0);
        SendMessageW(title, WM_SETFONT, (WPARAM)d->f_bold, TRUE);
        HWND l1 = dlg_mk(h, L"STATIC", L"7z 可执行文件", 0, K(24), K(58), K(100), K(32),
                         SS_LEFT | SS_CENTERIMAGE, 0);
        SendMessageW(l1, WM_SETFONT, (WPARAM)d->f_ui, TRUE);
        d->hSevenz = dlg_mk(h, L"EDIT", d->md->cfg.sevenz, IDD_SEVENZ,
                            K(130), K(58), K(330), K(32), WS_TABSTOP | ES_AUTOHSCROLL,
                            WS_EX_CLIENTEDGE);
        SendMessageW(d->hSevenz, WM_SETFONT, (WPARAM)d->f_ui, TRUE);
        HWND browse = dlg_mk(h, L"BUTTON", L"浏览", IDD_BROWSE7Z,
                             K(466), K(58), K(64), K(32), BS_PUSHBUTTON, 0);
        SendMessageW(browse, WM_SETFONT, (WPARAM)d->f_ui, TRUE);
        HWND hint = dlg_mk(h, L"STATIC",
                           L"留空自动在 PATH 查找（用于内置后端失败时的后备引擎）",
                           0, K(24), K(96), K(520), K(18), 0, 0);
        SendMessageW(hint, WM_SETFONT, (WPARAM)d->f_ui, TRUE);
        HWND l2 = dlg_mk(h, L"STATIC", L"解压引擎顺序", 0, K(24), K(126), K(200), K(20), 0, 0);
        SendMessageW(l2, WM_SETFONT, (WPARAM)d->f_ui, TRUE);
        d->hEngine0 = dlg_mk(h, L"BUTTON", L"内置（tar）优先", IDD_ENGINE0,
                             K(40), K(150), K(200), K(24), BS_AUTORADIOBUTTON | WS_GROUP, 0);
        d->hEngine1 = dlg_mk(h, L"BUTTON", L"7z.exe 优先", IDD_ENGINE1,
                             K(40), K(174), K(200), K(24), BS_AUTORADIOBUTTON, 0);
        SendMessageW(d->hEngine0, WM_SETFONT, (WPARAM)d->f_ui, TRUE);
        SendMessageW(d->hEngine1, WM_SETFONT, (WPARAM)d->f_ui, TRUE);
        HWND l3 = dlg_mk(h, L"STATIC", L"处理模式", 0, K(24), K(206), K(200), K(20), 0, 0);
        SendMessageW(l3, WM_SETFONT, (WPARAM)d->f_ui, TRUE);
        d->hMode0 = dlg_mk(h, L"BUTTON", L"仅收集图片（png / jpg / webp / bmp / gif）",
                           IDD_MODE0, K(40), K(230), K(400), K(24),
                           BS_AUTORADIOBUTTON | WS_GROUP, 0);
        d->hMode1 = dlg_mk(h, L"BUTTON", L"完整解压（保留所有文件与目录结构）",
                           IDD_MODE1, K(40), K(254), K(400), K(24), BS_AUTORADIOBUTTON, 0);
        SendMessageW(d->hMode0, WM_SETFONT, (WPARAM)d->f_ui, TRUE);
        SendMessageW(d->hMode1, WM_SETFONT, (WPARAM)d->f_ui, TRUE);
        d->hSubdir = dlg_mk(h, L"BUTTON", L"每个源文件单独一个子目录", IDD_SUBDIR,
                            K(40), K(288), K(300), K(24), BS_AUTOCHECKBOX, 0);
        d->hDedup = dlg_mk(h, L"BUTTON", L"内容去重（按 MD5 跳过重复文件）", IDD_DEDUP,
                           K(40), K(316), K(360), K(24), BS_AUTOCHECKBOX, 0);
        SendMessageW(d->hSubdir, WM_SETFONT, (WPARAM)d->f_ui, TRUE);
        SendMessageW(d->hDedup, WM_SETFONT, (WPARAM)d->f_ui, TRUE);
        HWND l4 = dlg_mk(h, L"STATIC", L"收集格式（仅“仅收集图片”模式生效）",
                         0, K(24), K(348), K(420), K(20), 0, 0);
        SendMessageW(l4, WM_SETFONT, (WPARAM)d->f_ui, TRUE);
        static const wchar_t *fmt_names[6] = {
            L"PNG", L"JPG", L"JPEG", L"WEBP", L"BMP", L"GIF"
        };
        int fmt_ids[6] = { IDD_FMT_PNG, IDD_FMT_JPG, IDD_FMT_JPEG,
                           IDD_FMT_WEBP, IDD_FMT_BMP, IDD_FMT_GIF };
        for (int i = 0; i < 6; i++) {
            d->hFmt[i] = dlg_mk(h, L"BUTTON", fmt_names[i], fmt_ids[i],
                                K(40 + i * 78), K(374), K(70), K(24),
                                BS_AUTOCHECKBOX, 0);
            SendMessageW(d->hFmt[i], WM_SETFONT, (WPARAM)d->f_ui, TRUE);
        }
        d->hKeep = dlg_mk(h, L"BUTTON", L"保留临时文件（便于排查）", IDD_KEEPTEMP,
                          K(40), K(410), K(320), K(24), BS_AUTOCHECKBOX, 0);
        d->hAuto = dlg_mk(h, L"BUTTON", L"完成后自动打开输出文件夹", IDD_AUTOOPEN,
                          K(40), K(438), K(320), K(24), BS_AUTOCHECKBOX, 0);
        d->hRemember = dlg_mk(h, L"BUTTON", L"记住以上设置", IDD_REMEMBER,
                              K(40), K(466), K(320), K(24), BS_AUTOCHECKBOX, 0);
        SendMessageW(d->hKeep, WM_SETFONT, (WPARAM)d->f_ui, TRUE);
        SendMessageW(d->hAuto, WM_SETFONT, (WPARAM)d->f_ui, TRUE);
        SendMessageW(d->hRemember, WM_SETFONT, (WPARAM)d->f_ui, TRUE);
        HWND cancel = dlg_mk(h, L"BUTTON", L"取消", IDD_CANCEL,
                             w - K(200), K(524), K(88), K(36), BS_PUSHBUTTON, 0);
        HWND save = dlg_mk(h, L"BUTTON", L"保存", IDD_SAVE,
                           w - K(104), K(524), K(88), K(36), BS_PUSHBUTTON, 0);
        SendMessageW(cancel, WM_SETFONT, (WPARAM)d->f_ui, TRUE);
        SendMessageW(save, WM_SETFONT, (WPARAM)d->f_ui, TRUE);
        /* 初始值 */
        CheckDlgButton(h, IDD_ENGINE0, d->md->cfg.engine == 0 ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(h, IDD_ENGINE1, d->md->cfg.engine == 1 ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(h, IDD_MODE0, d->md->cfg.mode == 0 ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(h, IDD_MODE1, d->md->cfg.mode == 1 ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(h, IDD_SUBDIR, d->md->cfg.subdir ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(h, IDD_DEDUP, d->md->cfg.dedup ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(h, IDD_KEEPTEMP, d->md->cfg.keep_temp ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(h, IDD_AUTOOPEN, d->md->cfg.auto_open ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(h, IDD_REMEMBER, d->md->cfg.remember ? BST_CHECKED : BST_UNCHECKED);
        for (int i = 0; i < 6; i++)
            CheckDlgButton(h, fmt_ids[i], d->md->cfg.exts[i] ? BST_CHECKED : BST_UNCHECKED);
#undef K
        return 0;
    }
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORBTN: {
        HDC dc = (HDC)wp;
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, RGB(0xE6, 0xEA, 0xF2));
        return (LRESULT)d->bg_brush;
    }
    case WM_CTLCOLOREDIT: {
        HDC dc = (HDC)wp;
        SetTextColor(dc, RGB(0xE6, 0xEA, 0xF2));
        SetBkColor(dc, RGB(0x14, 0x1A, 0x2C));
        return (LRESULT)d->bg_brush;
    }
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT:
        paint_dlg_bg(h);
        return 0;
    case WM_COMMAND: {
        int id = LOWORD(wp);
        if (id == IDD_BROWSE7Z) {
            wchar_t buf[MAX_PATH_BUF];
            buf[0] = 0;
            OPENFILENAMEW ofn;
            memset(&ofn, 0, sizeof(ofn));
            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner = h;
            ofn.lpstrFilter = L"可执行文件 (*.exe)\0*.exe\0所有文件 (*.*)\0*.*\0\0";
            ofn.lpstrFile = buf;
            ofn.nMaxFile = MAX_PATH_BUF;
            ofn.Flags = OFN_FILEMUSTEXIST | OFN_EXPLORER;
            if (GetOpenFileNameW(&ofn)) SetWindowTextW(d->hSevenz, buf);
            return 0;
        }
        if (id == IDD_SAVE) {
            GetWindowTextW(d->hSevenz, d->md->cfg.sevenz, MAX_PATH_BUF);
            d->md->cfg.engine =
                IsDlgButtonChecked(h, IDD_ENGINE1) == BST_CHECKED ? 1 : 0;
            d->md->cfg.mode =
                IsDlgButtonChecked(h, IDD_MODE1) == BST_CHECKED ? 1 : 0;
            d->md->cfg.subdir = IsDlgButtonChecked(h, IDD_SUBDIR) == BST_CHECKED;
            d->md->cfg.dedup = IsDlgButtonChecked(h, IDD_DEDUP) == BST_CHECKED;
            d->md->cfg.keep_temp = IsDlgButtonChecked(h, IDD_KEEPTEMP) == BST_CHECKED;
            d->md->cfg.auto_open = IsDlgButtonChecked(h, IDD_AUTOOPEN) == BST_CHECKED;
            d->md->cfg.remember = IsDlgButtonChecked(h, IDD_REMEMBER) == BST_CHECKED;
            int fmt_ids[6] = { IDD_FMT_PNG, IDD_FMT_JPG, IDD_FMT_JPEG,
                               IDD_FMT_WEBP, IDD_FMT_BMP, IDD_FMT_GIF };
            for (int i = 0; i < 6; i++)
                d->md->cfg.exts[i] =
                    IsDlgButtonChecked(h, fmt_ids[i]) == BST_CHECKED ? 1 : 0;
            md_save_settings(d->md);
            wchar_t *msg = xwcsdup(L"设置已保存");
            PostMessageW(d->md->hwnd, WM_APP_LOG, LOG_OK, (LPARAM)msg);
            d->done = true;
            DestroyWindow(h);
            return 0;
        }
        if (id == IDD_CANCEL) {
            d->done = true;
            DestroyWindow(h);
            return 0;
        }
        break;
    }
    case WM_CLOSE:
        d->done = true;
        DestroyWindow(h);
        return 0;
    case WM_DESTROY:
        DeleteObject(d->f_ui);
        DeleteObject(d->f_bold);
        DeleteObject(d->bg_brush);
        return 0;
    case WM_NCDESTROY:
        free(d);
        return 0;
    }
    return DefWindowProcW(h, m, wp, lp);
}

void dialog_open_settings(HWND owner, MainData *md) {
    SettingsData *d = (SettingsData *)xcalloc(1, sizeof(SettingsData));
    d->md = md;
    d->k = dlg_scale(owner);
    HWND dlg = center_dialog(owner, L"ZipSettingsDlg", L"设置",
                             (int)(560 * d->k), (int)(580 * d->k), d);
    if (!dlg) { free(d); return; }
    run_modal(owner, dlg, &d->done);
}

/* ================= 关于 ================= */

typedef struct {
    float k;
    HFONT f_ui, f_bold, f_title;
    HBRUSH bg_brush;
    bool done;
} AboutData;

static LRESULT CALLBACK AboutProc(HWND h, UINT m, WPARAM wp, LPARAM lp) {
    AboutData *d = (AboutData *)GetWindowLongPtrW(h, GWLP_USERDATA);
    if (m == WM_NCCREATE) {
        d = (AboutData *)((CREATESTRUCTW *)lp)->lpCreateParams;
        SetWindowLongPtrW(h, GWLP_USERDATA, (LONG_PTR)d);
        return TRUE;
    }
    if (!d) return DefWindowProcW(h, m, wp, lp);
    switch (m) {
    case WM_CREATE: {
        d->f_ui = dlg_font(10, 0);
        d->f_bold = dlg_font(13, 1);
        d->f_title = dlg_font(16, 1);
        d->bg_brush = CreateSolidBrush(RGB(0x10, 0x14, 0x1E));
        float k = d->k;
#define K(v) ((int)((v) * k))
        HWND title = dlg_mk(h, L"STATIC", L"ZIPedIT Crystal (C)", 0,
                            K(24), K(18), K(300), K(30), 0, 0);
        SendMessageW(title, WM_SETFONT, (WPARAM)d->f_title, TRUE);
        const wchar_t *lines[] = {
            L"套娃压缩包解压 + 图片收集工具",
            L"",
            L"• 支持 7z / zip / rar / tar 主包与子包递归解压",
            L"• 一级 / 多级密码批量尝试、密码列表导入",
            L"• 仅收集图片或完整解压两种模式",
            L"• MD5 内容去重、按来源分子目录、格式筛选",
            L"• 多文件批量、拖拽文件或文件夹、任务历史",
            L"• 图片图库预览、日志导出",
            L"",
            L"用 C 语言重写 · 原生 Win32 界面",
        };
        int y = K(64);
        for (size_t i = 0; i < sizeof(lines) / sizeof(lines[0]); i++) {
            HWND lbl = dlg_mk(h, L"STATIC", lines[i], 0,
                              K(28), y, K(420), K(24), 0, 0);
            SendMessageW(lbl, WM_SETFONT,
                         (WPARAM)(i == 1 ? d->f_ui : d->f_ui), TRUE);
            y += K(24);
        }
        HWND close = dlg_mk(h, L"BUTTON", L"关闭", IDD_ABOUT_CLOSE,
                            K(360), K(320), K(88), K(36), BS_PUSHBUTTON, 0);
        SendMessageW(close, WM_SETFONT, (WPARAM)d->f_ui, TRUE);
#undef K
        return 0;
    }
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORBTN: {
        HDC dc = (HDC)wp;
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, RGB(0xE6, 0xEA, 0xF2));
        return (LRESULT)d->bg_brush;
    }
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT:
        paint_dlg_bg(h);
        return 0;
    case WM_COMMAND:
        if (LOWORD(wp) == IDD_ABOUT_CLOSE) {
            d->done = true;
            DestroyWindow(h);
            return 0;
        }
        break;
    case WM_CLOSE:
        d->done = true;
        DestroyWindow(h);
        return 0;
    case WM_DESTROY:
        DeleteObject(d->f_ui);
        DeleteObject(d->f_bold);
        DeleteObject(d->f_title);
        DeleteObject(d->bg_brush);
        return 0;
    case WM_NCDESTROY:
        free(d);
        return 0;
    }
    return DefWindowProcW(h, m, wp, lp);
}

void dialog_open_about(HWND owner) {
    AboutData *d = (AboutData *)xcalloc(1, sizeof(AboutData));
    d->k = dlg_scale(owner);
    HWND dlg = center_dialog(owner, L"ZipAboutDlg", L"关于 ZIPedIT",
                             (int)(470 * d->k), (int)(380 * d->k), d);
    if (!dlg) { free(d); return; }
    run_modal(owner, dlg, &d->done);
}

/* ================= 历史 ================= */

typedef struct {
    MainData *md;
    float k;
    HFONT f_ui, f_bold;
    HBRUSH bg_brush;
    HWND hList, hCount;
    HistoryRec **recs;
    int n;
    bool done;
} HistData;

static void hist_fill_list(HistData *d, HWND h) {
    SendMessageW(d->hList, LB_RESETCONTENT, 0, 0);
    for (int i = 0; i < d->n; i++) {
        HistoryRec *r = d->recs[i];
        wchar_t line[1024];
        swprintf(line, 1024, L"%ls  ·  %d %ls  ·  %ls  ·  %.1fs  ·  %ls",
                 r->ts, r->images, r->mode == 0 ? L"图" : L"文件",
                 path_basename(r->out[0] ? r->out : L"未知目录"),
                 r->elapsed, r->success ? L"✓" : L"✗");
        SendMessageW(d->hList, LB_ADDSTRING, 0, (LPARAM)line);
    }
    wchar_t cnt[128];
    swprintf(cnt, 128, L"最近 %d 次 · 双击恢复", d->n);
    SetWindowTextW(d->hCount, cnt);
    (void)h;
}

static LRESULT CALLBACK HistProc(HWND h, UINT m, WPARAM wp, LPARAM lp) {
    HistData *d = (HistData *)GetWindowLongPtrW(h, GWLP_USERDATA);
    if (m == WM_NCCREATE) {
        d = (HistData *)((CREATESTRUCTW *)lp)->lpCreateParams;
        SetWindowLongPtrW(h, GWLP_USERDATA, (LONG_PTR)d);
        return TRUE;
    }
    if (!d) return DefWindowProcW(h, m, wp, lp);
    switch (m) {
    case WM_CREATE: {
        d->f_ui = dlg_font(10, 0);
        d->f_bold = dlg_font(14, 1);
        d->bg_brush = CreateSolidBrush(RGB(0x10, 0x14, 0x1E));
        float k = d->k;
#define K(v) ((int)((v) * k))
        HWND title = dlg_mk(h, L"STATIC", L"🕘 任务历史", 0,
                            K(20), K(14), K(220), K(28), 0, 0);
        SendMessageW(title, WM_SETFONT, (WPARAM)d->f_bold, TRUE);
        d->hCount = dlg_mk(h, L"STATIC", L"", 0,
                           K(420), K(18), K(200), K(20), SS_RIGHT, 0);
        SendMessageW(d->hCount, WM_SETFONT, (WPARAM)d->f_ui, TRUE);
        d->hList = dlg_mk(h, L"LISTBOX", L"", IDD_HIST_LIST,
                          K(20), K(56), K(580), K(320),
                          WS_TABSTOP | LBS_NOTIFY | WS_VSCROLL | LBS_HASSTRINGS,
                          WS_EX_CLIENTEDGE);
        SendMessageW(d->hList, WM_SETFONT, (WPARAM)d->f_ui, TRUE);
        HWND clear = dlg_mk(h, L"BUTTON", L"清空历史", IDD_HIST_CLEAR,
                            K(420), K(392), K(88), K(36), BS_PUSHBUTTON, 0);
        HWND close = dlg_mk(h, L"BUTTON", L"关闭", IDD_HIST_CLOSE,
                            K(516), K(392), K(88), K(36), BS_PUSHBUTTON, 0);
        SendMessageW(clear, WM_SETFONT, (WPARAM)d->f_ui, TRUE);
        SendMessageW(close, WM_SETFONT, (WPARAM)d->f_ui, TRUE);
        history_load(&d->recs, &d->n);
        hist_fill_list(d, h);
#undef K
        return 0;
    }
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORBTN: {
        HDC dc = (HDC)wp;
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, RGB(0xE6, 0xEA, 0xF2));
        return (LRESULT)d->bg_brush;
    }
    case WM_CTLCOLORLISTBOX: {
        HDC dc = (HDC)wp;
        SetTextColor(dc, RGB(0xB6, 0xC2, 0xD9));
        SetBkColor(dc, RGB(0x14, 0x1A, 0x2C));
        return (LRESULT)d->bg_brush;
    }
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT:
        paint_dlg_bg(h);
        return 0;
    case WM_COMMAND: {
        int id = LOWORD(wp);
        if (id == IDD_HIST_CLEAR) {
            history_clear();
            history_free(d->recs, d->n);
            d->recs = NULL;
            d->n = 0;
            hist_fill_list(d, h);
            wchar_t *msg = xwcsdup(L"历史记录已清空");
            PostMessageW(d->md->hwnd, WM_APP_LOG, LOG_OK, (LPARAM)msg);
            return 0;
        }
        if (id == IDD_HIST_CLOSE) {
            d->done = true;
            DestroyWindow(h);
            return 0;
        }
        if (HIWORD(wp) == LBN_DBLCLK) {
            int sel = (int)SendMessageW(d->hList, LB_GETCURSEL, 0, 0);
            if (sel >= 0 && sel < d->n) {
                HistoryRec *r = d->recs[sel];
                md_set_files(d->md, (const wchar_t **)r->files, r->nfiles);
                if (r->out[0]) md_set_out_dir(d->md, r->out);
                d->md->cfg.mode = r->mode;
                wchar_t *msg = xwcsdup(L"↩️ 已恢复历史任务");
                PostMessageW(d->md->hwnd, WM_APP_LOG, LOG_OK, (LPARAM)msg);
                d->done = true;
                DestroyWindow(h);
            }
            return 0;
        }
        break;
    }
    case WM_CLOSE:
        d->done = true;
        DestroyWindow(h);
        return 0;
    case WM_DESTROY:
        history_free(d->recs, d->n);
        DeleteObject(d->f_ui);
        DeleteObject(d->f_bold);
        DeleteObject(d->bg_brush);
        return 0;
    case WM_NCDESTROY:
        free(d);
        return 0;
    }
    return DefWindowProcW(h, m, wp, lp);
}

void dialog_open_history(HWND owner, MainData *md) {
    HistData *d = (HistData *)xcalloc(1, sizeof(HistData));
    d->md = md;
    d->k = dlg_scale(owner);
    HWND dlg = center_dialog(owner, L"ZipHistDlg", L"任务历史",
                             (int)(620 * d->k), (int)(450 * d->k), d);
    if (!dlg) { free(d); return; }
    run_modal(owner, dlg, &d->done);
}

/* ================= 图库 ================= */

typedef struct {
    float k;
    HFONT f_ui, f_bold;
    HBRUSH bg_brush;
    HWND hList, hCount;
    wchar_t **paths;
    int n;
    HIMAGELIST il;
    IWICImagingFactory *fac;
    bool done;
} GalData;

static HBITMAP make_thumb(IWICImagingFactory *fac, const wchar_t *path, int box,
                          int *ow, int *oh) {
    IWICBitmapDecoder *dec = NULL;
    HRESULT hr = fac->lpVtbl->CreateDecoderFromFilename(
        fac, path, NULL, GENERIC_READ, WICDecodeMetadataCacheOnDemand, &dec);
    if (FAILED(hr)) return NULL;
    IWICBitmapFrameDecode *frame = NULL;
    hr = dec->lpVtbl->GetFrame(dec, 0, &frame);
    dec->lpVtbl->Release(dec);
    if (FAILED(hr)) return NULL;
    UINT w = 0, h = 0;
    frame->lpVtbl->GetSize(frame, &w, &h);
    if (!w || !h) { frame->lpVtbl->Release(frame); return NULL; }
    float scale = (float)box / (w > h ? w : h);
    if (scale > 1.0f) scale = 1.0f;
    UINT sw = (UINT)(w * scale);
    UINT sh = (UINT)(h * scale);
    if (sw < 1) sw = 1;
    if (sh < 1) sh = 1;
    IWICBitmapScaler *sc = NULL;
    fac->lpVtbl->CreateBitmapScaler(fac, &sc);
    if (!sc) { frame->lpVtbl->Release(frame); return NULL; }
    hr = sc->lpVtbl->Initialize(sc, (IWICBitmapSource *)frame, sw, sh,
                                WICBitmapInterpolationModeCubic);
    frame->lpVtbl->Release(frame);
    if (FAILED(hr)) { sc->lpVtbl->Release(sc); return NULL; }
    BITMAPINFO bmi;
    memset(&bmi, 0, sizeof(bmi));
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = (LONG)sw;
    bmi.bmiHeader.biHeight = -(LONG)sh;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    void *px = NULL;
    HBITMAP bmp = CreateDIBSection(NULL, &bmi, DIB_RGB_COLORS, &px, NULL, 0);
    if (!bmp) { sc->lpVtbl->Release(sc); return NULL; }
    WICRect rc = { 0, 0, (INT)sw, (INT)sh };
    hr = sc->lpVtbl->CopyPixels(sc, &rc, sw * 4, sw * sh * 4, (BYTE *)px);
    sc->lpVtbl->Release(sc);
    if (FAILED(hr)) { DeleteObject(bmp); return NULL; }
    if (ow) *ow = (int)sw;
    if (oh) *oh = (int)sh;
    return bmp;
}

static void gallery_scan(const wchar_t *dir, int depth, GalData *d) {
    if (depth > 64) return;
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
            gallery_scan(full, depth + 1, d);
        } else {
            static const wchar_t *exts[] = {
                L".png", L".jpg", L".jpeg", L".webp", L".bmp", L".gif"
            };
            bool ok = false;
            for (int i = 0; i < 6; i++)
                if (ends_with_ci(fd.cFileName, exts[i])) { ok = true; break; }
            if (ok) {
                d->paths = (wchar_t **)xrealloc(d->paths,
                                                (d->n + 1) * sizeof(wchar_t *));
                d->paths[d->n++] = xwcsdup(full);
            }
        }
    } while (FindNextFileW(h, &fd));
    FindClose(h);
}

static LRESULT CALLBACK GalleryProc(HWND h, UINT m, WPARAM wp, LPARAM lp) {
    GalData *d = (GalData *)GetWindowLongPtrW(h, GWLP_USERDATA);
    if (m == WM_NCCREATE) {
        d = (GalData *)((CREATESTRUCTW *)lp)->lpCreateParams;
        SetWindowLongPtrW(h, GWLP_USERDATA, (LONG_PTR)d);
        return TRUE;
    }
    if (!d) return DefWindowProcW(h, m, wp, lp);
    switch (m) {
    case WM_CREATE: {
        d->f_ui = dlg_font(10, 0);
        d->f_bold = dlg_font(14, 1);
        d->bg_brush = CreateSolidBrush(RGB(0x10, 0x14, 0x1E));
        CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
        CoCreateInstance(&CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER,
                         &IID_IWICImagingFactory, (void **)&d->fac);
        float k = d->k;
#define K(v) ((int)((v) * k))
        HWND title = dlg_mk(h, L"STATIC", L"🖼️ 收集结果预览", 0,
                            K(20), K(14), K(260), K(28), 0, 0);
        SendMessageW(title, WM_SETFONT, (WPARAM)d->f_bold, TRUE);
        d->hCount = dlg_mk(h, L"STATIC", L"", 0,
                           K(560), K(18), K(260), K(20), SS_RIGHT, 0);
        SendMessageW(d->hCount, WM_SETFONT, (WPARAM)d->f_ui, TRUE);
        d->il = ImageList_Create(K(96), K(96), ILC_COLOR32, 0, 32);
        d->hList = CreateWindowExW(WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"",
                                   WS_CHILD | WS_VISIBLE | LVS_ICON |
                                   LVS_AUTOARRANGE | LVS_SHOWSELALWAYS,
                                   K(20), K(56), K(800), K(480),
                                   h, (HMENU)(intptr_t)IDD_GAL_LIST,
                                   GetModuleHandleW(NULL), NULL);
        SendMessageW(d->hList, WM_SETFONT, (WPARAM)d->f_ui, TRUE);
        ListView_SetExtendedListViewStyle(d->hList, LVS_EX_DOUBLEBUFFER);
        ListView_SetBkColor(d->hList, RGB(0x10, 0x14, 0x1E));
        ListView_SetTextColor(d->hList, RGB(0xE6, 0xEA, 0xF2));
        ListView_SetTextBkColor(d->hList, RGB(0x10, 0x14, 0x1E));
        if (d->il) ListView_SetImageList(d->hList, d->il, LVSIL_NORMAL);
        HWND close = dlg_mk(h, L"BUTTON", L"关闭", IDD_GAL_CLOSE,
                            K(732), K(548), K(88), K(36), BS_PUSHBUTTON, 0);
        SendMessageW(close, WM_SETFONT, (WPARAM)d->f_ui, TRUE);
        (void)title;
#undef K
        return 0;
    }
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORBTN: {
        HDC dc = (HDC)wp;
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, RGB(0xE6, 0xEA, 0xF2));
        return (LRESULT)d->bg_brush;
    }
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT:
        paint_dlg_bg(h);
        return 0;
    case WM_COMMAND:
        if (LOWORD(wp) == IDD_GAL_CLOSE) {
            d->done = true;
            DestroyWindow(h);
            return 0;
        }
        break;
    case WM_NOTIFY: {
        NMHDR *nm = (NMHDR *)lp;
        if (nm->idFrom == IDD_GAL_LIST && nm->code == NM_DBLCLK) {
            int sel = ListView_GetNextItem(d->hList, -1, LVNI_SELECTED);
            if (sel >= 0) {
                LVITEMW it;
                memset(&it, 0, sizeof(it));
                it.mask = LVIF_PARAM;
                it.iItem = sel;
                if (ListView_GetItem(d->hList, &it) && it.lParam >= 0 &&
                    it.lParam < d->n) {
                    ShellExecuteW(NULL, L"open", d->paths[it.lParam], NULL, NULL,
                                  SW_SHOWNORMAL);
                }
            }
            return 0;
        }
        break;
    }
    case WM_CLOSE:
        d->done = true;
        DestroyWindow(h);
        return 0;
    case WM_DESTROY:
        if (d->il) ImageList_Destroy(d->il);
        for (int i = 0; i < d->n; i++) free(d->paths[i]);
        free(d->paths);
        if (d->fac) d->fac->lpVtbl->Release(d->fac);
        CoUninitialize();
        DeleteObject(d->f_ui);
        DeleteObject(d->f_bold);
        DeleteObject(d->bg_brush);
        return 0;
    case WM_NCDESTROY:
        free(d);
        return 0;
    }
    return DefWindowProcW(h, m, wp, lp);
}

void dialog_open_gallery(HWND owner, const wchar_t *folder) {
    GalData *d = (GalData *)xcalloc(1, sizeof(GalData));
    d->k = dlg_scale(owner);
    HWND dlg = center_dialog(owner, L"ZipGalDlg", L"图片图库",
                             (int)(840 * d->k), (int)(600 * d->k), d);
    if (!dlg) { free(d); return; }
    /* 扫描图片 */
    gallery_scan(folder, 0, d);
    int shown = 0;
    int total = d->n;
    for (int i = 0; i < d->n && i < 500 && d->fac && d->il; i++) {
        int ow = 0, oh = 0;
        HBITMAP bmp = make_thumb(d->fac, d->paths[i], (int)(96 * d->k), &ow, &oh);
        if (!bmp) continue;
        int idx = ImageList_Add(d->il, bmp, NULL);
        DeleteObject(bmp);
        if (idx < 0) continue;
        LVITEMW it;
        memset(&it, 0, sizeof(it));
        it.mask = LVIF_TEXT | LVIF_IMAGE | LVIF_PARAM;
        it.iItem = shown;
        it.iImage = idx;
        it.lParam = i;
        wchar_t name[MAX_PATH_BUF];
        wcsncpy(name, path_basename(d->paths[i]), MAX_PATH_BUF - 1);
        name[MAX_PATH_BUF - 1] = 0;
        it.pszText = name;
        ListView_InsertItem(d->hList, &it);
        shown++;
    }
    wchar_t cnt[256];
    if (total > 500)
        swprintf(cnt, 256, L"共 %d 张 · 预览前 %d 张 · 双击打开原图", total, shown);
    else
        swprintf(cnt, 256, L"共 %d 张 · 双击打开原图", total);
    SetWindowTextW(d->hCount, cnt);
    run_modal(owner, dlg, &d->done);
}

bool dialogs_register_classes(HINSTANCE hInst) {
    struct { const wchar_t *name; WNDPROC proc; } cls[] = {
        { L"ZipSettingsDlg", SettingsProc },
        { L"ZipAboutDlg", AboutProc },
        { L"ZipHistDlg", HistProc },
        { L"ZipGalDlg", GalleryProc }
    };
    for (size_t i = 0; i < sizeof(cls) / sizeof(cls[0]); i++) {
        WNDCLASSEXW wc;
        memset(&wc, 0, sizeof(wc));
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = cls[i].proc;
        wc.hInstance = hInst;
        wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
        wc.hbrBackground = NULL;
        wc.lpszClassName = cls[i].name;
        if (!RegisterClassExW(&wc)) return false;
    }
    return true;
}
