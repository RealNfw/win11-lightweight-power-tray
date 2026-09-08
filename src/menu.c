#include "power.h"
#include "tray.h"
#include "menu.h"
#include "resource.h"

ULONGLONG g_uLastMenuDismissTime = 0;

static BOOL HasPhysicalBattery(void) {
    SYSTEM_POWER_STATUS sps;
    if (!GetSystemPowerStatus(&sps)) return FALSE;

    // BATTERY_FLAG_NO_BATTERY (bit 7 = 128) -- explicit "no battery installed"
    if (sps.BatteryFlag != 255 && (sps.BatteryFlag & 128)) {
        return FALSE;
    }

    // desktops: both flag and percentage come back as "unknown" (255)
    if (sps.BatteryFlag == 255 && sps.BatteryLifePercent == 255) {
        return FALSE;
    }

    return TRUE;
}

// mode is a GUID, one of exactly three values -- maps it to the matching
// menu command id for CheckMenuRadioItem, or 0 if it's not one of the three
static UINT PowerModeToCommandId(const GUID *pMode, UINT idEfficiency, UINT idBalanced, UINT idPerformance) {
    if (IsEqualGUID(pMode, &GUID_POWER_MODE_BEST_EFFICIENCY)) return idEfficiency;
    if (IsEqualGUID(pMode, &GUID_POWER_MODE_NONE))            return idBalanced;
    if (IsEqualGUID(pMode, &GUID_POWER_MODE_BEST_PERFORMANCE)) return idPerformance;
    return 0;
}

void ShowContextMenu(HWND hWnd, POINT pt) {
    HMENU hMenu = CreatePopupMenu();
    if (!hMenu) return;

    // JIT: query live, not cached -- the user could've changed this in Settings
    GUID acMode = {0};
    GUID dcMode = {0};
    BOOL bACSupported = (g_PowerSubsys.GetACMode(&acMode) == ERROR_SUCCESS);
    BOOL bDCSupported = (g_PowerSubsys.GetDCMode(&dcMode) == ERROR_SUCCESS);

    // if neither works at all, say so up front instead of showing two dead sections
    if (!bACSupported && !bDCSupported) {
        InsertMenuW(hMenu, (UINT)-1,MF_BYPOSITION | MF_STRING | MF_GRAYED | MF_DISABLED, 0,
                    L"Power Modes Unavailable on Current Scheme");
        InsertMenuW(hMenu, (UINT)-1,MF_BYPOSITION | MF_SEPARATOR, 0, NULL);
    }

    if (g_bBatterySaverActive) {
        InsertMenuW(hMenu, (UINT)-1,MF_BYPOSITION | MF_STRING | MF_GRAYED | MF_DISABLED, 0,
                    L"Energy Saver is currently active");
        InsertMenuW(hMenu, (UINT)-1,MF_BYPOSITION | MF_SEPARATOR, 0, NULL);
    }

    // "header" rows: id 0, grayed+disabled, so they're inert labels, not clickable items
    InsertMenuW(hMenu, (UINT)-1,MF_BYPOSITION | MF_STRING | MF_GRAYED | MF_DISABLED, 0, L"Plugged In (AC)");
    InsertMenuW(hMenu, (UINT)-1,MF_BYPOSITION | MF_STRING, IDM_AC_EFFICIENCY,  L"  Best Power Efficiency");
    InsertMenuW(hMenu, (UINT)-1,MF_BYPOSITION | MF_STRING, IDM_AC_BALANCED,    L"  Balanced");
    InsertMenuW(hMenu, (UINT)-1,MF_BYPOSITION | MF_STRING, IDM_AC_PERFORMANCE, L"  Best Performance");

    InsertMenuW(hMenu, (UINT)-1,MF_BYPOSITION | MF_SEPARATOR, 0, NULL);

    InsertMenuW(hMenu, (UINT)-1,MF_BYPOSITION | MF_STRING | MF_GRAYED | MF_DISABLED, 0, L"On Battery (DC)");
    InsertMenuW(hMenu, (UINT)-1,MF_BYPOSITION | MF_STRING, IDM_DC_EFFICIENCY,  L"  Best Power Efficiency");
    InsertMenuW(hMenu, (UINT)-1,MF_BYPOSITION | MF_STRING, IDM_DC_BALANCED,    L"  Balanced");
    InsertMenuW(hMenu, (UINT)-1,MF_BYPOSITION | MF_STRING, IDM_DC_PERFORMANCE, L"  Best Performance");

    InsertMenuW(hMenu, (UINT)-1,MF_BYPOSITION | MF_SEPARATOR, 0, NULL);
    InsertMenuW(hMenu, (UINT)-1,MF_BYPOSITION | MF_STRING, IDM_EXIT, L"Exit");

    // CheckMenuRadioItem needs the whole id range contiguous -- that's why
    // resource.h keeps IDM_AC_* and IDM_DC_* as two unbroken blocks
    UINT acCheckId = PowerModeToCommandId(&acMode, IDM_AC_EFFICIENCY, IDM_AC_BALANCED, IDM_AC_PERFORMANCE);
    if (bACSupported && acCheckId != 0) {
        CheckMenuRadioItem(hMenu, IDM_AC_EFFICIENCY, IDM_AC_PERFORMANCE, acCheckId, MF_BYCOMMAND);
    } else {
        for (UINT id = IDM_AC_EFFICIENCY; id <= IDM_AC_PERFORMANCE; id++) {
            EnableMenuItem(hMenu, id, MF_BYCOMMAND | MF_GRAYED | MF_DISABLED);
        }
    }

    // DC also needs a physical battery, not just API support -- desktops fail this
    BOOL bHasBattery = HasPhysicalBattery();
    UINT dcCheckId = PowerModeToCommandId(&dcMode, IDM_DC_EFFICIENCY, IDM_DC_BALANCED, IDM_DC_PERFORMANCE);
    if (bDCSupported && bHasBattery && dcCheckId != 0) {
        CheckMenuRadioItem(hMenu, IDM_DC_EFFICIENCY, IDM_DC_PERFORMANCE, dcCheckId, MF_BYCOMMAND);
    } else {
        for (UINT id = IDM_DC_EFFICIENCY; id <= IDM_DC_PERFORMANCE; id++) {
            EnableMenuItem(hMenu, id, MF_BYCOMMAND | MF_GRAYED | MF_DISABLED);
        }
    }

    // our window is hidden and never naturally the foreground window --
    // TrackPopupMenu needs it to be, or the menu can fail to dismiss correctly
    SetForegroundWindow(hWnd);

    // TPM_RIGHTALIGN | TPM_BOTTOMALIGN anchors the menu's bottom-right corner
    // at pt -- pairs with Tray_GetAnchorPoint's {rcIcon.right, rcIcon.top}
    // TPM_RIGHTBUTTON additionally lets the right button select items, so a
    // right-click-drag-release picks an entry the same way the left one does
    TrackPopupMenuEx(hMenu, TPM_RIGHTALIGN | TPM_BOTTOMALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, hWnd, NULL);

    // documented workaround: without this harmless message, clicking away to
    // dismiss the menu (instead of picking an item) can leave it stuck open
    PostMessageW(hWnd, WM_NULL, 0, 0);

    DestroyMenu(hMenu);

    // feeds main.c's 150ms debounce so the dismissing click doesn't reopen it
    g_uLastMenuDismissTime = GetTickCount64();
}
