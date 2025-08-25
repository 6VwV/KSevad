#include <ntifs.h>
#include <ntddk.h>

#define DRIVER_PREFIX         "[KSevad] "
#define DRIVER_DEVICE_NAME    L"\\Device\\KSevadDevice"
#define DRIVER_SYMBOLIC_LINK  L"\\??\\KSevadDevice"
#define TARGET_PROCESS        "al-khaser.exe"

const wchar_t* blacklist[] = {
    L"\\Device\\vmci",
};

extern "C" UCHAR* PsGetProcessImageFileName(PEPROCESS Process);

PDRIVER_DISPATCH OriginalCreateHandler[ARRAYSIZE(blacklist)] = { nullptr };
PDEVICE_OBJECT g_DeviceObject = nullptr;

NTSTATUS HookedCreate(PDEVICE_OBJECT DeviceObject, PIRP Irp) {
    PEPROCESS process = PsGetCurrentProcess();
    const char* imageName = (const char*)PsGetProcessImageFileName(process);

    if (_stricmp(imageName, TARGET_PROCESS) == 0) {
        DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL,
            DRIVER_PREFIX "Blocking IRP_MJ_CREATE for %s to %wZ\n", imageName, &DeviceObject->DriverObject->DriverName);

        Irp->IoStatus.Status = STATUS_OBJECT_NAME_NOT_FOUND;
        Irp->IoStatus.Information = 0;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return STATUS_OBJECT_NAME_NOT_FOUND;
    }
    return OriginalCreateHandler[0](DeviceObject, Irp);
}

NTSTATUS InstallDeviceHook() {
    NTSTATUS lastStatus = STATUS_SUCCESS;
    for (int i = 0; i < ARRAYSIZE(blacklist); ++i) {
        UNICODE_STRING deviceName;
        RtlInitUnicodeString(&deviceName, blacklist[i]);
        PFILE_OBJECT fileObject = nullptr;
        PDEVICE_OBJECT deviceObject = nullptr;

        NTSTATUS status = IoGetDeviceObjectPointer(&deviceName, FILE_READ_DATA, &fileObject, &deviceObject);
        if (!NT_SUCCESS(status)) {
            DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL,
                DRIVER_PREFIX "Failed to get device object for %wZ (0x%08X)\n", &deviceName, status);
            lastStatus = status;
            continue;
        }
        PDRIVER_OBJECT targetDriver = deviceObject->DriverObject;
        OriginalCreateHandler[i] = targetDriver->MajorFunction[IRP_MJ_CREATE];
        InterlockedExchangePointer(
            (PVOID*)&targetDriver->MajorFunction[IRP_MJ_CREATE],
            (PVOID)HookedCreate
        );
        ObDereferenceObject(fileObject);
    }
    return lastStatus;
}

NTSTATUS UninstallDeviceHook() {
    for (int i = 0; i < ARRAYSIZE(blacklist); ++i) {
        if (OriginalCreateHandler[i] == nullptr) continue;

        UNICODE_STRING deviceName;
        RtlInitUnicodeString(&deviceName, blacklist[i]);
        PFILE_OBJECT fileObject = nullptr;
        PDEVICE_OBJECT deviceObject = nullptr;

        NTSTATUS status = IoGetDeviceObjectPointer(&deviceName, FILE_READ_DATA, &fileObject, &deviceObject);
        if (NT_SUCCESS(status)) {
            InterlockedExchangePointer((PVOID*)&deviceObject->DriverObject->MajorFunction[IRP_MJ_CREATE],
                (PVOID)OriginalCreateHandler[i]);
            ObDereferenceObject(fileObject);
        }
        OriginalCreateHandler[i] = nullptr;
    }
    return STATUS_SUCCESS;
}

void MyUnload(PDRIVER_OBJECT DriverObject) {
    UNREFERENCED_PARAMETER(DriverObject);
    DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, DRIVER_PREFIX "Unloading...\n");
    UninstallDeviceHook();
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
    DriverObject->DriverUnload = MyUnload;
    status = InstallDeviceHook();
    if (!NT_SUCCESS(status)) {
        DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, DRIVER_PREFIX "Failed to install device hook: (0x%08X)\n", status);
        IoDeleteSymbolicLink(&symbolicLink);
        IoDeleteDevice(g_DeviceObject);
        return status;
    }
    DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, DRIVER_PREFIX "Driver loaded successfully.\n");
    return STATUS_SUCCESS;
}