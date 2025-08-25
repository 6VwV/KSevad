#include <ntifs.h>
#include <ntddk.h>

#define DRIVER_PREFIX         "[KSevad] "
#define DRIVER_DEVICE_NAME    L"\\Device\\KSevadFile"
#define DRIVER_SYMBOLIC_LINK  L"\\??\\KSevadFile"
#define TARGET_PROCESS        "al-khaser.exe"

// Blacklisted files
const wchar_t* g_FileBlacklist[] = {
    L"\\System32\\drivers\\vmmouse.sys",
    L"\\System32\\drivers\\vmhgfs.sys",
    L"\\System32\\drivers\\vmci.sys",
    L"\\System32\\drivers\\vmmemctl.sys",
    L"\\System32\\drivers\\vmrawdsk.sys",
    L"\\System32\\drivers\\vmusbmouse.sys",
};

extern "C" UCHAR* PsGetProcessImageFileName(PEPROCESS Process);

extern "C" POBJECT_TYPE* IoDriverObjectType;

extern "C" NTSTATUS ObReferenceObjectByName(
    PUNICODE_STRING ObjectName,
    ULONG Attributes,
    PACCESS_STATE AccessState,
    ACCESS_MASK DesiredAccess,
    POBJECT_TYPE ObjectType,
    KPROCESSOR_MODE AccessMode,
    PVOID ParseContext,
    PVOID* Object);

PDRIVER_DISPATCH OriginalNtfsCreate = nullptr;
PDEVICE_OBJECT g_DeviceObject = nullptr;


bool IsBlacklistedFile(PUNICODE_STRING fileName) {
    for (int i = 0; i < ARRAYSIZE(g_FileBlacklist); ++i) {
        if (wcsstr(fileName->Buffer, g_FileBlacklist[i])) {
            return true;
        }
    }
    return false;
}

NTSTATUS HookedNtfsCreate(PDEVICE_OBJECT DeviceObject, PIRP Irp) {
    PIO_STACK_LOCATION irpSp = IoGetCurrentIrpStackLocation(Irp);
    PEPROCESS process = PsGetCurrentProcess();
    const char* imageName = (const char*)PsGetProcessImageFileName(process);

    if (irpSp && irpSp->FileObject && imageName && _stricmp(imageName, TARGET_PROCESS) == 0) {
        PUNICODE_STRING fileName = &irpSp->FileObject->FileName;
        if (fileName && fileName->Buffer && IsBlacklistedFile(fileName)) {
            DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, DRIVER_PREFIX "Blocking file access to %wZ from %s\n", fileName, imageName);
            Irp->IoStatus.Status = STATUS_OBJECT_NAME_NOT_FOUND;
            Irp->IoStatus.Information = 0;
            IoCompleteRequest(Irp, IO_NO_INCREMENT);
            return STATUS_OBJECT_NAME_NOT_FOUND;
        }
    }
    return OriginalNtfsCreate(DeviceObject, Irp);
}

NTSTATUS InstallNtfsHook() {
    UNICODE_STRING ntfsName;
    PDRIVER_OBJECT ntfsDriverObject = nullptr;
    RtlInitUnicodeString(&ntfsName, L"\\FileSystem\\NTFS");
    NTSTATUS status = ObReferenceObjectByName(&ntfsName, OBJ_CASE_INSENSITIVE, nullptr, 0,
        *IoDriverObjectType, KernelMode, nullptr, (PVOID*)&ntfsDriverObject);
    if (!NT_SUCCESS(status)) {
        DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, DRIVER_PREFIX "Failed to get NTFS driver object (0x%08X)\n", status);
        return status;
    }
    OriginalNtfsCreate = ntfsDriverObject->MajorFunction[IRP_MJ_CREATE];
    InterlockedExchangePointer((PVOID*)&ntfsDriverObject->MajorFunction[IRP_MJ_CREATE], (PVOID)HookedNtfsCreate);
    ObDereferenceObject(ntfsDriverObject);
    return STATUS_SUCCESS;
}

NTSTATUS UninstallNtfsHook() {
    UNICODE_STRING ntfsName;
    PDRIVER_OBJECT ntfsDriverObject = nullptr;
    RtlInitUnicodeString(&ntfsName, L"\\FileSystem\\NTFS");
    NTSTATUS status = ObReferenceObjectByName(&ntfsName, OBJ_CASE_INSENSITIVE, nullptr, 0,
        *IoDriverObjectType, KernelMode, nullptr, (PVOID*)&ntfsDriverObject);
    if (NT_SUCCESS(status) && OriginalNtfsCreate != nullptr) {
        InterlockedExchangePointer((PVOID*)&ntfsDriverObject->MajorFunction[IRP_MJ_CREATE], (PVOID)OriginalNtfsCreate);
        ObDereferenceObject(ntfsDriverObject);
    }
    return STATUS_SUCCESS;
}

void MyUnload(PDRIVER_OBJECT DriverObject) {
    UNREFERENCED_PARAMETER(DriverObject);
    DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, DRIVER_PREFIX "Unloading...\n");
    UninstallNtfsHook();
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
    status = InstallNtfsHook();
    if (!NT_SUCCESS(status)) {
        DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, DRIVER_PREFIX "Failed to install NTFS hook: (0x%08X)\n", status);
        IoDeleteSymbolicLink(&symbolicLink);
        IoDeleteDevice(g_DeviceObject);
        return status;
    }
    DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, DRIVER_PREFIX "Driver loaded successfully.\n");
    return STATUS_SUCCESS;
}