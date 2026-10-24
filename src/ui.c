#include "app.h"
#include <richedit.h>
#include <stdarg.h>

#ifndef RICHEDIT_CLASS_W
#define RICHEDIT_CLASS_W L"RICHEDIT50W"
#endif

static HINSTANCE g_hInst = NULL;
static const wchar_t *VERSION_CYCLE[] = {
    L"v4.2 · Crystal (C)", L"v4.3 · 混沌水晶", L"v9.9 · 加强版（并没有）",
    L"v∞ · 无限循环", L"v0.0.1 · 最初的模样"
};
static const wchar_t *FUN_LINES[] = {
    L"🎊 图片们重见天日！", L"⚡ 这速度，闪电都追不上。",
    L"🍀 今日幸运：全部解压成功。", L"🎉 恭喜，你成功解救了这些图片。",
    L"💎 水晶之力加持，任务圆满完成。", L"🚀 解压完成，收工！"
};

#define K(v) ((int)((v) * md->k))

static float get_scale(void) {
    HDC dc = GetDC(NULL);
    int dpi = GetDeviceCaps(dc, LOGPIXELSY);
    ReleaseDC(NULL, dc);
    float s = dpi / 96.0f;
    return s < 1.0f ? 1.0f : s;
}

static HFONT mkfont(int size, int bold) {
    HDC dc = GetDC(NULL);
    int dpi = GetDeviceCaps(dc, LOGPIXELSY);
    ReleaseDC(NULL, dc);
    return CreateFontW(-MulDiv(size, dpi, 72), 0, 0, 0,
                       bold ? FW_SEMIBOLD : FW_NORMAL, 0, 0, 0,
                       DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                       CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                       DEFAULT_PITCH, L"Microsoft YaHei UI");
}

static HWND mk(HWND parent, const wchar_t *cls, const wchar_t *text, int id,
               int x, int y, int w, int h, DWORD style, DWORD ex) {
    return CreateWindowExW(ex, cls, text, WS_CHILD | WS_VISIBLE | style,
                           x, y, w, h, parent, (HMENU)(intptr_t)id,
                           g_hInst, NULL);
}

static void set_font(HWND h, HFONT f) {
    if (h) SendMessageW(h, WM_SETFONT, (WPARAM)f, TRUE);
}

/* ================ ZipCard ================ */

static LRESULT CALLBACK CardProc(HWND h, UINT m, WPARAM wp, LPARAM lp) {
    switch (m) {
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(h, &ps);
        RECT rc;
        GetClientRect(h, &rc);
        int w = rc.right, hh = rc.bottom;
        if (w > 0 && hh > 0) {
            Dib *d = dib_create(w, hh);
            if (d) {
                dib_fill_round_gradient(d, 12,
                    argb_color(255, 16, 20, 30),
                    argb_color(255, 18, 24, 38),
                    0xFF0D1119);
                dib_premultiply(d);
                dib_blend(d, dc, 0, 0, w, hh);
                dib_free(d);
            }
        }
        EndPaint(h, &ps);
        return 0;
    }
    case WM_SIZE:
        InvalidateRect(h, NULL, FALSE);
        return 0;
    }
    return DefWindowProcW(h, m, wp, lp);
}

/* ================ ZipNeoButton ================ */

typedef struct {
    int kind;       /* 0 primary 1 ghost 2 danger */
    bool hover, press;
} NBData;

static LRESULT CALLBACK NeoButtonProc(HWND h, UINT m, WPARAM wp, LPARAM lp) {
    NBData *d = (NBData *)GetWindowLongPtrW(h, GWLP_USERDATA);
    if (m == WM_NCCREATE) {
        d = (NBData *)xcalloc(1, sizeof(NBData));
        SetWindowLongPtrW(h, GWLP_USERDATA, (LONG_PTR)d);
        return TRUE;
    }
    if (!d) return DefWindowProcW(h, m, wp, lp);
    switch (m) {
    case WM_NCDESTROY:
        free(d);
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_ENABLE:
        InvalidateRect(h, NULL, TRUE);
        return 0;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(h, &ps);
        RECT rc;
        GetClientRect(h, &rc);
        int w = rc.right, hh = rc.bottom;
        bool enabled = IsWindowEnabled(h) != 0;
        uint32_t c0, c1;
        if (d->kind == 0) {
            c0 = d->hover ? 0xFF4CC8FA : C_ACCENT_A;
            c1 = d->hover ? 0xFF6E6BF5 : C_ACCENT_B;
            if (d->press) { c0 = lerp_color(c0, C_BG_BOTTOM, 0.22f); c1 = lerp_color(c1, C_BG_BOTTOM, 0.22f); }
            if (!enabled) { c0 = 0xFF2C3A52; c1 = 0xFF26344B; }
        } else if (d->kind == 2) {
            c0 = C_DANGER;
            c1 = 0xFFB91C4B;
            if (d->hover) { c0 = 0xFFFB8B8B; c1 = 0xFFD34A6E; }
            if (d->press) { c0 = lerp_color(c0, C_BG_BOTTOM, 0.2f); c1 = lerp_color(c1, C_BG_BOTTOM, 0.2f); }
            if (!enabled) { c0 = 0xFF2C3A52; c1 = 0xFF26344B; }
        } else {
            c0 = 0xFF1A2233;
            c1 = 0xFF161C2E;
            if (d->press) { c0 = 0xFF121826; c1 = 0xFF121826; }
            if (!enabled) { c0 = 0xFF161C2E; c1 = 0xFF131928; }
        }
        Dib *dib = dib_create(w, hh);
        if (dib) {
            dib_fill_round_gradient(dib, hh / 2, c0, c1, 0xFF0D1119);
            dib_premultiply(dib);
            dib_blend(dib, dc, 0, 0, w, hh);
            dib_free(dib);
        }
        if (d->kind == 1 && enabled) {
            HPEN pen = CreatePen(PS_SOLID, 1,
                d->hover ? RGB(0x38, 0xBD, 0xF8) : RGB(0x2A, 0x36, 0x4D));
            HGDIOBJ old = SelectObject(dc, pen);
            HBRUSH br = (HBRUSH)GetStockObject(NULL_BRUSH);
            SelectObject(dc, br);
            RoundRect(dc, 0, 0, w - 1, hh - 1, hh, hh);
            SelectObject(dc, old);
            DeleteObject(pen);
        }
        wchar_t text[256];
        GetWindowTextW(h, text, 256);
        SetBkMode(dc, TRANSPARENT);
        if (d->kind == 0 || d->kind == 2) {
            SetTextColor(dc, enabled ? RGB(0xFF, 0xFF, 0xFF) : RGB(0x8A, 0x94, 0xA8));
        } else {
            SetTextColor(dc, enabled ? (d->hover ? RGB(0xE6, 0xEA, 0xF2) : RGB(0xB6, 0xC2, 0xD9))
                                     : RGB(0x5E, 0x66, 0x76));
        }
        DrawTextW(dc, text, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        EndPaint(h, &ps);
        return 0;
    }
    case WM_MOUSEMOVE:
        if (!d->hover) {
            TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, h, 0 };
            TrackMouseEvent(&tme);
            d->hover = true;
            InvalidateRect(h, NULL, FALSE);
        }
        return 0;
    case WM_MOUSELEAVE:
        d->hover = false;
        InvalidateRect(h, NULL, FALSE);
        return 0;
    case WM_LBUTTONDOWN:
        SetCapture(h);
        d->press = true;
        InvalidateRect(h, NULL, FALSE);
        return 0;
    case WM_LBUTTONUP:
        if (d->press) {
            d->press = false;
            ReleaseCapture();
            InvalidateRect(h, NULL, FALSE);
            SendMessageW(GetParent(h), WM_COMMAND,
                         MAKEWPARAM((UINT)GetDlgCtrlID(h), BN_CLICKED),
                         (LPARAM)h);
        }
        return 0;
    case WM_SETCURSOR:
        SetCursor(LoadCursorW(NULL, IDC_HAND));
        return TRUE;
    case ZNB_SET_KIND:
        d->kind = (int)wp;
        InvalidateRect(h, NULL, FALSE);
        return 0;
    }
    return DefWindowProcW(h, m, wp, lp);
}

/* ================ ZipDropZone ================ */

typedef struct {
    bool hover;
    wchar_t *sub;
} DZData;

static LRESULT CALLBACK DropZoneProc(HWND h, UINT m, WPARAM wp, LPARAM lp) {
    DZData *d = (DZData *)GetWindowLongPtrW(h, GWLP_USERDATA);
    if (m == WM_NCCREATE) {
        d = (DZData *)xcalloc(1, sizeof(DZData));
        d->sub = xwcsdup(L"");
        SetWindowLongPtrW(h, GWLP_USERDATA, (LONG_PTR)d);
        return TRUE;
    }
    if (!d) return DefWindowProcW(h, m, wp, lp);
    switch (m) {
    case WM_NCDESTROY:
        free(d->sub);
        free(d);
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(h, &ps);
        RECT rc;
        GetClientRect(h, &rc);
        int w = rc.right, hh = rc.bottom;
        Dib *dib = dib_create(w, hh);
        if (dib) {
            dib_fill_round_gradient(dib, 12,
                0xFF0F1522,
                0xFF0F1522,
                0xFF0D1119);
            dib_premultiply(dib);
            dib_blend(dib, dc, 0, 0, w, hh);
            dib_free(dib);
        }
        HPEN pen = CreatePen(d->hover ? PS_DASH : PS_SOLID, 1,
            d->hover ? RGB(0x38, 0xBD, 0xF8) : RGB(0x2A, 0x36, 0x4D));
        HGDIOBJ old = SelectObject(dc, pen);
        HBRUSH br = (HBRUSH)GetStockObject(NULL_BRUSH);
        SelectObject(dc, br);
        RoundRect(dc, 0, 0, w - 1, hh - 1, 24, 24);
        SelectObject(dc, old);
        DeleteObject(pen);
        wchar_t hint[512];
        GetWindowTextW(h, hint, 512);
        SetBkMode(dc, TRANSPARENT);
        RECT tr = rc;
        tr.top = hh / 2 - 24;
        tr.bottom = tr.top + 24;
        SetTextColor(dc, RGB(0xE6, 0xEA, 0xF2));
        DrawTextW(dc, hint, -1, &tr, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        tr.top += 24;
        tr.bottom = tr.top + 20;
        SetTextColor(dc, RGB(0x5E, 0x66, 0x76));
        DrawTextW(dc, d->sub, -1, &tr, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        EndPaint(h, &ps);
        return 0;
    }
    case WM_MOUSEMOVE:
        if (!d->hover) {
            TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, h, 0 };
            TrackMouseEvent(&tme);
            d->hover = true;
            InvalidateRect(h, NULL, FALSE);
        }
        return 0;
    case WM_MOUSELEAVE:
        d->hover = false;
        InvalidateRect(h, NULL, FALSE);
        return 0;
    case WM_LBUTTONUP:
        SendMessageW(GetParent(h), WM_COMMAND,
                     MAKEWPARAM((UINT)GetDlgCtrlID(h), BN_CLICKED),
                     (LPARAM)h);
        return 0;
    case WM_SETCURSOR:
        SetCursor(LoadCursorW(NULL, IDC_HAND));
        return TRUE;
    case ZDZ_SET_SUB:
        free(d->sub);
        d->sub = xwcsdup((const wchar_t *)lp);
        InvalidateRect(h, NULL, FALSE);
        return 0;
    }
    return DefWindowProcW(h, m, wp, lp);
}

/* ================ ZipRing ================ */

typedef struct {
    int value;       /* 0..1000 */
    bool indet;
    float phase;
} RingData;

static LRESULT CALLBACK RingProc(HWND h, UINT m, WPARAM wp, LPARAM lp) {
    RingData *d = (RingData *)GetWindowLongPtrW(h, GWLP_USERDATA);
    if (m == WM_NCCREATE) {
        d = (RingData *)xcalloc(1, sizeof(RingData));
        SetWindowLongPtrW(h, GWLP_USERDATA, (LONG_PTR)d);
        return TRUE;
    }
    if (!d) return DefWindowProcW(h, m, wp, lp);
    switch (m) {
    case WM_NCDESTROY:
        free(d);
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_TIMER:
        d->phase += 1.0f / 30.0f;
        InvalidateRect(h, NULL, FALSE);
        return 0;
    case ZR_SET_VALUE:
        d->value = (int)wp;
        d->indet = false;
        KillTimer(h, 1);
        InvalidateRect(h, NULL, FALSE);
        return 0;
    case ZR_SET_INDET:
        d->indet = wp != 0;
        if (d->indet) SetTimer(h, 1, 33, NULL);
        else KillTimer(h, 1);
        InvalidateRect(h, NULL, FALSE);
        return 0;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(h, &ps);
        RECT rc;
        GetClientRect(h, &rc);
        int w = rc.right, hh = rc.bottom;
        if (w > 0 && hh > 0) {
            Dib *dib = dib_create(w, hh);
            if (dib) {
                int r = (w < hh ? w : hh) / 2 - 2;
                int pen = r / 5;
                if (pen < 5) pen = 5;
                dib_fill_ring(dib, w / 2, hh / 2, r, pen, d->value,
                              d->indet, d->phase, 0xFF10141E);
                dib_premultiply(dib);
                dib_blend(dib, dc, 0, 0, w, hh);
                dib_free(dib);
            }
        }
        EndPaint(h, &ps);
        return 0;
    }
    }
    return DefWindowProcW(h, m, wp, lp);
}

/* ================ ZipBar ================ */

typedef struct {
    int value;
    bool active;
    float phase;
} BarData;

static LRESULT CALLBACK BarProc(HWND h, UINT m, WPARAM wp, LPARAM lp) {
    BarData *d = (BarData *)GetWindowLongPtrW(h, GWLP_USERDATA);
    if (m == WM_NCCREATE) {
        d = (BarData *)xcalloc(1, sizeof(BarData));
        SetWindowLongPtrW(h, GWLP_USERDATA, (LONG_PTR)d);
        return TRUE;
    }
    if (!d) return DefWindowProcW(h, m, wp, lp);
    switch (m) {
    case WM_NCDESTROY:
        free(d);
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_TIMER:
        d->phase += 1.0f / 30.0f;
        InvalidateRect(h, NULL, FALSE);
        return 0;
    case ZB_SET_VALUE:
        d->value = (int)wp;
        InvalidateRect(h, NULL, FALSE);
        return 0;
    case ZB_SET_ACTIVE:
        d->active = wp != 0;
        if (d->active) SetTimer(h, 1, 33, NULL);
        else KillTimer(h, 1);
        InvalidateRect(h, NULL, FALSE);
        return 0;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(h, &ps);
        RECT rc;
        GetClientRect(h, &rc);
        int w = rc.right, hh = rc.bottom;
        if (w > 0 && hh > 0) {
            Dib *dib = dib_create(w, hh);
            if (dib) {
                dib_fill_bar(dib, hh / 2, C_ACCENT_A, C_ACCENT_B,
                             d->value, d->active, d->phase, 0xFF10141E);
                dib_premultiply(dib);
                dib_blend(dib, dc, 0, 0, w, hh);
                dib_free(dib);
            }
        }
        EndPaint(h, &ps);
        return 0;
    }
    }
    return DefWindowProcW(h, m, wp, lp);
}

/* ================ ZipPill ================ */

typedef struct {
    uint32_t color;
} PillData;

static LRESULT CALLBACK PillProc(HWND h, UINT m, WPARAM wp, LPARAM lp) {
    PillData *d = (PillData *)GetWindowLongPtrW(h, GWLP_USERDATA);
    if (m == WM_NCCREATE) {
        d = (PillData *)xcalloc(1, sizeof(PillData));
        d->color = C_TEXT_SUB;
        SetWindowLongPtrW(h, GWLP_USERDATA, (LONG_PTR)d);
        return TRUE;
    }
    if (!d) return DefWindowProcW(h, m, wp, lp);
    switch (m) {
    case WM_NCDESTROY:
        free(d);
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case ZP_SET_COLOR:
        d->color = (uint32_t)wp;
        InvalidateRect(h, NULL, TRUE);
        return 0;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(h, &ps);
        RECT rc;
        GetClientRect(h, &rc);
        int w = rc.right, hh = rc.bottom;
        HBRUSH bg = CreateSolidBrush(RGB(0x1A, 0x22, 0x33));
        FillRect(dc, &rc, bg);
        DeleteObject(bg);
        HPEN pen = CreatePen(PS_SOLID, 1,
            RGB((d->color >> 16) & 0xFF, (d->color >> 8) & 0xFF, d->color & 0xFF));
        HGDIOBJ old = SelectObject(dc, pen);
        HBRUSH br = (HBRUSH)GetStockObject(NULL_BRUSH);
        SelectObject(dc, br);
        RoundRect(dc, 0, 0, w - 1, hh - 1, hh, hh);
        SelectObject(dc, old);
        DeleteObject(pen);
        wchar_t text[256];
        GetWindowTextW(h, text, 256);
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, RGB((d->color >> 16) & 0xFF, (d->color >> 8) & 0xFF,
                             d->color & 0xFF));
        DrawTextW(dc, text, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        EndPaint(h, &ps);
        return 0;
    }
    }
    return DefWindowProcW(h, m, wp, lp);
}

/* ================ 主窗口 ================ */

static void confetti_burst(MainData *md, int count) {
    for (int i = 0; i < count && md->n_confetti < 160; i++) {
        int idx = md->n_confetti++;
        md->confetti[idx].x = (float)(rand() % (int)(1000.0f * md->k));
        md->confetti[idx].y = -20.0f;
        md->confetti[idx].vx = (float)((rand() % 200) - 100) / 60.0f;
        md->confetti[idx].vy = (float)(2 + rand() % 4);
        md->confetti[idx].rot = (float)(rand() % 360);
        md->confetti[idx].vr = (float)((rand() % 120) - 60) / 30.0f;
        static const uint32_t cols[] = {
            C_ACCENT_A, C_ACCENT_B, C_SUCCESS, C_WARNING, C_DANGER,
            0xFFFF8CF8, 0xFF2DD4BF
        };
        md->confetti[idx].color = cols[rand() % 7];
        md->confetti[idx].size = 3 + rand() % 5;
        md->confetti[idx].alive = true;
    }
}

static void confetti_tick(MainData *md) {
    bool any = false;
    for (int i = 0; i < md->n_confetti; i++) {
        if (!md->confetti[i].alive) continue;
        md->confetti[i].x += md->confetti[i].vx;
        md->confetti[i].y += md->confetti[i].vy;
        md->confetti[i].rot += md->confetti[i].vr;
        if (md->confetti[i].y > 760.0f * md->k + 20) md->confetti[i].alive = false;
        else any = true;
    }
    if (!any) md->n_confetti = 0;
}

static void paint_bg(MainData *md, HDC dc, int W, int H) {
    if (!md->bg || md->bg_w != W || md->bg_h != H) {
        dib_free(md->bg);
        md->bg = dib_create(W, H);
        if (md->bg) {
            dib_fill_round_gradient(md->bg, K(18), C_BG_TOP, C_BG_BOTTOM,
                                    C_BG_BOTTOM);
            dib_premultiply(md->bg);
        }
        md->bg_w = W;
        md->bg_h = H;
    }
    if (md->bg) dib_blend(md->bg, dc, 0, 0, W, H);

    float t = md->bg_t;
    if (!md->blob1) md->blob1 = dib_soft_blob(128, 0x38BDF8, 20);
    if (!md->blob2) md->blob2 = dib_soft_blob(128, 0x6366F1, 18);
    if (!md->blob3) md->blob3 = dib_soft_blob(128, 0x2DD4BF, 12);
    float fh = H / 1.0f;
    struct { Dib *b; float cx, cy, r; } blobs[3];
    blobs[0].b = md->blob1;
    blobs[0].cx = W * 0.16f + 40.0f * sinf(t * 0.23f);
    blobs[0].cy = H * 0.18f + 30.0f * sinf(t * 0.17f + 1.0f);
    blobs[0].r = fh * 0.62f;
    blobs[1].b = md->blob2;
    blobs[1].cx = W * 0.84f + 34.0f * sinf(t * 0.19f + 2.3f);
    blobs[1].cy = H * 0.72f + 32.0f * sinf(t * 0.15f + 3.2f);
    blobs[1].r = fh * 0.55f;
    blobs[2].b = md->blob3;
    blobs[2].cx = W * 0.52f + 28.0f * sinf(t * 0.26f + 4.0f);
    blobs[2].cy = H * 0.08f + 18.0f * sinf(t * 0.21f + 0.6f);
    blobs[2].r = fh * 0.34f;
    for (int i = 0; i < 3; i++) {
        int r = (int)blobs[i].r;
        if (r <= 0 || !blobs[i].b) continue;
        dib_blend(blobs[i].b, dc, (int)blobs[i].cx - r, (int)blobs[i].cy - r,
                  r * 2, r * 2);
    }
    if (md->geek) {
        int hue = ((int)(t * 70)) % 360;
        float hh = hue / 60.0f;
        float x = 1.0f - fabsf(fmodf(hh, 2.0f) - 1.0f);
        int r = 0, g = 0, b = 0;
        int seg = hue / 60;
        if (seg == 0) { r = 255; g = (int)(x * 255); }
        else if (seg == 1) { r = (int)(x * 255); g = 255; }
        else if (seg == 2) { g = 255; b = (int)(x * 255); }
        else if (seg == 3) { g = (int)(x * 255); b = 255; }
        else if (seg == 4) { r = (int)(x * 255); b = 255; }
        else { r = 255; b = (int)(x * 255); }
        uint32_t rgb = ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
        Dib *rb = dib_soft_blob(128, rgb, 26);
        if (rb) {
            int rr = (int)(H * 0.62f);
            dib_blend(rb, dc, W / 2 - rr, H / 2 - rr, rr * 2, rr * 2);
            dib_free(rb);
        }
    }
    for (int i = 0; i < md->n_confetti; i++) {
        if (!md->confetti[i].alive) continue;
        HBRUSH br = CreateSolidBrush(RGB(
            (md->confetti[i].color >> 16) & 0xFF,
            (md->confetti[i].color >> 8) & 0xFF,
            md->confetti[i].color & 0xFF));
        RECT r = { (LONG)md->confetti[i].x, (LONG)md->confetti[i].y,
                   (LONG)md->confetti[i].x + md->confetti[i].size,
                   (LONG)md->confetti[i].y + md->confetti[i].size + 2 };
        FillRect(dc, &r, br);
        DeleteObject(br);
    }
}

static void set_running_ui(MainData *md, bool on) {
    md->running = on;
    HWND ctrls[] = { md->hDrop, md->hPick, md->hPwd1, md->hEye,
                     md->hPwdList, md->hPwdEdit, md->hOut, md->hBrowse };
    for (size_t i = 0; i < sizeof(ctrls) / sizeof(ctrls[0]); i++)
        if (ctrls[i]) EnableWindow(ctrls[i], !on);
    if (md->hStart) {
        SendMessageW(md->hStart, ZNB_SET_KIND, on ? 2 : 0, 0);
        SetWindowTextW(md->hStart, on ? L"取消任务" : L"开始解压");
    }
    if (md->hStatusPill) {
        SetWindowTextW(md->hStatusPill, on ? L"处理中" : L"空闲");
        SendMessageW(md->hStatusPill, ZP_SET_COLOR, on ? C_WARNING : C_TEXT_SUB, 0);
    }
    if (md->hStatusLabel)
        SetWindowTextW(md->hStatusLabel, on ? L"正在解压…" : L"准备就绪");
    if (md->hStatsLabel) SetWindowTextW(md->hStatsLabel, on ? L"" : L"等待任务…");
    if (md->hBar) SendMessageW(md->hBar, ZB_SET_ACTIVE, on, 0);
    if (md->hRing && !on) SendMessageW(md->hRing, ZR_SET_INDET, 0, 0);
}

void md_append_log(MainData *md, int level, const wchar_t *msg) {
    if (!md->hLog) return;
    static const COLORREF cols[] = {
        RGB(0x7D, 0xD3, 0xFC), RGB(0x6E, 0xE7, 0xB7), RGB(0xFC, 0xD3, 0x4D),
        RGB(0xFC, 0xA5, 0xA5), RGB(0xB6, 0xC2, 0xD9)
    };
    CHARFORMAT2W cf;
    memset(&cf, 0, sizeof(cf));
    cf.cbSize = sizeof(cf);
    cf.dwMask = CFM_COLOR;
    cf.crTextColor = cols[level < 0 ? LOG_INFO : (level > LOG_INFO ? LOG_INFO : level)];
    SendMessageW(md->hLog, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);
    wchar_t buf[MAX_LOG_MSG + 4];
    swprintf(buf, MAX_LOG_MSG + 4, L"%ls\r\n", msg);
    SendMessageW(md->hLog, EM_REPLACESEL, FALSE, (LPARAM)buf);
    SendMessageW(md->hLog, WM_VSCROLL, SB_BOTTOM, 0);
}

static void log_line(MainData *md, int level, const wchar_t *fmt, ...) {
    wchar_t buf[MAX_LOG_MSG];
    va_list ap;
    va_start(ap, fmt);
    _vsnwprintf(buf, MAX_LOG_MSG - 1, fmt, ap);
    va_end(ap);
    buf[MAX_LOG_MSG - 1] = 0;
    md_append_log(md, level, buf);
}

static void md_free_str_array(wchar_t **arr, int n) {
    for (int i = 0; i < n; i++) free(arr[i]);
    free(arr);
}

void md_set_files(MainData *md, const wchar_t **paths, int n) {
    md_free_str_array(md->files, md->nfiles);
    md->files = NULL;
    md->nfiles = 0;
    if (paths && n > 0) {
        md->files = (wchar_t **)xcalloc(n, sizeof(wchar_t *));
        for (int i = 0; i < n; i++) {
            if (file_exists(paths[i]) && supported_archive(paths[i]))
                md->files[md->nfiles++] = xwcsdup(paths[i]);
        }
        if (!md->nfiles) { free(md->files); md->files = NULL; }
    }
    if (!md->hDrop) return;
    if (md->nfiles) {
        wchar_t t[512];
        if (md->nfiles == 1)
        swprintf(t, 512, L"%ls", path_basename(md->files[0]));
        else
            swprintf(t, 512, L"%ls 等 %d 个文件",
                     path_basename(md->files[0]), md->nfiles);
        SetWindowTextW(md->hDrop, t);
        SendMessageW(md->hDrop, ZDZ_SET_SUB, 0, (LPARAM)L"点击可重新选择");
        SetWindowTextW(md->hCount, L"");
        wchar_t cnt[64];
        swprintf(cnt, 64, L"%d 个文件", md->nfiles);
        SetWindowTextW(md->hCount, cnt);
    } else {
        SetWindowTextW(md->hDrop, L"拖入压缩包或文件夹，或点击选择");
        SendMessageW(md->hDrop, ZDZ_SET_SUB, 0,
                     (LPARAM)L"支持 7z / zip / rar / tar，可多文件");
        SetWindowTextW(md->hCount, L"未选择");
    }
}

void md_set_out_dir(MainData *md, const wchar_t *p) {
    wcsncpy(md->out_dir, p ? p : L"", MAX_PATH_BUF - 1);
    md->out_dir[MAX_PATH_BUF - 1] = 0;
    if (md->hOut) SetWindowTextW(md->hOut, md->out_dir);
}

void md_save_settings(MainData *md) {
    save_config_all(&md->cfg, md->first_pwd,
                    (const wchar_t **)md->seconds, md->nseconds,
                    (const wchar_t **)md->files, md->nfiles, md->out_dir);
}

static void select_files_dialog(MainData *md) {
    wchar_t buf[32768];
    buf[0] = 0;
    OPENFILENAMEW ofn;
    memset(&ofn, 0, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = md->hwnd;
    ofn.lpstrFilter =
        L"压缩包 (*.7z *.zip *.rar *.tar *.tar.gz *.tgz)\0"
        L"*.7z;*.zip;*.rar;*.tar;*.tar.gz;*.tgz\0"
        L"所有文件 (*.*)\0*.*\0\0";
    ofn.lpstrFile = buf;
    ofn.nMaxFile = 32768;
    ofn.Flags = OFN_ALLOWMULTISELECT | OFN_EXPLORER | OFN_FILEMUSTEXIST;
    if (!GetOpenFileNameW(&ofn)) return;
    const wchar_t *p = buf;
    if (p[wcslen(p) + 1] == 0) {
        const wchar_t *one[1] = { p };
        md_set_files(md, one, 1);
    } else {
        const wchar_t *dir = p;
        p += wcslen(p) + 1;
        wchar_t **paths = NULL;
        int n = 0;
        while (*p) {
            wchar_t full[MAX_PATH_BUF];
            path_join(full, MAX_PATH_BUF, dir, p);
            paths = (wchar_t **)xrealloc(paths, (n + 1) * sizeof(wchar_t *));
            paths[n++] = xwcsdup(full);
            p += wcslen(p) + 1;
        }
        md_set_files(md, (const wchar_t **)paths, n);
        for (int i = 0; i < n; i++) free(paths[i]);
        free(paths);
    }
    if (md->nfiles) {
        wchar_t m[512];
        swprintf(m, 512, L"已添加 %d 个压缩包", md->nfiles);
        log_line(md, LOG_OK, L"%ls", m);
    }
}

static void select_out_dir_dialog(MainData *md) {
    BROWSEINFOW bi;
    memset(&bi, 0, sizeof(bi));
    bi.hwndOwner = md->hwnd;
    bi.lpszTitle = L"选择输出目录";
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    PIDLIST_ABSOLUTE pidl = SHBrowseForFolderW(&bi);
    if (!pidl) return;
    wchar_t path[MAX_PATH_BUF];
    if (SHGetPathFromIDListW(pidl, path)) md_set_out_dir(md, path);
    CoTaskMemFree(pidl);
}

static void toggle_pwd_visible(MainData *md) {
    LONG style = GetWindowLongW(md->hPwd1, GWL_STYLE);
    if (style & ES_PASSWORD) {
        SetWindowLongW(md->hPwd1, GWL_STYLE, style & ~ES_PASSWORD);
        SetWindowTextW(md->hEye, L"🙈");
    } else {
        SetWindowLongW(md->hPwd1, GWL_STYLE, style | ES_PASSWORD);
        SetWindowTextW(md->hEye, L"👁");
    }
    InvalidateRect(md->hPwd1, NULL, TRUE);
}

static void refresh_pwd_list(MainData *md) {
    SendMessageW(md->hPwdList, LB_RESETCONTENT, 0, 0);
    for (int i = 0; i < md->nseconds; i++)
        SendMessageW(md->hPwdList, LB_ADDSTRING, 0, (LPARAM)md->seconds[i]);
}

static void add_password_from_edit(MainData *md) {
    wchar_t buf[MAX_PWD];
    GetWindowTextW(md->hPwdEdit, buf, MAX_PWD);
    if (!buf[0]) return;
    md->seconds = (wchar_t **)xrealloc(md->seconds,
                                       (md->nseconds + 1) * sizeof(wchar_t *));
    md->seconds[md->nseconds++] = xwcsdup(buf);
    SetWindowTextW(md->hPwdEdit, L"");
    refresh_pwd_list(md);
}

static void import_passwords(MainData *md) {
    wchar_t buf[MAX_PATH_BUF];
    buf[0] = 0;
    OPENFILENAMEW ofn;
    memset(&ofn, 0, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = md->hwnd;
    ofn.lpstrFilter = L"文本文件 (*.txt)\0*.txt\0所有文件 (*.*)\0*.*\0\0";
    ofn.lpstrFile = buf;
    ofn.nMaxFile = MAX_PATH_BUF;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_EXPLORER;
    if (!GetOpenFileNameW(&ofn)) return;
    FILE *f = _wfopen(buf, L"r, ccs=UTF-8");
    if (!f) {
        MessageBoxW(md->hwnd, L"无法读取密码文件", APP_NAME, MB_ICONWARNING);
        return;
    }
    int added = 0;
    wchar_t line[MAX_PWD];
    while (fgetws(line, MAX_PWD, f)) {
        size_t len = wcslen(line);
        while (len && (line[len - 1] == L'\n' || line[len - 1] == L'\r'))
            line[--len] = 0;
        if (!len) continue;
        md->seconds = (wchar_t **)xrealloc(md->seconds,
                                           (md->nseconds + 1) * sizeof(wchar_t *));
        md->seconds[md->nseconds++] = xwcsdup(line);
        added++;
    }
    fclose(f);
    refresh_pwd_list(md);
    if (added) log_line(md, LOG_OK, L"已从文件导入 %d 个密码", added);
}

static void remove_selected_password(MainData *md) {
    int sel = (int)SendMessageW(md->hPwdList, LB_GETCURSEL, 0, 0);
    if (sel < 0 || sel >= md->nseconds) return;
    free(md->seconds[sel]);
    for (int i = sel; i + 1 < md->nseconds; i++) md->seconds[i] = md->seconds[i + 1];
    md->nseconds--;
    refresh_pwd_list(md);
}

static void export_log(MainData *md) {
    wchar_t buf[MAX_PATH_BUF];
    buf[0] = 0;
    OPENFILENAMEW ofn;
    memset(&ofn, 0, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = md->hwnd;
    ofn.lpstrFilter = L"文本文件 (*.txt)\0*.txt\0\0";
    ofn.lpstrFile = buf;
    ofn.nMaxFile = MAX_PATH_BUF;
    ofn.lpstrDefExt = L"txt";
    ofn.lpstrFileTitle = NULL;
    ofn.Flags = OFN_OVERWRITEPROMPT;
    if (!GetSaveFileNameW(&ofn)) return;
    int len = GetWindowTextLengthW(md->hLog);
    wchar_t *text = (wchar_t *)xmalloc((len + 1) * sizeof(wchar_t));
    GetWindowTextW(md->hLog, text, len + 1);
    int u8len = WideCharToMultiByte(CP_UTF8, 0, text, len, NULL, 0, NULL, NULL);
    char *u8 = (char *)xmalloc(u8len + 3);
    u8[0] = (char)0xEF; u8[1] = (char)0xBB; u8[2] = (char)0xBF;
    WideCharToMultiByte(CP_UTF8, 0, text, len, u8 + 3, u8len, NULL, NULL);
    FILE *f = _wfopen(buf, L"wb");
    bool ok = false;
    if (f) { ok = fwrite(u8, 1, u8len + 3, f) == (size_t)(u8len + 3); fclose(f); }
    free(u8);
    free(text);
    log_line(md, ok ? LOG_OK : LOG_ERR, ok ? L"日志已导出：%ls" : L"导出日志失败", buf);
}

static void open_folder(const wchar_t *path) {
    ShellExecuteW(NULL, L"open", path, NULL, NULL, SW_SHOWNORMAL);
}

static void save_history(MainData *md, const WorkerStats *st) {
    HistoryRec r;
    memset(&r, 0, sizeof(r));
    SYSTEMTIME now;
    GetLocalTime(&now);
    swprintf(r.ts, 64, L"%02d-%02d %02d:%02d",
             now.wMonth, now.wDay, now.wHour, now.wMinute);
    r.images = st->images;
    r.archives = st->archives;
    r.elapsed = st->elapsed;
    r.success = 1;
    r.mode = st->mode;
    wcsncpy(r.out, st->out_dir, MAX_PATH_BUF - 1);
    r.files = md->files;
    r.nfiles = md->nfiles;
    history_add(&r);
}

static void open_output(MainData *md) {
    if (md->last_out && dir_exists(md->last_out)) open_folder(md->last_out);
    else MessageBoxW(md->hwnd, L"还没有可用的输出文件夹。", APP_NAME, MB_ICONINFORMATION);
}

static void start_work(MainData *md) {
    if (md->running) {
        if (md->worker) worker_cancel(md->worker);
        log_line(md, LOG_WARN, L"正在取消…");
        EnableWindow(md->hStart, FALSE);
        return;
    }
    if (!md->nfiles) {
        MessageBoxW(md->hwnd, L"请先选择压缩包文件", APP_NAME, MB_ICONWARNING);
        return;
    }
    wchar_t pwd[MAX_PWD];
    GetWindowTextW(md->hPwd1, pwd, MAX_PWD);
    while (pwd[0] == L' ') wmemmove(pwd, pwd + 1, wcslen(pwd));
    if (!pwd[0]) {
        MessageBoxW(md->hwnd, L"请输入一级密码", APP_NAME, MB_ICONWARNING);
        return;
    }
    if (!md->nseconds) {
        MessageBoxW(md->hwnd, L"请至少添加一个二级密码", APP_NAME, MB_ICONWARNING);
        return;
    }
    wcsncpy(md->first_pwd, pwd, MAX_PWD - 1);
    md->first_pwd[MAX_PWD - 1] = 0;
    GetWindowTextW(md->hOut, md->out_dir, MAX_PATH_BUF);
    if (!md->out_dir[0]) wcscpy(md->out_dir, L"extracted_imgs");
    md_save_settings(md);
    free(md->last_out);
    md->last_out = NULL;
    wchar_t absdir[MAX_PATH_BUF];
    if (GetFullPathNameW(md->out_dir, MAX_PATH_BUF, absdir, NULL))
        md->last_out = xwcsdup(absdir);
    else
        md->last_out = xwcsdup(md->out_dir);
    CreateDirectoryW(md->last_out, NULL);
    SetWindowTextW(md->hGallery, L"查看图片");
    EnableWindow(md->hGallery, FALSE);
    EnableWindow(md->hOpen, FALSE);
    SetWindowTextW(md->hLog, L"");
    md->last_joke = 0;
    log_line(md, LOG_STEP, L"任务开始");
    SendMessageW(md->hRing, ZR_SET_VALUE, 0, 0);
    SendMessageW(md->hBar, ZB_SET_VALUE, 0, 0);
    set_running_ui(md, true);
    md->worker = worker_create(md->hwnd,
        (const wchar_t **)md->files, md->nfiles,
        md->first_pwd, (const wchar_t **)md->seconds, md->nseconds,
        md->last_out, &md->cfg);
    worker_start(md->worker);
}

static void on_progress(MainData *md, int v) {
    if (md->hRing) SendMessageW(md->hRing, ZR_SET_VALUE, (WPARAM)(v * 10), 0);
    if (md->hBar) SendMessageW(md->hBar, ZB_SET_VALUE, (WPARAM)(v * 10), 0);
    static const int jokes[3] = { 42, 66, 99 };
    static const wchar_t *joke_text[3] = {
        L"🤔 42% —— 这就是宇宙的答案。",
        L"🔮 66% —— 胜利在望。",
        L"😤 99% —— 就差最后一口气！"
    };
    for (int i = 0; i < 3; i++) {
        if (md->last_joke < jokes[i] && v >= jokes[i]) {
            log_line(md, LOG_INFO, L"%ls", joke_text[i]);
            md->last_joke = jokes[i];
        }
    }
}

static void on_finished(MainData *md, WorkerStats *st) {
    set_running_ui(md, false);
    if (st->cancelled) st->files_ok = 0;
    if (st->files_ok > 0 && !st->cancelled) {
        SetWindowTextW(md->hStatusPill, L"完成");
        SendMessageW(md->hStatusPill, ZP_SET_COLOR, C_SUCCESS, 0);
        SendMessageW(md->hRing, ZR_SET_VALUE, 1000, 0);
        SendMessageW(md->hBar, ZB_SET_VALUE, 1000, 0);
        wchar_t t[256];
        if (st->mode == 0)
            swprintf(t, 256, L"收集到 %d 张图片", st->images);
        else
            swprintf(t, 256, L"已导出 %d 个文件", st->images);
        SetWindowTextW(md->hStatusLabel, t);
        swprintf(t, 256, L"子包 %d 个 · 成功 %d/%d · 用时 %.1f 秒%ls%ls",
                 st->archives, st->files_ok, st->files_total, st->elapsed,
                 st->skipped ? L" · 去重跳过 " : L"",
                 st->skipped ? L"部分" : L"");
        SetWindowTextW(md->hStatsLabel, t);
        EnableWindow(md->hOpen, TRUE);
        if (st->mode == 0 && st->images > 0) {
            wchar_t g[128];
            swprintf(g, 128, L"查看图片 (%d)", st->images);
            SetWindowTextW(md->hGallery, g);
            EnableWindow(md->hGallery, TRUE);
        }
        confetti_burst(md, 90);
        MessageBeep(MB_OK);
        log_line(md, LOG_WARN, L"%ls", FUN_LINES[rand() % 6]);
        save_history(md, st);
        if (md->cfg.auto_open && dir_exists(st->out_dir))
            open_folder(st->out_dir);
    } else {
        SetWindowTextW(md->hStatusPill, L"失败");
        SendMessageW(md->hStatusPill, ZP_SET_COLOR, C_DANGER, 0);
        SetWindowTextW(md->hStatusLabel,
                       st->cancelled ? L"已取消" : L"未提取到图片");
        wchar_t t[256];
        swprintf(t, 256, L"成功 %d/%d · 详见日志",
                 st->files_ok, st->files_total);
        SetWindowTextW(md->hStatsLabel, t);
    }
    worker_free(md->worker);
    md->worker = NULL;
    free(st);
    EnableWindow(md->hStart, TRUE);
}

static void activate_geek(MainData *md) {
    md->geek = true;
    SetWindowTextW(md->hLogo, L"◆");
    SetWindowTextW(GetDlgItem(md->hwnd, IDC_TITLE), L"ZIPedIT · 极客");
    SetWindowTextW(GetDlgItem(md->hwnd, IDC_SUBTITLE), L"秘密模式已激活 ✦ 背景已升级为彩虹能量");
    confetti_burst(md, 160);
    ShowWindow(md->hSelftest, SW_SHOW);
    log_line(md, LOG_WARN, L"🎉 彩蛋解锁：极客模式！右下角出现了「自检」按钮");
}

static void handle_key(MainData *md, WPARAM key) {
    md->konami[md->konami_n % 10] = (int)key;
    md->konami_n++;
    static const int seq[10] = {
        VK_UP, VK_UP, VK_DOWN, VK_DOWN, VK_LEFT, VK_RIGHT, VK_LEFT, VK_RIGHT, 'B', 'A'
    };
    if (md->konami_n >= 10) {
        bool ok = true;
        for (int i = 0; i < 10; i++)
            if (md->konami[(md->konami_n - 10 + i) % 10] != seq[i]) { ok = false; break; }
        if (ok) activate_geek(md);
    }
}

static void collect_dropped_rec(const wchar_t *path, int depth,
                                wchar_t ***out, int *n) {
    if (depth > 64) return;
    if (file_exists(path) && supported_archive(path)) {
        *out = (wchar_t **)xrealloc(*out, (*n + 1) * sizeof(wchar_t *));
        (*out)[(*n)++] = xwcsdup(path);
        return;
    }
    if (!dir_exists(path)) return;
    wchar_t pat[MAX_PATH_BUF];
    path_join(pat, MAX_PATH_BUF, path, L"*");
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW(pat, &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0)
            continue;
        wchar_t full[MAX_PATH_BUF];
        path_join(full, MAX_PATH_BUF, path, fd.cFileName);
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            collect_dropped_rec(full, depth + 1, out, n);
        } else if (supported_archive(fd.cFileName)) {
            *out = (wchar_t **)xrealloc(*out, (*n + 1) * sizeof(wchar_t *));
            (*out)[(*n)++] = xwcsdup(full);
        }
    } while (FindNextFileW(h, &fd));
    FindClose(h);
}

static void collect_dropped(const wchar_t *path, wchar_t ***out, int *n) {
    collect_dropped_rec(path, 0, out, n);
}

static LRESULT CALLBACK MainWndProc(HWND hwnd, UINT m, WPARAM wp, LPARAM lp) {
    MainData *md = (MainData *)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
    if (m == WM_NCCREATE) {
        md = (MainData *)((CREATESTRUCTW *)lp)->lpCreateParams;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)md);
        return TRUE;
    }
    if (!md) return DefWindowProcW(hwnd, m, wp, lp);

    switch (m) {
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(hwnd, &ps);
        RECT rc;
        GetClientRect(hwnd, &rc);
        paint_bg(md, dc, rc.right, rc.bottom);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_TIMER:
        if (wp == 1) {
            md->bg_t += 1.0f / 30.0f;
            confetti_tick(md);
            InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;
    case WM_SHOWWINDOW:
        if (wp) {
            SetTimer(hwnd, 1, 33, NULL);
        } else {
            KillTimer(hwnd, 1);
        }
        return 0;
    case WM_GETMINMAXINFO: {
        MINMAXINFO *mmi = (MINMAXINFO *)lp;
        int W = K(1000), H = K(760);
        mmi->ptMinTrackSize.x = W;
        mmi->ptMinTrackSize.y = H;
        mmi->ptMaxTrackSize.x = W;
        mmi->ptMaxTrackSize.y = H;
        return 0;
    }
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORLISTBOX: {
        HDC dc = (HDC)wp;
        HWND c = (HWND)lp;
        int id = GetDlgCtrlID(c);
        SetBkMode(dc, TRANSPARENT);
        if (id == IDC_LOGO) {
            SetTextColor(dc, RGB(0x38, 0xBD, 0xF8));
        } else if (id == IDC_TITLE || id == IDC_STATUS ||
                   id == IDC_CARDT1 || id == IDC_CARDT2 || id == IDC_CARDT3 ||
                   id == IDC_CARDT4 || id == IDC_CARDT5) {
            SetTextColor(dc, RGB(0xE6, 0xEA, 0xF2));
        } else if (id == IDC_SUBTITLE || id == IDC_VERSION ||
                   id == IDC_LAB1 || id == IDC_LAB2 || id == IDC_LABOUT) {
            SetTextColor(dc, RGB(0x98, 0xA2, 0xB3));
        } else if (id == IDC_PWD1 || id == IDC_PWDEDIT || id == IDC_OUT) {
            SetTextColor(dc, RGB(0xE6, 0xEA, 0xF2));
            SetBkColor(dc, RGB(0x14, 0x1A, 0x2C));
            return (LRESULT)md->edit_brush;
        } else if (id == IDC_PWDLIST) {
            SetTextColor(dc, RGB(0xB6, 0xC2, 0xD9));
            SetBkColor(dc, RGB(0x14, 0x1A, 0x2C));
            return (LRESULT)md->edit_brush;
        } else {
            SetTextColor(dc, RGB(0x5E, 0x66, 0x76));
        }
        return (LRESULT)GetStockObject(NULL_BRUSH);
    }
    case WM_LBUTTONDOWN:
        SendMessageW(hwnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
        return 0;
    case WM_KEYDOWN:
        handle_key(md, wp);
        return 0;
    case WM_DROPFILES: {
        HDROP hDrop = (HDROP)wp;
        int count = DragQueryFileW(hDrop, 0xFFFFFFFF, NULL, 0);
        wchar_t **found = NULL;
        int n = 0;
        for (int i = 0; i < count; i++) {
            wchar_t path[MAX_PATH_BUF];
            DragQueryFileW(hDrop, i, path, MAX_PATH_BUF);
            collect_dropped(path, &found, &n);
        }
        DragFinish(hDrop);
        if (n) {
            md_set_files(md, (const wchar_t **)found, n);
            log_line(md, LOG_OK, L"已添加 %d 个压缩包", md->nfiles);
        }
        for (int i = 0; i < n; i++) free(found[i]);
        free(found);
        return 0;
    }
    case WM_COMMAND: {
        int id = LOWORD(wp);
        switch (id) {
        case IDC_PICK:
        case IDC_DROP:
            select_files_dialog(md);
            return 0;
        case IDC_BROWSE:
            select_out_dir_dialog(md);
            return 0;
        case IDC_EYE:
            toggle_pwd_visible(md);
            return 0;
        case IDC_PWDADD:
            add_password_from_edit(md);
            return 0;
        case IDC_PWDIMPORT:
            import_passwords(md);
            return 0;
        case IDC_PWDREMOVE:
            remove_selected_password(md);
            return 0;
        case IDC_CLEAR:
            SetWindowTextW(md->hLog, L"");
            return 0;
        case IDC_EXPORT:
            export_log(md);
            return 0;
        case IDC_START:
            start_work(md);
            return 0;
        case IDC_OPEN:
            open_output(md);
            return 0;
        case IDC_GALLERY:
            if (md->last_out && dir_exists(md->last_out))
                dialog_open_gallery(md->hwnd, md->last_out);
            else
                MessageBoxW(hwnd, L"还没有可预览的图片。", APP_NAME, MB_ICONINFORMATION);
            return 0;
        case IDC_SETTINGS:
            dialog_open_settings(hwnd, md);
            return 0;
        case IDC_ABOUT:
            dialog_open_about(hwnd);
            return 0;
        case IDC_HISTORY:
            dialog_open_history(hwnd, md);
            return 0;
        case IDC_SELFTEST: {
            md_append_log(md, LOG_STEP, L"🔬 自检需要以命令行运行：zipedit.exe --selftest");
            return 0;
        }
        case IDC_LOGO:
            md->logo_clicks++;
            if (md->logo_clicks % 5 == 0) {
                confetti_burst(md, 60);
                log_line(md, LOG_WARN, L"💎 水晶能量爆发！连续点击 logo 的彩蛋已触发");
            }
            return 0;
        case IDC_VERSION:
            md->ver_idx = (md->ver_idx + 1) % 5;
            SetWindowTextW(md->hVersion, VERSION_CYCLE[md->ver_idx]);
            log_line(md, LOG_INFO, L"版本：%ls ✨", VERSION_CYCLE[md->ver_idx]);
            return 0;
        case IDC_MIN:
            ShowWindow(hwnd, SW_MINIMIZE);
            return 0;
        case IDC_CLOSE:
            SendMessageW(hwnd, WM_CLOSE, 0, 0);
            return 0;
        }
        break;
    }
    case WM_APP_LOG: {
        wchar_t *msg = (wchar_t *)lp;
        md_append_log(md, (int)wp, msg);
        free(msg);
        return 0;
    }
    case WM_APP_PROGRESS:
        on_progress(md, (int)wp);
        return 0;
    case WM_APP_BUSY:
        if (md->hRing) SendMessageW(md->hRing, ZR_SET_INDET, wp, 0);
        if (md->hBar) SendMessageW(md->hBar, ZB_SET_ACTIVE, wp, 0);
        return 0;
    case WM_APP_FINISHED:
        on_finished(md, (WorkerStats *)lp);
        return 0;
    case WM_CLOSE:
        md_save_settings(md);
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, m, wp, lp);
}

static void build_ui(MainData *md) {
    HWND h = md->hwnd;
    int W = K(1000), H = K(760);
    md->f_ui = mkfont(10, 0);
    md->f_bold = mkfont(11, 1);
    md->f_small = mkfont(9, 0);
    md->f_title = mkfont(14, 1);

    /* 标题栏 */
    md->hLogo = mk(h, L"STATIC", L"◆", IDC_LOGO, K(26), K(12), K(24), K(24),
                    SS_NOTIFY, WS_EX_TRANSPARENT);
    set_font(md->hLogo, md->f_title);
    HWND t1 = mk(h, L"STATIC", APP_TITLE, IDC_TITLE, K(56), K(8), K(220), K(26),
                  SS_LEFT, WS_EX_TRANSPARENT | WS_DISABLED);
    set_font(t1, md->f_title);
    HWND t2 = mk(h, L"STATIC", APP_SUBTITLE, IDC_SUBTITLE, K(56), K(32), K(340), K(20),
                  SS_LEFT, WS_EX_TRANSPARENT | WS_DISABLED);
    set_font(t2, md->f_small);
    md->hVersion = mk(h, L"STATIC", APP_VERSION, IDC_VERSION, W - K(220), K(18),
                      K(160), K(24), SS_RIGHT | SS_NOTIFY, WS_EX_TRANSPARENT);
    set_font(md->hVersion, md->f_small);
    HWND btnMin = mk(h, L"ZipNeoButton", L"—", IDC_MIN, W - K(84), K(10), K(30), K(30), 0, 0);
    HWND btnClose = mk(h, L"ZipNeoButton", L"✕", IDC_CLOSE, W - K(46), K(10), K(30), K(30), 0, 0);
    SendMessageW(btnMin, ZNB_SET_KIND, 1, 0);
    SendMessageW(btnClose, ZNB_SET_KIND, 1, 0);
    set_font(btnMin, md->f_ui);
    set_font(btnClose, md->f_ui);

    int bodyY = K(76);
    int leftX = K(26), leftW = K(558);
    int rightX = K(600), rightW = K(358);

    /* 卡片背景 */
    mk(h, L"ZipCard", L"", 0, leftX, bodyY, leftW, K(218), 0, 0);
    mk(h, L"ZipCard", L"", 0, leftX, bodyY + K(232), leftW, K(210), 0, 0);
    mk(h, L"ZipCard", L"", 0, leftX, bodyY + K(456), leftW, K(120), 0, 0);
    mk(h, L"ZipCard", L"", 0, rightX, bodyY, rightW, K(250), 0, 0);
    mk(h, L"ZipCard", L"", 0, rightX, bodyY + K(264), rightW, K(354), 0, 0);

    /* 卡片 1：压缩包文件 */
    HWND chip = mk(h, L"STATIC", L"1", 0, leftX + K(16), bodyY + K(14), K(24), K(24),
                    SS_CENTER | SS_CENTERIMAGE, WS_EX_TRANSPARENT);
    set_font(chip, md->f_small);
    HWND cardTitle = mk(h, L"STATIC", L"压缩包文件", IDC_CARDT1,
                        leftX + K(48), bodyY + K(14),
                        K(160), K(24), SS_LEFT, WS_EX_TRANSPARENT);
    set_font(cardTitle, md->f_bold);
    md->hCount = mk(h, L"ZipPill", L"未选择", IDC_COUNT,
                    leftX + leftW - K(16) - K(96), bodyY + K(12), K(96), K(26), 0, 0);
    set_font(md->hCount, md->f_small);
    HWND pick = mk(h, L"ZipNeoButton", L"选择文件", IDC_PICK,
                   leftX + leftW - K(16) - K(90), bodyY + K(44), K(90), K(30), 0, 0);
    md->hPick = pick;
    SendMessageW(pick, ZNB_SET_KIND, 1, 0);
    set_font(pick, md->f_ui);
    md->hDrop = mk(h, L"ZipDropZone", L"拖入压缩包或文件夹，或点击选择", IDC_DROP,
                   leftX + K(16), bodyY + K(84), leftW - K(32), K(122), 0, 0);
    SendMessageW(md->hDrop, ZDZ_SET_SUB, 0,
                 (LPARAM)L"支持 7z / zip / rar / tar，可多文件");
    set_font(md->hDrop, md->f_ui);

    /* 卡片 2：密码设置 */
    chip = mk(h, L"STATIC", L"2", 0, leftX + K(16), bodyY + K(246), K(24), K(24),
              SS_CENTER | SS_CENTERIMAGE, WS_EX_TRANSPARENT);
    set_font(chip, md->f_small);
    cardTitle = mk(h, L"STATIC", L"密码设置", IDC_CARDT2,
                   leftX + K(48), bodyY + K(246),
                   K(160), K(24), SS_LEFT, WS_EX_TRANSPARENT);
    set_font(cardTitle, md->f_bold);
    HWND lbl1 = mk(h, L"STATIC", L"一级密码", IDC_LAB1,
                   leftX + K(16), bodyY + K(284),
                   K(70), K(30), SS_LEFT | SS_CENTERIMAGE, WS_EX_TRANSPARENT);
    set_font(lbl1, md->f_ui);
    md->hPwd1 = mk(h, L"EDIT", L"", IDC_PWD1,
                   leftX + K(92), bodyY + K(284), leftW - K(152), K(34),
                   WS_TABSTOP | ES_AUTOHSCROLL | ES_PASSWORD, WS_EX_CLIENTEDGE);
    set_font(md->hPwd1, md->f_ui);
    md->hEye = mk(h, L"ZipNeoButton", L"👁", IDC_EYE,
                  leftX + leftW - K(16) - K(44), bodyY + K(284), K(44), K(34), 0, 0);
    SendMessageW(md->hEye, ZNB_SET_KIND, 1, 0);
    set_font(md->hEye, md->f_ui);
    HWND lbl2 = mk(h, L"STATIC", L"二级密码", IDC_LAB2,
                   leftX + K(16), bodyY + K(328),
                   K(160), K(20), SS_LEFT, WS_EX_TRANSPARENT);
    set_font(lbl2, md->f_ui);
    md->hPwdList = mk(h, L"LISTBOX", L"", IDC_PWDLIST,
                      leftX + K(16), bodyY + K(352), leftW - K(32), K(58),
                      WS_TABSTOP | LBS_NOTIFY | WS_VSCROLL, WS_EX_CLIENTEDGE);
    set_font(md->hPwdList, md->f_ui);
    md->hPwdEdit = mk(h, L"EDIT", L"", IDC_PWDEDIT,
                      leftX + K(16), bodyY + K(418), K(330), K(30),
                      WS_TABSTOP | ES_AUTOHSCROLL, WS_EX_CLIENTEDGE);
    set_font(md->hPwdEdit, md->f_ui);
    HWND add = mk(h, L"ZipNeoButton", L"添加", IDC_PWDADD,
                  leftX + K(352), bodyY + K(418), K(70), K(30), 0, 0);
    SendMessageW(add, ZNB_SET_KIND, 1, 0);
    set_font(add, md->f_ui);
    HWND imp = mk(h, L"ZipNeoButton", L"导入", IDC_PWDIMPORT,
                  leftX + K(428), bodyY + K(418), K(70), K(30), 0, 0);
    SendMessageW(imp, ZNB_SET_KIND, 1, 0);
    set_font(imp, md->f_ui);
    HWND del = mk(h, L"ZipNeoButton", L"删", IDC_PWDREMOVE,
                  leftX + K(504), bodyY + K(418), K(38), K(30), 0, 0);
    SendMessageW(del, ZNB_SET_KIND, 1, 0);
    set_font(del, md->f_ui);

    /* 卡片 3：输出目录 */
    cardTitle = mk(h, L"STATIC", L"输出目录", IDC_LABOUT,
                   leftX + K(16), bodyY + K(488),
                   K(70), K(34), SS_LEFT | SS_CENTERIMAGE, WS_EX_TRANSPARENT);
    set_font(cardTitle, md->f_ui);
    md->hOut = mk(h, L"EDIT", L"", IDC_OUT,
                  leftX + K(92), bodyY + K(488), leftW - K(172), K(34),
                  WS_TABSTOP | ES_AUTOHSCROLL, WS_EX_CLIENTEDGE);
    set_font(md->hOut, md->f_ui);
    HWND browse = mk(h, L"ZipNeoButton", L"浏览", IDC_BROWSE,
                     leftX + leftW - K(16) - K(64), bodyY + K(488), K(64), K(34), 0, 0);
    md->hBrowse = browse;
    SendMessageW(browse, ZNB_SET_KIND, 1, 0);
    set_font(browse, md->f_ui);

    /* 卡片 4：处理进度 */
    cardTitle = mk(h, L"STATIC", L"处理进度", IDC_CARDT4,
                   rightX + K(16), bodyY + K(14),
                   K(160), K(24), SS_LEFT, WS_EX_TRANSPARENT);
    set_font(cardTitle, md->f_bold);
    md->hStatusPill = mk(h, L"ZipPill", L"空闲", IDC_STATUSPILL,
                         rightX + rightW - K(16) - K(96), bodyY + K(12), K(96), K(26), 0, 0);
    set_font(md->hStatusPill, md->f_small);
    md->hRing = mk(h, L"ZipRing", L"", IDC_RING,
                   rightX + K(16), bodyY + K(52), K(96), K(96), 0, 0);
    md->hStatusLabel = mk(h, L"STATIC", L"准备就绪", IDC_STATUS,
                          rightX + K(124), bodyY + K(54), rightW - K(140), K(24),
                          SS_LEFT, WS_EX_TRANSPARENT);
    set_font(md->hStatusLabel, md->f_bold);
    md->hBar = mk(h, L"ZipBar", L"", IDC_BAR,
                  rightX + K(124), bodyY + K(88), rightW - K(140), K(14), 0, 0);
    md->hStatsLabel = mk(h, L"STATIC", L"等待任务…", IDC_STATS,
                         rightX + K(124), bodyY + K(112), rightW - K(140), K(72),
                         SS_LEFT, WS_EX_TRANSPARENT);
    set_font(md->hStatsLabel, md->f_small);
    md->hStart = mk(h, L"ZipNeoButton", L"开始解压", IDC_START,
                    rightX + K(16), bodyY + K(196), rightW - K(32), K(42), 0, 0);
    SendMessageW(md->hStart, ZNB_SET_KIND, 0, 0);
    set_font(md->hStart, md->f_bold);

    /* 卡片 5：日志 */
    HWND dot = mk(h, L"STATIC", L"●", 0, rightX + K(16), bodyY + K(278), K(12), K(24),
                  SS_LEFT, WS_EX_TRANSPARENT);
    set_font(dot, md->f_small);
    cardTitle = mk(h, L"STATIC", L"运行日志", IDC_CARDT5,
                   rightX + K(34), bodyY + K(278),
                   K(120), K(24), SS_LEFT, WS_EX_TRANSPARENT);
    set_font(cardTitle, md->f_bold);
    HWND clearBtn = mk(h, L"ZipNeoButton", L"清空", IDC_CLEAR,
                       rightX + rightW - K(16) - K(140), bodyY + K(276), K(64), K(26), 0, 0);
    SendMessageW(clearBtn, ZNB_SET_KIND, 1, 0);
    set_font(clearBtn, md->f_small);
    HWND exportBtn = mk(h, L"ZipNeoButton", L"导出", IDC_EXPORT,
                        rightX + rightW - K(16) - K(70), bodyY + K(276), K(64), K(26), 0, 0);
    SendMessageW(exportBtn, ZNB_SET_KIND, 1, 0);
    set_font(exportBtn, md->f_small);
    md->hLog = mk(h, RICHEDIT_CLASS_W, L"", IDC_LOG,
                  rightX + K(16), bodyY + K(310), rightW - K(32), K(296),
                  ES_MULTILINE | ES_READONLY | WS_VSCROLL | ES_AUTOVSCROLL,
                  WS_EX_CLIENTEDGE);
    set_font(md->hLog, md->f_ui);
    SendMessageW(md->hLog, EM_SETBKGNDCOLOR, 0, (LPARAM)RGB(0x10, 0x14, 0x1E));
    CHARFORMAT2W cf;
    memset(&cf, 0, sizeof(cf));
    cf.cbSize = sizeof(cf);
    cf.dwMask = CFM_COLOR;
    cf.crTextColor = RGB(0xB6, 0xC2, 0xD9);
    SendMessageW(md->hLog, EM_SETCHARFORMAT, SCF_ALL, (LPARAM)&cf);

    /* 底部栏 */
    HWND hint = mk(h, L"STATIC", L"可拖入 7z / zip / rar / tar 等压缩包到窗口任意位置",
                   0, K(26), H - K(44), K(420), K(18),
                   SS_LEFT, WS_EX_TRANSPARENT | WS_DISABLED);
    set_font(hint, md->f_small);
    int widths[6] = { K(78), K(78), K(78), K(104), K(96), K(70) };
    int total = 0;
    for (int i = 0; i < 6; i++) total += widths[i] + K(6);
    total -= K(6);
    int x = W - K(26) - total;
    int y = H - K(52), bh = K(34);
    const wchar_t *labels[6] = { L"设置", L"关于", L"历史", L"查看图片", L"打开输出", L"自检" };
    int ids[6] = { IDC_SETTINGS, IDC_ABOUT, IDC_HISTORY, IDC_GALLERY, IDC_OPEN, IDC_SELFTEST };
    for (int i = 0; i < 6; i++) {
        HWND b = mk(h, L"ZipNeoButton", labels[i], ids[i], x, y, widths[i], bh, 0, 0);
        SendMessageW(b, ZNB_SET_KIND, 1, 0);
        set_font(b, md->f_ui);
        if (i == 3) md->hGallery = b;
        if (i == 4) md->hOpen = b;
        if (i == 5) md->hSelftest = b;
        x += widths[i] + K(6);
    }
    ShowWindow(md->hSelftest, SW_HIDE);
    EnableWindow(md->hGallery, FALSE);
    EnableWindow(md->hOpen, FALSE);

    /* 初始化状态 */
    refresh_pwd_list(md);
    SetWindowTextW(md->hPwd1, md->first_pwd);
    SetWindowTextW(md->hOut, md->out_dir);
    md_set_files(md, (const wchar_t **)md->files, md->nfiles);
    SendMessageW(md->hRing, ZR_SET_VALUE, 0, 0);
    SendMessageW(md->hBar, ZB_SET_VALUE, 0, 0);
    if (!md->nseconds) {
        md->seconds = (wchar_t **)xcalloc(3, sizeof(wchar_t *));
        md->seconds[0] = xwcsdup(L"123ab");
        md->seconds[1] = xwcsdup(L"123AB");
        md->seconds[2] = xwcsdup(L"ab123");
        md->nseconds = 3;
        refresh_pwd_list(md);
    }
    md->edit_brush = CreateSolidBrush(RGB(0x14, 0x1A, 0x2C));
}

void maindata_free(MainData *md) {
    if (!md) return;
    if (md->worker) worker_free(md->worker);
    md_free_str_array(md->files, md->nfiles);
    md_free_str_array(md->seconds, md->nseconds);
    free(md->last_out);
    DeleteObject(md->f_ui);
    DeleteObject(md->f_bold);
    DeleteObject(md->f_small);
    DeleteObject(md->f_title);
    if (md->edit_brush) DeleteObject(md->edit_brush);
    dib_free(md->blob1);
    dib_free(md->blob2);
    dib_free(md->blob3);
    dib_free(md->bg);
    free(md);
}

bool ui_register_classes(HINSTANCE hInst) {
    g_hInst = hInst;
    struct { const wchar_t *name; WNDPROC proc; } cls[] = {
        { L"ZipCard", CardProc }, { L"ZipNeoButton", NeoButtonProc },
        { L"ZipDropZone", DropZoneProc }, { L"ZipRing", RingProc },
        { L"ZipBar", BarProc }, { L"ZipPill", PillProc },
        { L"ZipMain", MainWndProc }
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

HWND create_main_window(HINSTANCE hInst) {
    MainData *md = (MainData *)xcalloc(1, sizeof(MainData));
    md->inst = hInst;
    md->k = get_scale();
    md->nseconds = 0;
    md->nfiles = 0;
    load_config(&md->cfg);
    load_ui_fields(md->first_pwd, MAX_PWD, &md->seconds, &md->nseconds,
                   &md->files, &md->nfiles, md->out_dir, MAX_PATH_BUF);
    int W = K(1000), H = K(760);
    HWND hwnd = CreateWindowExW(WS_EX_APPWINDOW,
                                L"ZipMain", APP_NAME,
                                WS_POPUP | WS_SYSMENU | WS_MINIMIZEBOX |
                                WS_CLIPCHILDREN,
                                CW_USEDEFAULT, CW_USEDEFAULT, W, H,
                                NULL, NULL, hInst, md);
    if (!hwnd) {
        maindata_free(md);
        return NULL;
    }
    md->hwnd = hwnd;
    HRGN rgn = CreateRoundRectRgn(0, 0, W + 1, H + 1, K(36), K(36));
    if (rgn) SetWindowRgn(hwnd, rgn, TRUE);
    HMODULE msftedit = LoadLibraryW(L"Msftedit.dll");
    md->rich_ok = msftedit != NULL;
    build_ui(md);
    DragAcceptFiles(hwnd, TRUE);
    SetFocus(md->hPwd1);
    return hwnd;
}
