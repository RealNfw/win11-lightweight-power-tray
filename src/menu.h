#ifndef MENU_H
#define MENU_H

#include <windows.h>

extern ULONGLONG g_uLastMenuDismissTime;

void ShowContextMenu(HWND hWnd, POINT pt);

#endif // MENU_H
