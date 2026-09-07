#ifndef POWER_H
#define POWER_H

#include <windows.h>

#define WIN11_POWER_MODE_INVALID ((DWORD)0xFFFFFFFF)

typedef enum _WIN11_POWER_MODE {
    WIN11_POWER_MODE_EFFICIENCY  = 0,
    WIN11_POWER_MODE_BALANCED    = 1,
    WIN11_POWER_MODE_PERFORMANCE = 2
} WIN11_POWER_MODE;

typedef DWORD (WINAPI *PFN_PowerGetUserConfiguredACPowerMode)(DWORD *pMode);
typedef DWORD (WINAPI *PFN_PowerGetUserConfiguredDCPowerMode)(DWORD *pMode);
typedef DWORD (WINAPI *PFN_PowerSetUserConfiguredACPowerMode)(DWORD Mode);
typedef DWORD (WINAPI *PFN_PowerSetUserConfiguredDCPowerMode)(DWORD Mode);

typedef struct _POWER_SUBSYSTEM_DISPATCH {
    HMODULE hPowrProf;
    PFN_PowerGetUserConfiguredACPowerMode GetACMode;
    PFN_PowerGetUserConfiguredDCPowerMode GetDCMode;
    PFN_PowerSetUserConfiguredACPowerMode SetACMode;
    PFN_PowerSetUserConfiguredDCPowerMode SetDCMode;
} POWER_SUBSYSTEM_DISPATCH;

extern POWER_SUBSYSTEM_DISPATCH g_PowerSubsys;

extern const GUID GUID_SRC_ACDC;
extern const GUID GUID_SAVER_STATUS;

BOOL PowerSubsystem_Init(BOOL bShowDialogOnFailure);
void PowerSubsystem_Shutdown(void);

#endif // POWER_H
