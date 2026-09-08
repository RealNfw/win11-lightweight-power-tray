#include <windows.h>

#include "resource.h"
#include "power.h"
#include "tray.h"
#include "menu.h"

static UINT g_uTaskbarCreatedMsg = 0;
LRESULT CALLBACK MainWindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

/**
 * GUI entry point
 */
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nShowCmd) {

    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);
    UNREFERENCED_PARAMETER(nShowCmd);

    MSG uMsg;
    WNDCLASSEXW WndCls = {0};

    WndCls.cbSize = sizeof(WNDCLASSEXW);
    WndCls.lpfnWndProc = MainWindowProc;
    WndCls.hInstance = hInstance;
    WndCls.lpszClassName = L"PowerModeTray";

    // aqcuire tray mutex
    HANDLE trayMutex = CreateMutexW(NULL, TRUE, L"Local\\Win11PowerModeTray_Mutex");

    // check if mutex already existed, if so, open the window
    if (GetLastError() == ERROR_ALREADY_EXISTS) {

        HWND hExisting = NULL;

        // find existing window over 1.5s
        int i = 0;
        while (i < 15) {
            hExisting = FindWindowW(WndCls.lpszClassName, NULL);
            if (hExisting) {
                break;
            }

            i++;
            Sleep(100);
        }

        // if window is found, show it
        if (hExisting) {
            DWORD targetPid = 0;
            GetWindowThreadProcessId(hExisting, &targetPid);
            AllowSetForegroundWindow(targetPid);

            PostMessageW(hExisting, WM_APP_RESHOW_TRAY, 0, 0);
        } else {
            MessageBoxW(
                NULL,
                L"Win11PowerModeTray is already running, but the existing instance could not be found.",
                L"Win11PowerModeTray",
                MB_OK | MB_ICONWARNING);
        }

        CloseHandle(trayMutex);
        return 0;
    }

    if (trayMutex == NULL) {
        return 1;
    }

    if (!PowerSubsystem_Init(TRUE)) {
        CloseHandle(trayMutex);
        return 1;
    }

    if (!RegisterClassExW(&WndCls)) {
        CloseHandle(trayMutex);
        PowerSubsystem_Shutdown();
        return 1;
    }

    g_uTaskbarCreatedMsg = RegisterWindowMessageW(L"TaskbarCreated");
    HWND hWnd = CreateWindowExW(0, WndCls.lpszClassName, L"", WS_POPUP, 0, 0, 0, 0, NULL, NULL, hInstance, NULL);
    if (!hWnd) {
        CloseHandle(trayMutex);
        PowerSubsystem_Shutdown();
        return 1;
    }

    // blocking loop until QUIT message is sent.
    while (GetMessageW(&uMsg, NULL, 0, 0) > 0) {
        TranslateMessage(&uMsg);
        DispatchMessageW(&uMsg);
    }

    if (trayMutex) {
        ReleaseMutex(trayMutex);
        CloseHandle(trayMutex);
    }

    // returns whatever exit code was passted to PostQuitMessage
    return (int) uMsg.wParam;
}

/**
 * Sends messages for the hidden window
 */
LRESULT CALLBACK MainWindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {

    // re initialise tray icon if explorer restarts
    if (uMsg == g_uTaskbarCreatedMsg) {
        Tray_Init(hWnd);
        return 0;
    }

    switch(uMsg) {
        case WM_CREATE: // Window created

            Tray_Init(hWnd); // initialise tray icon

            break;
        case WM_COMMAND: { // Menu item clicked

            DWORD ID = LOWORD(wParam);

            // Set correct power mode based on item selected
            if (ID >= IDM_AC_EFFICIENCY && ID <= IDM_AC_PERFORMANCE) {
                DWORD mode = ID - IDM_AC_EFFICIENCY;
                g_PowerSubsys.SetACMode(mode);
                Tray_UpdateTooltip(hWnd);
            } else if (ID >= IDM_DC_EFFICIENCY && ID <= IDM_DC_PERFORMANCE) {
                DWORD mode = ID - IDM_DC_EFFICIENCY;
                g_PowerSubsys.SetDCMode(mode);
                Tray_UpdateTooltip(hWnd);
            } else if (ID == IDM_EXIT) {
                DestroyWindow(hWnd);
            }

            break;
        }
        case WM_DESTROY: // Window destroyed

            Tray_Cleanup();
            PowerSubsystem_Shutdown();
            PostQuitMessage(0);

            break;
        case WM_APP_TRAYMSG: { // Tray clicked

            UINT uCode = LOWORD(lParam);

            // if left/right click and 150ms have passed, show the menu.
            if ((uCode == NIN_SELECT || uCode == WM_CONTEXTMENU) && GetTickCount64() - g_uLastMenuDismissTime >= 150) {
                POINT pt;
                GetCursorPos(&pt);
                ShowContextMenu(hWnd, pt);
            }

            break;
        }
        case WM_APP_RESHOW_TRAY: { // Reshow train on second program open
            POINT pt;
            Tray_GetAnchorPoint(hWnd, &pt);
            ShowContextMenu(hWnd, pt);

            break;
        }
        default:
            return DefWindowProcW(hWnd, uMsg, wParam, lParam);
    }

    return 0;
}