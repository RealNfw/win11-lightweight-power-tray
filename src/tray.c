#include "power.h"
#include "tray.h"
#include "menu.h"
#include "resource.h"
#include <strsafe.h>

NOTIFYICONDATAW g_nid = {0};
BOOL g_bIsAC = TRUE;
BOOL g_bBatterySaverActive = FALSE;

static UINT GetCurrentTrayDpi(void) {

    // taskbar's own dpi, they can differ across monitors
    HWND hTaskbar = FindWindowW(L"Shell_TrayWnd", NULL);

    if (hTaskbar) {

        typedef UINT (WINAPI *PFN_GetDpiForWindow)(HWND);
        HMODULE hUser32 = GetModuleHandleW(L"user32.dll");

        if (hUser32) {

            PFN_GetDpiForWindow pfn = (PFN_GetDpiForWindow)GetProcAddress(hUser32, "GetDpiForWindow");

            if (pfn) {
                return pfn(hTaskbar);
            }

        }

    }

    return 96; // fallback: assume 100% scaling
}

static HICON LoadScaledTrayIcon(void) {

    UINT dpi = GetCurrentTrayDpi();
    int cx = 16, cy = 16; // default at 16x16

    typedef int (WINAPI *PFN_GetSystemMetricsForDpi)(int, UINT);
    HMODULE hUser32 = GetModuleHandleW(L"user32.dll");

    if (hUser32) {

        PFN_GetSystemMetricsForDpi pfn = (PFN_GetSystemMetricsForDpi)GetProcAddress(hUser32, "GetSystemMetricsForDpi");

        if (pfn) { // SM_CXSMICON = "small icon" metric, what the tray actually uses (not SM_CXICON)
            cx = pfn(SM_CXSMICON, dpi);
            cy = pfn(SM_CYSMICON, dpi);
        }

    }

    // load from own resources
    return (HICON)LoadImageW(GetModuleHandleW(NULL), MAKEINTRESOURCEW(IDI_APP), IMAGE_ICON, cx, cy, LR_DEFAULTCOLOR);
}

// re-derive from the os, the power broadcasts may never have registered
void Tray_RefreshPowerFlags(void) {
    SYSTEM_POWER_STATUS sps;
    if (GetSystemPowerStatus(&sps)) {
        // 0 = battery, 1 = plugged in, 255 = unknown, treat unknown as AC
        g_bIsAC = (sps.ACLineStatus != 0);
        g_bBatterySaverActive = (sps.SystemStatusFlag == 1);
    }
}

// fills g_nid.szTip only, safe to call before the icon exists
static void FormatTooltipText(void) {

    // don't trust the cached flags, a failed registration would freeze them
    Tray_RefreshPowerFlags();

    // energy saver overrides the configured mode, so report what's actually in effect
    if (g_bBatterySaverActive) {
        StringCchPrintfW(g_nid.szTip, ARRAYSIZE(g_nid.szTip), L"Power Mode: Best Power Efficiency (Energy Saver Active)");
        return;
    }

    GUID mode = {0};
    DWORD status;

    if (g_bIsAC) {
        status = g_PowerSubsys.GetACMode(&mode);
    } else {
        status = g_PowerSubsys.GetDCMode(&mode);
    }

    // mode is a GUID, one of exactly three values, not an index
    const WCHAR* modeStr = L"Unknown";

    if (status == ERROR_SUCCESS) {
        if (IsEqualGUID(&mode, &GUID_POWER_MODE_BEST_EFFICIENCY)) {
            modeStr = L"Best Power Efficiency";
        } else if (IsEqualGUID(&mode, &GUID_POWER_MODE_NONE)) {
            modeStr = L"Balanced";
        } else if (IsEqualGUID(&mode, &GUID_POWER_MODE_BEST_PERFORMANCE)) {
            modeStr = L"Best Performance";
        }
    }

    // write unconditionally, a failed query must not leave stale text behind
    StringCchPrintfW(g_nid.szTip, ARRAYSIZE(g_nid.szTip), L"Power Mode: %s (%s)", modeStr, g_bIsAC ? L"Plugged in" : L"On Battery");
}

void Tray_Init(HWND hWnd) {

    // free the old handle before reloading
    if (g_nid.hIcon) {
        DestroyIcon(g_nid.hIcon);
    }

    // setup notifyiconw payload
    g_nid.cbSize = sizeof(g_nid);
    g_nid.hWnd = hWnd;
    g_nid.uID = ID_TRAY_ICON;
    g_nid.uCallbackMessage = WM_APP_TRAYMSG;
    g_nid.uFlags = (NIF_MESSAGE | NIF_ICON | NIF_TIP | NIF_SHOWTIP);

    g_nid.hIcon = LoadScaledTrayIcon();

    // populate szTip before NIM_ADD since we declare NIF_TIP here
    FormatTooltipText();

    // if the shell isn't accepting icons yet, TaskbarCreated brings us back
    if (!Shell_NotifyIconW(NIM_ADD, &g_nid)) {
        return;
    }

    // must add first, then upgrade
    g_nid.uVersion = NOTIFYICON_VERSION_4;
    Shell_NotifyIconW(NIM_SETVERSION, &g_nid);
}

void Tray_UpdateIcon(HWND hWnd) {

    // called on dpi/display change, old handle's size may no longer be right, reload it
    if (g_nid.hIcon) {
        DestroyIcon(g_nid.hIcon);
    }
    g_nid.hIcon = LoadScaledTrayIcon();

    g_nid.uFlags = NIF_ICON;
    Shell_NotifyIconW(NIM_MODIFY, &g_nid);
}

void Tray_UpdateTooltip(HWND hWnd) {
    FormatTooltipText();

    g_nid.uFlags = (NIF_TIP | NIF_SHOWTIP);
    Shell_NotifyIconW(NIM_MODIFY, &g_nid);
}

/**
 * Finds where the tray icon currently is on screen programmatically
 */
void Tray_GetAnchorPoint(HWND hWnd, POINT *pPt) {
    // Needed for when a second instance is opened which should open the menu.

    NOTIFYICONIDENTIFIER nidIdent = {0};
    nidIdent.cbSize = sizeof(NOTIFYICONIDENTIFIER);
    nidIdent.hWnd = hWnd;
    nidIdent.uID = ID_TRAY_ICON;

    RECT rcIcon;

    if (SUCCEEDED(Shell_NotifyIconGetRect(&nidIdent, &rcIcon))) {
        // top-right of the icon, so the menu expands up-and-left like a real click
        pPt->x = rcIcon.right;
        pPt->y = rcIcon.top;
        return;
    }

    // if getrect fails, fallback to putting it in corner
    HWND hTaskbar = FindWindowW(L"Shell_TrayWnd", NULL);
    HMONITOR hMon = MonitorFromWindow(hTaskbar ? hTaskbar : hWnd, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi = {0};
    mi.cbSize = sizeof(mi);

    if (GetMonitorInfoW(hMon, &mi)) {
        pPt->x = mi.rcWork.right;
        pPt->y = mi.rcWork.bottom;
    } else {
        GetCursorPos(pPt);
    }
}

void Tray_Cleanup(void) {

    // delete icon from tray
    Shell_NotifyIconW(NIM_DELETE, &g_nid);

    // NIM_DELETE removes it from the tray but doesn't free the icon handle itself
    DestroyIcon(g_nid.hIcon);
    g_nid.hIcon = NULL;
}