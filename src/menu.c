#include "power.h"
#include "tray.h"
#include "menu.h"
#include "resource.h"

ULONGLONG g_uLastMenuDismissTime = 0;

static BOOL HasPhysicalBattery(void) {
    SYSTEM_POWER_STATUS sps;
    if (!GetSystemPowerStatus(&sps)) return FALSE;

    // BATTERY_FLAG_NO_BATTERY (bit 7 = 128), explicit "no battery installed"
    if (sps.BatteryFlag != 255 && (sps.BatteryFlag & 128)) {
        return FALSE;
    }

    // desktops: both flag and percentage come back as "unknown" (255)
    if (sps.BatteryFlag == 255 && sps.BatteryLifePercent == 255) {
        return FALSE;
    }

    return TRUE;
}

// maps a mode GUID to its menu command id, or 0 if it isn't one of the three
static UINT PowerModeToCommandId(const GUID *pMode, UINT idEfficiency, UINT idBalanced, UINT idPerformance) {
    if (IsEqualGUID(pMode, &GUID_POWER_MODE_BEST_EFFICIENCY)) return idEfficiency;
    if (IsEqualGUID(pMode, &GUID_POWER_MODE_NONE))            return idBalanced;
    if (IsEqualGUID(pMode, &GUID_POWER_MODE_BEST_PERFORMANCE)) return idPerformance;
    return 0;
}

void ShowContextMenu(HWND hWnd, POINT pt) {
    HMENU hMenu = CreatePopupMenu();
    if (!hMenu) return;

    // query live, not cached, the user could've changed this in settings
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

    // header rows use id 0 and are grayed, so they're inert labels
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

    // CheckMenuRadioItem needs the id range contiguous, see resource.h
    UINT acCheckId = PowerModeToCommandId(&acMode, IDM_AC_EFFICIENCY, IDM_AC_BALANCED, IDM_AC_PERFORMANCE);
    if (bACSupported) {
        // an unrecognised oem mode leaves no dot, but the modes stay selectable
        if (acCheckId != 0) {
            CheckMenuRadioItem(hMenu, IDM_AC_EFFICIENCY, IDM_AC_PERFORMANCE, acCheckId, MF_BYCOMMAND);
        }
    } else {
        for (UINT id = IDM_AC_EFFICIENCY; id <= IDM_AC_PERFORMANCE; id++) {
            EnableMenuItem(hMenu, id, MF_BYCOMMAND | MF_GRAYED | MF_DISABLED);
        }
    }

    // DC also needs a physical battery, not just API support
    BOOL bHasBattery = HasPhysicalBattery();
    UINT dcCheckId = PowerModeToCommandId(&dcMode, IDM_DC_EFFICIENCY, IDM_DC_BALANCED, IDM_DC_PERFORMANCE);
    if (bDCSupported && bHasBattery) {
        // same here, capability decides enablement, the guid only decides the dot
        if (dcCheckId != 0) {
            CheckMenuRadioItem(hMenu, IDM_DC_EFFICIENCY, IDM_DC_PERFORMANCE, dcCheckId, MF_BYCOMMAND);
        }
    } else {
        for (UINT id = IDM_DC_EFFICIENCY; id <= IDM_DC_PERFORMANCE; id++) {
            EnableMenuItem(hMenu, id, MF_BYCOMMAND | MF_GRAYED | MF_DISABLED);
        }
    }

    // our hidden window is never naturally foreground, but TrackPopupMenu needs it to be
    SetForegroundWindow(hWnd);

    // anchors the menu's bottom-right at pt, right button can select items too
    TrackPopupMenuEx(hMenu, TPM_RIGHTALIGN | TPM_BOTTOMALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, hWnd, NULL);

    // without this the menu can stick open when dismissed by clicking away
    PostMessageW(hWnd, WM_NULL, 0, 0);

    DestroyMenu(hMenu);

    // feeds main.c's 150ms debounce so the dismissing click doesn't reopen it
    g_uLastMenuDismissTime = GetTickCount64();
}
