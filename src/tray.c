#include "power.h"
#include "tray.h"
#include "menu.h"
#include "resource.h"
#include <strsafe.h>

NOTIFYICONDATAW g_nid = {0};
BOOL g_bIsAC = TRUE;
BOOL g_bBatterySaverActive = FALSE;

static UINT GetCurrentTrayDpi(void) {

    // taskbar's own dpi, not the app's -- they can differ across monitors
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

// fills g_nid.szTip only -- doesn't talk to the shell, so it's safe to call
// before the icon even exists yet (Tray_Init) as well as after (Tray_UpdateTooltip)
static void FormatTooltipText(void) {
    GUID mode = {0};
    DWORD status;

    if (g_bIsAC) {
        status = g_PowerSubsys.GetACMode(&mode);
    } else {
        status = g_PowerSubsys.GetDCMode(&mode);
    }

    if (status == ERROR_SUCCESS) {

        // mode is a GUID, one of exactly three values -- not an index to look up
        const WCHAR* modeStr;
        if (IsEqualGUID(&mode, &GUID_POWER_MODE_BEST_EFFICIENCY)) {
            modeStr = L"Best Power Efficiency";
        } else if (IsEqualGUID(&mode, &GUID_POWER_MODE_NONE)) {
            modeStr = L"Balanced";
        } else if (IsEqualGUID(&mode, &GUID_POWER_MODE_BEST_PERFORMANCE)) {
            modeStr = L"Best Performance";
        } else {
            modeStr = L"Unknown";
        }

        // write tooltip message in g_nid.sztip
        StringCchPrintfW(g_nid.szTip, ARRAYSIZE(g_nid.szTip), L"Power Mode: %s (%s)", modeStr, g_bIsAC ? L"Plugged in" : L"On Battery");
    }
}

void Tray_Init(HWND hWnd) {

    // setup notifyiconw payload
    // cbSize is how the shell picks which NOTIFYICONDATA layout/behaviour to use.
    // Leaving it 0 makes Shell_NotifyIconW still return TRUE, but the icon stays
    // on legacy (pre-v4) semantics -- NIM_SETVERSION is silently ignored and
    // clicks arrive as raw WM_LBUTTONUP/WM_RBUTTONUP instead of NIN_SELECT/WM_CONTEXTMENU.
    g_nid.cbSize = sizeof(g_nid);
    g_nid.hWnd = hWnd;
    g_nid.uID = ID_TRAY_ICON;
    g_nid.uCallbackMessage = WM_APP_TRAYMSG;
    g_nid.uFlags = (NIF_MESSAGE | NIF_ICON | NIF_TIP | NIF_SHOWTIP);

    g_nid.hIcon = LoadScaledTrayIcon();

    // populate szTip before NIM_ADD -- we declare NIF_TIP right here, so the
    // text should already be correct at the moment the icon is born, not
    // patched in via a second call right after
    FormatTooltipText();

    Shell_NotifyIconW(NIM_ADD, &g_nid);

    // must add first, then upgrade -- can't create an icon already at v4
    g_nid.uVersion = NOTIFYICON_VERSION_4;
    Shell_NotifyIconW(NIM_SETVERSION, &g_nid);
}

void Tray_UpdateIcon(HWND hWnd) {

    // called on dpi/display change -- old handle's size may no longer be right, reload it
    DestroyIcon(g_nid.hIcon);
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
        // top-right of the icon -- pairs with menu.c's TPM_RIGHTALIGN | TPM_BOTTOMALIGN
        // so the menu expands up-and-left from here, same as a real click would
        pPt->x = rcIcon.right;
        pPt->y = rcIcon.top;
        return;
    }

    // if getrect fails, fallback to putting it in corner
    HWND hTaskbar = FindWindowW(L"Shell_TrayWnd", NULL);
    HMONITOR hMon = MonitorFromWindow(hTaskbar ? hTaskbar : hWnd, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi = {sizeof(MONITORINFO)};

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