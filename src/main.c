#include "app.h"
#include <time.h>

int wmain(int argc, wchar_t **argv) {
    if (argc > 1 &&
        (wcscmp(argv[1], L"--selftest") == 0 ||
         wcscmp(argv[1], L"-t") == 0)) {
        return run_selftest();
    }
    SetProcessDPIAware();
    srand((unsigned)time(NULL));
    INITCOMMONCONTROLSEX icc;
    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_WIN95_CLASSES | ICC_STANDARD_CLASSES | ICC_LISTVIEW_CLASSES;
    InitCommonControlsEx(&icc);
    HINSTANCE hInst = GetModuleHandleW(NULL);
    if (!ui_register_classes(hInst)) {
        MessageBoxW(NULL, L"注册窗口类失败", APP_NAME, MB_ICONERROR);
        return 1;
    }
    if (!dialogs_register_classes(hInst)) {
        MessageBoxW(NULL, L"注册对话框类失败", APP_NAME, MB_ICONERROR);
        return 1;
    }
    HWND hwnd = create_main_window(hInst);
    if (!hwnd) {
        MessageBoxW(NULL, L"创建主窗口失败", APP_NAME, MB_ICONERROR);
        return 1;
    }
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);
    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return (int)msg.wParam;
}
