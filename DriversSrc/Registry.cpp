#include <ntifs.h>

#define DRIVER_PREFIX         "[KSevad] "
#define DRIVER_DEVICE_NAME    L"\\Device\\KSevadRegistry"
#define DRIVER_SYMBOLIC_LINK  L"\\??\\KSevadRegistry"
#define REG_CALLBACK_ALTITUDE L"31102.0003"
#define TARGET_PROCESS        "al-khaser.exe"
#define TARGET_PROCESS2       "vmaware.exe"

// Blacklisted registry
const wchar_t* blacklist[] = {
            L"VMware Tools",
            L"vmdebug",
            L"VMTools",
            L"VMMEMCTL",
            L"CdRomNECVMWar_VMware_IDE_CD",
            L"CdRomNECVMWar_VMware_SATA_CD",
            L"SystemInformation",
            L"Logical Unit Id 0",
            L"Enum",
            L"0000",
            L"Video"
};

#ifndef PsGetProcessImageFileName
extern "C" PCHAR PsGetProcessImageFileName(_In_ PEPROCESS Process);
#endif

LARGE_INTEGER g_RegContext = {};
PDEVICE_OBJECT g_DeviceObject = nullptr;

NTSTATUS RegNotify(PVOID context, PVOID Argument1, PVOID Argument2) {
    UNREFERENCED_PARAMETER(context);
    NTSTATUS status = STATUS_SUCCESS;

    switch ((REG_NOTIFY_CLASS)(ULONG_PTR)Argument1) {
    case RegNtPreOpenKeyEx: {
        PEPROCESS proc = PsGetCurrentProcess();
        CHAR       imageName[16] = { 0 };
        RtlCopyMemory(imageName, PsGetProcessImageFileName(proc), sizeof(imageName) - 1);

        if (_stricmp(imageName, TARGET_PROCESS) != 0) {
            if (_stricmp(imageName, TARGET_PROCESS2) != 0) {
            break;
            }
        }

        auto* info = (REG_PRE_OPEN_KEY_INFORMATION*)Argument2;
        if (info && info->CompleteName && info->CompleteName->Buffer) {
            DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, DRIVER_PREFIX "PreOpenKeyEx: %wZ\n", info->CompleteName);

            for (int i = 0; i < ARRAYSIZE(blacklist); ++i) {

                if (wcsstr(info->CompleteName->Buffer, blacklist[i])) {
                    DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, DRIVER_PREFIX "Blocked registry key open: %wZ (matched: %ws)\n", info->CompleteName, blacklist[i]);
                    return STATUS_ACCESS_DENIED;
                }
            }
        }
        break;
    }
    }
    return status;
}

void MyUnload(PDRIVER_OBJECT DriverObject) {
    UNREFERENCED_PARAMETER(DriverObject);
    DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, DRIVER_PREFIX "Unloading...\n");

    if (g_RegContext.QuadPart != 0) {
        NTSTATUS status = CmUnRegisterCallback(g_RegContext);
        if (!NT_SUCCESS(status)) {
            DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, DRIVER_PREFIX "Failed to unregister registry callbacks: (0x%08X)\n", status);
        }
    }

    UNICODE_STRING symbolicLink = RTL_CONSTANT_STRING(DRIVER_SYMBOLIC_LINK);
    IoDeleteSymbolicLink(&symbolicLink);

    if (g_DeviceObject) {
        IoDeleteDevice(g_DeviceObject);
    }
}

extern "C"
NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath) {
    UNREFERENCED_PARAMETER(RegistryPath);

    NTSTATUS status = STATUS_SUCCESS;
    UNICODE_STRING deviceName = RTL_CONSTANT_STRING(DRIVER_DEVICE_NAME);
    UNICODE_STRING symbolicLink = RTL_CONSTANT_STRING(DRIVER_SYMBOLIC_LINK);
    UNICODE_STRING regAltitude = RTL_CONSTANT_STRING(REG_CALLBACK_ALTITUDE);
    status = IoCreateDevice(DriverObject, 0, &deviceName, FILE_DEVICE_UNKNOWN, 0, FALSE, &g_DeviceObject);

    if (!NT_SUCCESS(status)) {
        DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, DRIVER_PREFIX "Failed to create device: (0x%08X)\n", status);
        return status;
    }

    status = IoCreateSymbolicLink(&symbolicLink, &deviceName);
    if (!NT_SUCCESS(status)) {
        DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, DRIVER_PREFIX "Failed to create symbolic link: (0x%08X)\n", status);
        IoDeleteDevice(g_DeviceObject);
        return status;
    }
    status = CmRegisterCallbackEx(RegNotify, &regAltitude, DriverObject, nullptr, &g_RegContext, nullptr);

    if (!NT_SUCCESS(status)) {
        DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, DRIVER_PREFIX "Failed to register registry callback: (0x%08X)\n", status);
        IoDeleteSymbolicLink(&symbolicLink);
        IoDeleteDevice(g_DeviceObject);
        return status;
    }
    DriverObject->DriverUnload = MyUnload;
    DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, DRIVER_PREFIX "Driver loaded successfully.\n");
    return STATUS_SUCCESS;
}