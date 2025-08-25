using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Runtime.InteropServices;
using System.Windows.Forms;

namespace KSevad
{
    static class Program
    {
        // Driver info class, similar to your C++ struct
        public class DriverInfo
        {
            public string ServiceName;
            public string DriverPath;
            public DriverInfo(string serviceName, string driverPath)
            {
                ServiceName = serviceName;
                DriverPath = driverPath;
            }
        }

        // Your driver table
        public static Dictionary<string, DriverInfo> driverTable = new Dictionary<string, DriverInfo>()
        {
            { "reg",     new DriverInfo("RegistryH", "Registry.sys") },
            { "proc",    new DriverInfo("ProcessesH", "Processes.sys") },
            { "files",   new DriverInfo("FilesH", "Files.sys") },
            { "pci",     new DriverInfo("PCIH", "PCI.sys") },
            { "devices", new DriverInfo("DevicesH", "Devices.sys") }
        };

        [DllImport("kernel32.dll")]
        static extern IntPtr GetConsoleWindow();

        [DllImport("user32.dll")]
        static extern bool ShowWindow(IntPtr hWnd, int nCmdShow);

        const int SW_HIDE = 0;

        [STAThread]
        static void Main(string[] args)
        {
            var handle = GetConsoleWindow();
            ShowWindow(handle, SW_HIDE);
            // GUI Mode
            Application.EnableVisualStyles();
            Application.SetCompatibleTextRenderingDefault(false);
            Application.Run(new Form1());
        }

        public static string GetExeDirectory()
        {
            return Path.GetDirectoryName(Application.ExecutablePath);
        }


        // Windows API wrappers for driver management
        [DllImport("advapi32.dll", SetLastError = true, CharSet = CharSet.Unicode)]
        public static extern IntPtr OpenSCManager(string lpMachineName, string lpDatabaseName, uint dwDesiredAccess);

        [DllImport("advapi32.dll", SetLastError = true, CharSet = CharSet.Unicode)]
        public static extern IntPtr CreateService(IntPtr hSCManager, string lpServiceName, string lpDisplayName,
            uint dwDesiredAccess, uint dwServiceType, uint dwStartType, uint dwErrorControl,
            string lpBinaryPathName, string lpLoadOrderGroup, IntPtr lpdwTagId,
            string lpDependencies, string lpServiceStartName, string lpPassword);

        [DllImport("advapi32.dll", SetLastError = true, CharSet = CharSet.Unicode)]
        public static extern IntPtr OpenService(IntPtr hSCManager, string lpServiceName, uint dwDesiredAccess);

        [DllImport("advapi32.dll", SetLastError = true)]
        [return: MarshalAs(UnmanagedType.Bool)]
        public static extern bool StartService(IntPtr hService, int dwNumServiceArgs, IntPtr lpServiceArgVectors);

        [DllImport("advapi32.dll", SetLastError = true)]
        [return: MarshalAs(UnmanagedType.Bool)]
        public static extern bool DeleteService(IntPtr hService);

        [DllImport("advapi32.dll", SetLastError = true)]
        [return: MarshalAs(UnmanagedType.Bool)]
        public static extern bool CloseServiceHandle(IntPtr hSCObject);

        [DllImport("advapi32.dll", SetLastError = true)]
        [return: MarshalAs(UnmanagedType.Bool)]
        public static extern bool ControlService(IntPtr hService, int dwControl, ref SERVICE_STATUS lpServiceStatus);

        public const uint SC_MANAGER_CREATE_SERVICE = 0x0002;
        public const uint SC_MANAGER_CONNECT = 0x0001;
        public const uint SERVICE_START = 0x0010;
        public const uint SERVICE_STOP = 0x0020;
        public const uint DELETE = 0x10000;
        public const uint SERVICE_KERNEL_DRIVER = 0x00000001;
        public const uint SERVICE_DEMAND_START = 0x00000003;
        public const uint SERVICE_ERROR_IGNORE = 0x00000000;
        public const int SERVICE_CONTROL_STOP = 1;
        public const int ERROR_SERVICE_EXISTS = 1073;
        public const int ERROR_SERVICE_ALREADY_RUNNING = 1056;
        public const int ERROR_SERVICE_NOT_ACTIVE = 1062;

        [StructLayout(LayoutKind.Sequential)]
        public struct SERVICE_STATUS
        {
            public int dwServiceType;
            public int dwCurrentState;
            public int dwControlsAccepted;
            public int dwWin32ExitCode;
            public int dwServiceSpecificExitCode;
            public int dwCheckPoint;
            public int dwWaitHint;
        }

        // Implementations (partial, see your original for logic)
        public static bool LoadDriver(string serviceName, string driverPath)
        {
            IntPtr hSCM = OpenSCManager(null, null, SC_MANAGER_CREATE_SERVICE);
            if (hSCM == IntPtr.Zero)
            {
                Console.WriteLine("[-] OpenSCManager failed: " + Marshal.GetLastWin32Error());
                Console.WriteLine("[!] Remember to run as admin.");
                return false;
            }

            IntPtr hService = CreateService(hSCM, serviceName, serviceName, SERVICE_START | DELETE | SERVICE_STOP,
                SERVICE_KERNEL_DRIVER, SERVICE_DEMAND_START, SERVICE_ERROR_IGNORE, driverPath,
                null, IntPtr.Zero, null, null, null);

            if (hService == IntPtr.Zero)
            {
                int err = Marshal.GetLastWin32Error();
                if (err == ERROR_SERVICE_EXISTS)
                {
                    hService = OpenService(hSCM, serviceName, SERVICE_START | DELETE | SERVICE_STOP);
                    if (hService == IntPtr.Zero)
                    {
                        Console.WriteLine("[-] OpenService failed: " + Marshal.GetLastWin32Error());
                        CloseServiceHandle(hSCM);
                        return false;
                    }
                }
                else
                {
                    Console.WriteLine("[-] CreateService failed: " + err);
                    CloseServiceHandle(hSCM);
                    return false;
                }
            }

            if (!StartService(hService, 0, IntPtr.Zero))
            {
                int err = Marshal.GetLastWin32Error();
                if (err != ERROR_SERVICE_ALREADY_RUNNING)
                {
                    Console.WriteLine("[-] StartService failed: " + err);
                    CloseServiceHandle(hService);
                    CloseServiceHandle(hSCM);
                    return false;
                }
            }

            Console.WriteLine("[+] Driver loaded: " + serviceName);
            CloseServiceHandle(hService);
            CloseServiceHandle(hSCM);
            return true;
        }

        public static bool UnloadDriver(string serviceName)
        {
            IntPtr hSCM = OpenSCManager(null, null, SC_MANAGER_CONNECT);
            if (hSCM == IntPtr.Zero)
            {
                Console.WriteLine("[-] OpenSCManager failed: " + Marshal.GetLastWin32Error());
                return false;
            }
            IntPtr hService = OpenService(hSCM, serviceName, SERVICE_STOP | DELETE);
            if (hService == IntPtr.Zero)
            {
                Console.WriteLine("[-] OpenService failed: " + Marshal.GetLastWin32Error());
                CloseServiceHandle(hSCM);
                return false;
            }

            SERVICE_STATUS ss = new SERVICE_STATUS();
            if (!ControlService(hService, SERVICE_CONTROL_STOP, ref ss))
            {
                int err = Marshal.GetLastWin32Error();
                if (err != ERROR_SERVICE_NOT_ACTIVE)
                {
                    Console.WriteLine("[-] ControlService (stop) failed: " + err);
                    // Continue, sometimes it is not running but we can delete
                }
            }

            if (!DeleteService(hService))
            {
                Console.WriteLine("[-] DeleteService failed: " + Marshal.GetLastWin32Error());
                CloseServiceHandle(hService);
                CloseServiceHandle(hSCM);
                return false;
            }

            Console.WriteLine("[+] Driver unloaded: " + serviceName);
            CloseServiceHandle(hService);
            CloseServiceHandle(hSCM);
            return true;
        }
    }
}