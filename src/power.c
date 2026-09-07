#include "power.h"

// initialise global struct memory
POWER_SUBSYSTEM_DISPATCH g_PowerSubsys = {0};

// Manually creating GUID to be notified when a device switches between plugged in power (AC) and battery power (DC)
// GUID_ACDC_POWER_SOURCE: 5d3e9a59-e9d5-4b00-a6bd-ff34ff516548
const GUID GUID_SRC_ACDC = { 0x5d3e9a59, 0xe9d5, 0x4b00, { 0xa6, 0xbd, 0xff, 0x34, 0xff, 0x51, 0x65, 0x48 } };

// Manualy create GUID to be notified whether Battery Saver is active/inactive
// GUID_BATTERY_SAVER_STATUS: e00958c0-c213-4ace-ac77-ec5dee0d5ea5
const GUID GUID_SAVER_STATUS = { 0xe00958c0, 0xc213, 0x4ace, { 0xac, 0x77, 0xec, 0x5d, 0xee, 0x0d, 0x5e, 0xa5 } };

/**
 * Loads pwrprof.dll and assigns Windows 11 power configuration functions
 */
BOOL PowerSubsystem_Init(BOOL bShowDialogOnFailure) {

    // load the power management profile dll
    g_PowerSubsys.hPowrProf = LoadLibraryExW(L"powrprof.dll", NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);

    if (!g_PowerSubsys.hPowrProf) {
        if (bShowDialogOnFailure) {
            MessageBoxW(
                NULL, 
                L"Failed to load powrprof.dll. This file should be in Windows 11's System32 folder.", 
                L"Error", 
                MB_OK | MB_ICONERROR);
        }

        PowerSubsystem_Shutdown();


        return FALSE;
    }

    // search for AC/DC getter/setters memory adresses and convert them to function pointers
    g_PowerSubsys.GetACMode = (PFN_PowerGetUserConfiguredACPowerMode) GetProcAddress(g_PowerSubsys.hPowrProf, "PowerGetUserConfiguredACPowerMode");
    g_PowerSubsys.GetDCMode = (PFN_PowerGetUserConfiguredDCPowerMode) GetProcAddress(g_PowerSubsys.hPowrProf, "PowerGetUserConfiguredDCPowerMode");
    g_PowerSubsys.SetACMode = (PFN_PowerSetUserConfiguredACPowerMode) GetProcAddress(g_PowerSubsys.hPowrProf, "PowerSetUserConfiguredACPowerMode");
    g_PowerSubsys.SetDCMode = (PFN_PowerSetUserConfiguredDCPowerMode) GetProcAddress(g_PowerSubsys.hPowrProf, "PowerSetUserConfiguredDCPowerMode");

    // if NULL, powrprof.dll is probably missing from System32
    if (!g_PowerSubsys.GetACMode || !g_PowerSubsys.GetDCMode || !g_PowerSubsys.SetACMode || !g_PowerSubsys.SetDCMode) {

        if (bShowDialogOnFailure) {
            MessageBoxW(
                NULL, 
                L"Required Windows 11 Power Mode APIs were not found in powrprof.dll.",
                L"Unsupported Operating System",
                MB_OK | MB_ICONERROR);
        }

        PowerSubsystem_Shutdown();

        return FALSE;
    }


    return TRUE;
}

/**
 * Releases powrprof.dll and resets power structure
 */
void PowerSubsystem_Shutdown(void) {

    if (g_PowerSubsys.hPowrProf) {
        FreeLibrary(g_PowerSubsys.hPowrProf);
        g_PowerSubsys.hPowrProf = NULL;
    }

    g_PowerSubsys = (POWER_SUBSYSTEM_DISPATCH){0};
}