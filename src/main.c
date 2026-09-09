#include <windows.h>
#include <windowsx.h>

#include "resource.h"
#include "power.h"
#include "tray.h"
#include "menu.h"

static UINT g_uTaskbarCreatedMsg = 0;

// power notification handles, unregistered in WM_DESTROY
static HPOWERNOTIFY g_hPowerSrcNotify = NULL;
static HPOWERNOTIFY g_hPowerSaveNotify = NULL;

// throttles the hover requery, a hover fires dozens of WM_MOUSEMOVE
static ULONGLONG g_uLastTooltipRefresh = 0;

LRESULT CALLBACK MainWindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

/**
 * GUI entry point
 */
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nShowCmd) {

    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);
    UNREFERENCED_PARAMETER(nShowCmd);

    MSG uMsg = {0};
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
    // guard the id, 0 would alias WM_NULL which menu.c posts after every menu
    if (g_uTaskbarCreatedMsg != 0 && uMsg == g_uTaskbarCreatedMsg) {
        Tray_Init(hWnd);
        return 0;
    }

    switch(uMsg) {
        case WM_CREATE: { // Window created

            // seed the flags before Tray_Init, it formats the first tooltip from them
            SYSTEM_POWER_STATUS sps;
            if (GetSystemPowerStatus(&sps)) {
                // 0 = battery, 1 = plugged in, 255 = unknown, treat unknown as AC
                g_bIsAC = (sps.ACLineStatus != 0);
                g_bBatterySaverActive = (sps.SystemStatusFlag == 1);
            }

            // subscribe to the only two OS events this app listens for
            g_hPowerSrcNotify  = RegisterPowerSettingNotification(hWnd, &GUID_SRC_ACDC, DEVICE_NOTIFY_WINDOW_HANDLE);
            g_hPowerSaveNotify = RegisterPowerSettingNotification(hWnd, &GUID_SAVER_STATUS, DEVICE_NOTIFY_WINDOW_HANDLE);

            Tray_Init(hWnd); // initialise tray icon

            break;
        }
        case WM_COMMAND: { // Menu item clicked

            DWORD ID = LOWORD(wParam);

            // Set correct power mode based on item selected
            if (ID >= IDM_AC_EFFICIENCY && ID <= IDM_AC_PERFORMANCE) {
                const GUID *pMode;
                if (ID == IDM_AC_EFFICIENCY) {
                    pMode = &GUID_POWER_MODE_BEST_EFFICIENCY;
                } else if (ID == IDM_AC_BALANCED) {
                    pMode = &GUID_POWER_MODE_NONE;
                } else {
                    pMode = &GUID_POWER_MODE_BEST_PERFORMANCE;
                }

                DWORD status = g_PowerSubsys.SetACMode(pMode);
                Tray_UpdateTooltip(hWnd);

                // policy or a driver override can refuse the switch, tell the user
                if (status != ERROR_SUCCESS) {
                    // NULL owner, our window is 0x0 so a dialog owned by it lands in the corner
                    MessageBoxW(NULL, L"Windows refused to change the power mode.", L"Win11PowerModeTray", MB_OK | MB_ICONWARNING);
                }
            } else if (ID >= IDM_DC_EFFICIENCY && ID <= IDM_DC_PERFORMANCE) {
                const GUID *pMode;
                if (ID == IDM_DC_EFFICIENCY) {
                    pMode = &GUID_POWER_MODE_BEST_EFFICIENCY;
                } else if (ID == IDM_DC_BALANCED) {
                    pMode = &GUID_POWER_MODE_NONE;
                } else {
                    pMode = &GUID_POWER_MODE_BEST_PERFORMANCE;
                }

                DWORD status = g_PowerSubsys.SetDCMode(pMode);
                Tray_UpdateTooltip(hWnd);

                if (status != ERROR_SUCCESS) {
                    MessageBoxW(NULL, L"Windows refused to change the power mode.", L"Win11PowerModeTray", MB_OK | MB_ICONWARNING);
                }
            } else if (ID == IDM_EXIT) {
                DestroyWindow(hWnd);
            }

            break;
        }
        case WM_POWERBROADCAST: { // AC/DC or energy-saver state changed

            if (wParam == PBT_POWERSETTINGCHANGE) {
                const POWERBROADCAST_SETTING *pSetting = (const POWERBROADCAST_SETTING *)lParam;

                // both registrations arrive here, the GUID tells them apart
                if (pSetting && pSetting->DataLength >= sizeof(DWORD)) {

                    // payload is a DWORD, Data[0] would only read the low byte
                    DWORD value = *(const DWORD *)pSetting->Data;
                    BOOL bChanged = FALSE;

                    if (IsEqualGUID(&pSetting->PowerSetting, &GUID_SRC_ACDC)) {
                        // 0 = AC (plugged in), 1 = DC (on battery)
                        g_bIsAC = (value == 0);
                        bChanged = TRUE;
                    } else if (IsEqualGUID(&pSetting->PowerSetting, &GUID_SAVER_STATUS)) {
                        // 0 = off, 1 = energy saver on
                        g_bBatterySaverActive = (value != 0);
                        bChanged = TRUE;
                    }

                    // tooltip names the power source too, so either flag makes it stale
                    if (bChanged) {
                        Tray_UpdateTooltip(hWnd);
                    }
                }
            }

            // WM_POWERBROADCAST must return TRUE, not the trailing return 0
            return TRUE;
        }

        case WM_DPICHANGED:      // moved to a monitor with different scaling
        case WM_DISPLAYCHANGE: { // resolution or monitor layout changed

            // cached icon was sized for the old dpi, reload it
            Tray_UpdateIcon(hWnd);

            break;
        }

        case WM_DESTROY: // Window destroyed

            // drop the power subscriptions before tearing anything else down
            if (g_hPowerSrcNotify) {
                UnregisterPowerSettingNotification(g_hPowerSrcNotify);
                g_hPowerSrcNotify = NULL;
            }
            if (g_hPowerSaveNotify) {
                UnregisterPowerSettingNotification(g_hPowerSaveNotify);
                g_hPowerSaveNotify = NULL;
            }

            Tray_Cleanup();
            PowerSubsystem_Shutdown();
            PostQuitMessage(0);

            break;
        case WM_APP_TRAYMSG: { // Tray clicked

            UINT uCode = LOWORD(lParam);

            // hover means the tooltip is about to show, requery in case settings changed the mode
            if (uCode == WM_MOUSEMOVE) {
                if (GetTickCount64() - g_uLastTooltipRefresh >= 1000) {
                    g_uLastTooltipRefresh = GetTickCount64();
                    Tray_UpdateTooltip(hWnd);
                }

                break;
            }

            // if left/right click and 150ms have passed, show the menu.
            if ((uCode == NIN_SELECT || uCode == WM_CONTEXTMENU) && GetTickCount64() - g_uLastMenuDismissTime >= 150) {
                // v4 hands us the anchor point in wParam, signed so monitors left of primary work
                POINT pt = { GET_X_LPARAM(wParam), GET_Y_LPARAM(wParam) };
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