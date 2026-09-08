#ifndef TRAY_H
#define TRAY_H

#include <windows.h>

#define WM_APP_TRAYMSG       (WM_APP + 1)
#define WM_APP_RESHOW_TRAY   (WM_APP + 2)
#define ID_TRAY_ICON         1

extern NOTIFYICONDATAW g_nid;
extern BOOL g_bIsAC;
extern BOOL g_bBatterySaverActive;

void Tray_Init(HWND hWnd);
void Tray_UpdateIcon(HWND hWnd);
void Tray_UpdateTooltip(HWND hWnd);
void Tray_GetAnchorPoint(HWND hWnd, POINT *pPt);
void Tray_Cleanup(void);

#endif // TRAY_H
