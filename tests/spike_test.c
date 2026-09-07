// Verifies that the PowrProf power mode subsystem (in src/power.c) resolves and behaves correctly against the real, running OS.

#include "power.h"
#include <stdio.h>
#include <wchar.h>

#define WIN11_MIN_BUILD 22000

// Mirrors RTL_OSVERSIONINFOEXW (winternl.h), declared locally.
typedef struct _SPIKE_OSVERSIONINFOEXW {
    ULONG  dwOSVersionInfoSize;
    ULONG  dwMajorVersion;
    ULONG  dwMinorVersion;
    ULONG  dwBuildNumber;
    ULONG  dwPlatformId;
    WCHAR  szCSDVersion[128];
    USHORT wServicePackMajor;
    USHORT wServicePackMinor;
    USHORT wSuiteMask;
    UCHAR  wProductType;
    UCHAR  wReserved;
} SPIKE_OSVERSIONINFOEXW;

typedef LONG (WINAPI *PFN_RtlGetVersion)(SPIKE_OSVERSIONINFOEXW *);

// Reads the true kernel version/build via ntdll!RtlGetVersion
static BOOL GetTrueOSVersion(SPIKE_OSVERSIONINFOEXW *pInfo) {

    ZeroMemory(pInfo, sizeof(*pInfo));
    pInfo->dwOSVersionInfoSize = sizeof(*pInfo);

    HMODULE hNtdll = GetModuleHandleW(L"ntdll.dll");
    if (!hNtdll) {
        return FALSE;
    }

    PFN_RtlGetVersion pRtlGetVersion = (PFN_RtlGetVersion)GetProcAddress(hNtdll, "RtlGetVersion");
    if (!pRtlGetVersion) {
        return FALSE;
    }

    return pRtlGetVersion(pInfo) == 0; // STATUS_SUCCESS
}

int wmain(int argc, wchar_t *argv[]) {
    UNREFERENCED_PARAMETER(argc);
    UNREFERENCED_PARAMETER(argv);

    if (PowerSubsystem_Init(FALSE)) {
        DWORD acMode = WIN11_POWER_MODE_INVALID;
        DWORD dcMode = WIN11_POWER_MODE_INVALID;
        DWORD acStatus = g_PowerSubsys.GetACMode(&acMode);
        DWORD dcStatus = g_PowerSubsys.GetDCMode(&dcMode);

        PowerSubsystem_Shutdown();

        if (acStatus != ERROR_SUCCESS || dcStatus != ERROR_SUCCESS) {
            fwprintf(stderr,
                L"FAIL: PowerGetUserConfigured{AC,DC}PowerMode returned an error "
                L"(AC status=%lu, DC status=%lu).\n", acStatus, dcStatus);
            return 1;
        }

        wprintf(L"PASS: AC power mode = %lu, DC power mode = %lu\n", acMode, dcMode);
        return 0;
    }

    // Init failed -- determine whether that is expected for this OS.
    SPIKE_OSVERSIONINFOEXW osInfo;
    if (!GetTrueOSVersion(&osInfo)) {

        fwprintf(stderr,
            L"FAIL: PowerSubsystem_Init failed and RtlGetVersion could not be resolved.\n");

        return 1;
    }

    BOOL bIsClient = (osInfo.wProductType == VER_NT_WORKSTATION);
    BOOL bIsWin11OrNewer = (osInfo.dwMajorVersion > 10) || (osInfo.dwMajorVersion == 10 && osInfo.dwBuildNumber >= WIN11_MIN_BUILD);

    if (bIsClient && bIsWin11OrNewer) {

        fwprintf(stderr,
            L"FAIL: Running on a Windows 11+ client (build %lu), but the PowrProf "
            L"power mode APIs are unavailable.\n", osInfo.dwBuildNumber);
            
        return 1;
    }

    wprintf(L"SKIP: PowrProf power mode APIs unavailable on this OS "
            L"(productType=%u, build=%lu) -- expected on Windows Server or "
            L"down-level Windows clients.\n",
            (unsigned)osInfo.wProductType, osInfo.dwBuildNumber);

    return 0;
}
