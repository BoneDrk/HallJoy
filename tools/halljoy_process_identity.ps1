# Query executable identity without loading remote process modules (including elevated apps).
if (-not ('HallJoyBuild.ProcessIdentity' -as [type])) {
    Add-Type -TypeDefinition @'
using System;
using System.Text;
using System.Runtime.InteropServices;
namespace HallJoyBuild {
 public static class ProcessIdentity {
  delegate bool WindowVisitor(IntPtr window, IntPtr context);
  [DllImport("user32.dll")] static extern bool EnumWindows(WindowVisitor visitor, IntPtr context);
  [DllImport("user32.dll")] static extern uint GetWindowThreadProcessId(IntPtr window, out uint pid);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] static extern int GetClassName(IntPtr window, StringBuilder name, int size);
  [DllImport("user32.dll")] public static extern bool IsIconic(IntPtr window);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] static extern IntPtr GetProp(IntPtr window, string name);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] static extern uint RegisterWindowMessage(string name);
  [DllImport("user32.dll")] static extern bool PostMessage(IntPtr window, uint message, IntPtr w, IntPtr l);
  public static IntPtr MainWindow(int pid) {
   IntPtr result = IntPtr.Zero;
   EnumWindows(delegate(IntPtr window, IntPtr context) {
    uint owner; GetWindowThreadProcessId(window, out owner);
    if (owner != pid) return true;
    var name = new StringBuilder(256); GetClassName(window, name, name.Capacity);
    if (name.ToString() != "WootingVigemGui") return true;
    result = window; return false;
   }, IntPtr.Zero);
   return result;
  }
  public static bool RequestClose(int pid) {
   IntPtr window = MainWindow(pid);
   uint message = GetProp(window, "HallJoy.TrayLifecycle.v1") != IntPtr.Zero
    ? RegisterWindowMessage("HallJoy.ExitForBuild.v1") : 0x10;
   return window != IntPtr.Zero && PostMessage(window, message, IntPtr.Zero, IntPtr.Zero);
  }
  [DllImport("kernel32.dll", SetLastError=true)] static extern IntPtr OpenProcess(uint access, bool inherit, int pid);
  [DllImport("kernel32.dll", CharSet=CharSet.Unicode, SetLastError=true)] static extern bool QueryFullProcessImageName(IntPtr process, uint flags, StringBuilder path, ref int size);
  [DllImport("kernel32.dll")] static extern bool CloseHandle(IntPtr handle);
  public static string ImagePath(int pid) {
   IntPtr handle = OpenProcess(0x1000, false, pid);
   if (handle == IntPtr.Zero) throw new System.ComponentModel.Win32Exception();
   try {
    int size = 32768; var path = new StringBuilder(size);
    if (!QueryFullProcessImageName(handle, 0, path, ref size)) throw new System.ComponentModel.Win32Exception();
    return path.ToString();
   } finally { CloseHandle(handle); }
  }
 }
}
'@
}
