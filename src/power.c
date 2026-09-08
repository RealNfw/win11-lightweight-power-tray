#include "power.h"

// initialise global struct memory
POWER_SUBSYSTEM_DISPATCH g_PowerSubsys = {0};

// Manually creating GUID to be notified when a device switches between plugged in power (AC) and battery power (DC)
// GUID_ACDC_POWER_SOURCE: 5d3e9a59-e9d5-4b00-a6bd-ff34ff516548
const GUID GUID_SRC_ACDC = { 0x5d3e9a59, 0xe9d5, 0x4b00, { 0xa6, 0xbd, 0xff, 0x34, 0xff, 0x51, 0x65, 0x48 } };

// Manualy create GUID to be notified whether Battery Saver is active/inactive
// GUID_POWER_SAVING_STATUS: e00958c0-c213-4ace-ac77-fecced2eeea5
const GUID GUID_SAVER_STATUS = { 0xe00958c0, 0xc213, 0x4ace, { 0xac, 0x77, 0xfe, 0xcc, 0xed, 0x2e, 0xee, 0xa5 } };

// The three real Windows 11 power mode values (PowerGetUserConfiguredACPowerMode's
// output/PowerSetUserConfiguredACPowerMode's input GUID) -- not a DWORD index.
// GUID_POWER_MODE_BEST_EFFICIENCY: 961cc777-2547-4f9d-8174-7d86181b8a7a
const GUID GUID_POWER_MODE_BEST_EFFICIENCY = { 0x961cc777, 0x2547, 0x4f9d, { 0x81, 0x74, 0x7d, 0x86, 0x18, 0x1b, 0x8a, 0x7a } };
// GUID_POWER_MODE_NONE ("Balanced"): 00000000-0000-0000-0000-000000000000
const GUID GUID_POWER_MODE_NONE = { 0x00000000, 0x0000, 0x0000, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } };
// GUID_POWER_MODE_BEST_PERFORMANCE: ded574b5-45a0-4f42-8737-46345c09c238
const GUID GUID_POWER_MODE_BEST_PERFORMANCE = { 0xded574b5, 0x45a0, 0x4f42, { 0x87, 0x37, 0x46, 0x34, 0x5c, 0x09, 0xc2, 0x38 } };

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