#include <windows.h>
#include <string>
#include <iostream>
#include <map>
#include <fcntl.h>
#include <io.h>

#include <shlwapi.h>

#pragma comment(lib, "Shlwapi.lib")

std::wstring GetExeDirectory() {
    wchar_t path[MAX_PATH];
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    PathRemoveFileSpecW(path);
    return std::wstring(path);
}

struct DriverInfo {
    std::wstring serviceName;
    std::wstring driverPath;
};

// Map your drivers here: key = flag, serviceName, driver .sys path
std::map<std::wstring, DriverInfo> driverTable = {
    {L"reg",        {L"RegistryH",   L"Registry.sys"}},
    {L"proc",       {L"ProcessesH",  L"Processes.sys"}},
    {L"files",      {L"FilesH",  L"Files.sys"}},
    {L"pci",        {L"PCIH",  L"PCI.sys"}},
    {L"devices",    {L"DevicesH",  L"Devices.sys"}}

};

bool LoadDriver(const std::wstring& serviceName, const std::wstring& driverPath) {
    SC_HANDLE hSCM = OpenSCManager(nullptr, nullptr, SC_MANAGER_CREATE_SERVICE);
    if (!hSCM) {
        std::wcerr << L"[-] OpenSCManager failed: " << GetLastError() << std::endl;
        std::wcout << L"[!] Remember to run as admin.\n";
        return false;
    }

    // Try to create the service
    SC_HANDLE hService = CreateServiceW(
        hSCM, serviceName.c_str(), serviceName.c_str(),
        SERVICE_START | DELETE | SERVICE_STOP,
        SERVICE_KERNEL_DRIVER,
        SERVICE_DEMAND_START,
        SERVICE_ERROR_IGNORE,
        driverPath.c_str(),
        nullptr, nullptr, nullptr, nullptr, nullptr);

    if (!hService) {
        DWORD err = GetLastError();
        if (err == ERROR_SERVICE_EXISTS) {
            hService = OpenServiceW(hSCM, serviceName.c_str(), SERVICE_START | DELETE | SERVICE_STOP);
            if (!hService) {
                std::wcerr << L"[-] OpenService failed: " << GetLastError() << std::endl;
                CloseServiceHandle(hSCM);
                return false;
            }
        }
        else {
            std::wcerr << L"[-] CreateService failed: " << err << std::endl;
            CloseServiceHandle(hSCM);
            return false;
        }
    }

    // Start the service (driver)
    if (!StartServiceW(hService, 0, nullptr)) {
        DWORD err = GetLastError();
        if (err != ERROR_SERVICE_ALREADY_RUNNING) {
            std::wcerr << L"[-] StartService failed: " << err << std::endl;
            CloseServiceHandle(hService);
            CloseServiceHandle(hSCM);
            return false;
        }
    }

    std::wcout << L"[+] Driver loaded: " << serviceName << std::endl;
    CloseServiceHandle(hService);
    CloseServiceHandle(hSCM);
    return true;
}

bool UnloadDriver(const std::wstring& serviceName) {
    SC_HANDLE hSCM = OpenSCManager(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (!hSCM) {
        std::wcerr << L"[-] OpenSCManager failed: " << GetLastError() << std::endl;
        return false;
    }

    SC_HANDLE hService = OpenServiceW(hSCM, serviceName.c_str(), SERVICE_STOP | DELETE);
    if (!hService) {
        std::wcerr << L"[-] OpenService failed: " << GetLastError() << std::endl;
        CloseServiceHandle(hSCM);
        return false;
    }

    SERVICE_STATUS ss = { 0 };
    if (!ControlService(hService, SERVICE_CONTROL_STOP, &ss)) {
        DWORD err = GetLastError();
        if (err != ERROR_SERVICE_NOT_ACTIVE) {
            std::wcerr << L"[-] ControlService (stop) failed: " << err << std::endl;
            // Continue, sometimes it is not running but we can delete
        }
    }

    if (!DeleteService(hService)) {
        std::wcerr << L"[-] DeleteService failed: " << GetLastError() << std::endl;
        CloseServiceHandle(hService);
        CloseServiceHandle(hSCM);
        return false;
    }

    std::wcout << L"[+] Driver unloaded: " << serviceName << std::endl;
    CloseServiceHandle(hService);
    CloseServiceHandle(hSCM);
    return true;
}

void PrintUsage() {
    _setmode(_fileno(stdout), _O_U16TEXT);
    std::wcout << L"               |               |              \n";
    std::wcout << L"               |     █    █    |              \n";
    std::wcout << L"------------█--|---██---██----██------█-------\n";
    std::wcout << L"            █  |  ███ ███    ██|     █        \n";
    std::wcout << L"       █     ██|  █████████ ███|    ██        \n";
    std::wcout << L"        ███   ██ █████████████ |  ███         \n";
    std::wcout << L"          ███  ████        ████████           \n";
    std::wcout << L"     █     ███████ ████████ █████             \n";
    std::wcout << L"------███----████ ██      ██ ███████----------\n";
    std::wcout << L"        ████████ ██ ██████ ██ ████    ███     \n";
    std::wcout << L"           ████ ███ ███ ██ ███ ████████       \n";
    std::wcout << L"          █████ ████ ███  ███ █████           \n";
    std::wcout << L"        ████  ██ ████ ███████ ████████        \n";
    std::wcout << L"       ██    ████ ████       ███   ███        \n";
    std::wcout << L"-----███---█████████████████████---██---------\n";
    std::wcout << L"    ██    ███  ███  ████ ██ ███|   ██         \n";
    std::wcout << L"    █   ███   ███   ███  ██  ██|    ██        \n";
    std::wcout << L"        █     ██   ██ █  ██   ██  █  █        \n";
    std::wcout << L"             ██|   ██    ██    ██             \n";
    std::wcout << L"            █  |   █      █    |█             \n";
    std::wcout << L"------------█--|----█----------|--------------\n";
    std::wcout << L"               |               |              \n";
    std::wcout << L"               |               KSevad @VwV    \n";
    std::wcout << L"                                              \n";
    std::wcout << L"[*]     Usage:                                \n";
    std::wcout << L"              - DriverManager.exe --load <evasion>\n";
    std::wcout << L"              - DriverManager.exe --unload <evasion>\n\n";
    std::wcout << L"[*]     Available evasions:                    \n";
    for (const auto& kv : driverTable) std::wcout << L"              - " << kv.first << L"\n";
    std::wcout << L"[!] Remember to run as admin.\n";
    std::wcout << std::endl;
}

int wmain(int argc, wchar_t* argv[]) {
    if (argc != 3) {
        PrintUsage();
        return 1;
    }

    std::wstring action = argv[1];
    std::wstring type = argv[2];

    auto it = driverTable.find(type);
    if (it == driverTable.end()) {
        std::wcerr << L"[-] Unknown driver type: " << type << std::endl;
        PrintUsage();
        return 1;
    }

    const auto& info = it->second;
    // Build driver path: <exe_dir>\drivers\<driver_file>
    std::wstring driverPath = GetExeDirectory() + L"\\drivers\\" + info.driverPath;

    if (action == L"--load") {
        if (!LoadDriver(info.serviceName, driverPath)) {
            std::wcerr << L"[-] Failed to load driver.\n";
            return 1;
        }
    }
    else if (action == L"--unload") {
        if (!UnloadDriver(info.serviceName)) {
            std::wcerr << L"[-] Failed to unload driver.\n";
            return 1;
        }
    }
    else {
        PrintUsage();
        return 1;
    }

    return 0;
}

