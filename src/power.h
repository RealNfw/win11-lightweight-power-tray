#ifndef POWER_H
#define POWER_H

#include <windows.h>

extern const GUID GUID_POWER_MODE_BEST_EFFICIENCY;
extern const GUID GUID_POWER_MODE_NONE; // Balanced
extern const GUID GUID_POWER_MODE_BEST_PERFORMANCE;

typedef DWORD (WINAPI *PFN_PowerGetUserConfiguredACPowerMode)(GUID *pMode);
typedef DWORD (WINAPI *PFN_PowerGetUserConfiguredDCPowerMode)(GUID *pMode);
typedef DWORD (WINAPI *PFN_PowerSetUserConfiguredACPowerMode)(const GUID *pMode);
typedef DWORD (WINAPI *PFN_PowerSetUserConfiguredDCPowerMode)(const GUID *pMode);

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
