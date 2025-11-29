// HugoLauncher.cpp

#include <windows.h>
#include <shlwapi.h>
#include <commctrl.h>
#include <string>
#include <thread>
#include <filesystem>
#include <sddl.h>

#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "msimg32.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "advapi32.lib")

#define W 320
#define H 240

HBITMAP hBg = nullptr;
HDC hBack = nullptr;
HBITMAP hBmp = nullptr;
HBITMAP hOldBmp = nullptr;
bool btnHover[5] = { false };

const RECT btnRect[5] = {
    { 5, 119, 121, 147 },   // Hugo 1
    { 5, 149, 121, 177 },   // Hugo 2
    { 5, 179, 121, 207 },   // Hugo 3
    { 5, 209, 121, 237 },   // Hugo 4
    { 306, 0, 321, 15 }     // Крестик
};

HWND g_hWnd = nullptr;
namespace fs = std::filesystem;

// ======================= Совместимость Win95 =======================
void SetCompatFlag(const std::wstring& exePath)
{
    HKEY hKey;
    if (RegCreateKeyExW(
        HKEY_CURRENT_USER,
        L"Software\\Microsoft\\Windows NT\\CurrentVersion\\AppCompatFlags\\Layers",
        0, nullptr, REG_OPTION_NON_VOLATILE,
        KEY_WRITE, nullptr, &hKey, nullptr) == ERROR_SUCCESS)
    {
        const wchar_t* mode = L"WIN95";
        RegSetValueExW(
            hKey,
            exePath.c_str(),
            0,
            REG_SZ,
            (BYTE*)mode,
            DWORD((wcslen(mode) + 1) * sizeof(wchar_t))
        );
        RegCloseKey(hKey);
    }
}

// ======================= Админ =======================
bool IsRunAsAdmin()
{
    BOOL isAdmin = FALSE;
    SID_IDENTIFIER_AUTHORITY NtAuthority = SECURITY_NT_AUTHORITY;
    PSID AdministratorsGroup;
    if (AllocateAndInitializeSid(&NtAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID,
        DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &AdministratorsGroup))
    {
        CheckTokenMembership(nullptr, AdministratorsGroup, &isAdmin);
        FreeSid(AdministratorsGroup);
    }
    return isAdmin != FALSE;
}

void RunAsAdmin()
{
    wchar_t szPath[MAX_PATH];
    GetModuleFileNameW(nullptr, szPath, MAX_PATH);

    SHELLEXECUTEINFOW sei = { sizeof(sei) };
    sei.lpVerb = L"runas";
    sei.lpFile = szPath;
    sei.hwnd = nullptr;
    sei.nShow = SW_NORMAL;

    if (ShellExecuteExW(&sei))
        exit(0);

    MessageBoxW(nullptr, L"Не удалось получить права администратора.\n\n", L"Ошибка", MB_ICONERROR);
    exit(1);
}

// ======================= Утилиты =======================
std::wstring GetExeDirectory()
{
    wchar_t path[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    PathRemoveFileSpecW(path);
    return path;
}

bool RunHidden(const std::wstring& cmd)
{
    STARTUPINFOW si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    PROCESS_INFORMATION pi{};
    std::wstring c = cmd;

    BOOL ok = CreateProcessW(
        nullptr,
        c.data(),
        nullptr, nullptr,
        FALSE,
        CREATE_NO_WINDOW,
        nullptr, nullptr,
        &si, &pi);

    if (ok)
    {
        WaitForSingleObject(pi.hProcess, INFINITE);
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        return true;
    }
    return false;
}

// ======================= Hugo 3 =======================
void LaunchHugo3()
{
    std::wstring exeDir = GetExeDirectory();
    std::wstring hugo3Path = exeDir + L"\\hug3.exe";

    if (!fs::exists(hugo3Path))
    {
        MessageBoxW(g_hWnd, L"hug3.exe не найден!", L"Ошибка", MB_ICONERROR);
        return;
    }

    // ---------- Поиск буквы ----------
    wchar_t driveLetter = 0;
    for (wchar_t d = L'R'; d <= L'Z'; ++d)
    {
        std::wstring root = std::wstring(1, d) + L":\\";
        if (GetDriveTypeW(root.c_str()) == DRIVE_NO_ROOT_DIR)
        {
            driveLetter = d;
            break;
        }
    }

    if (!driveLetter)
    {
        MessageBoxW(g_hWnd, L"Нет свободной буквы диска!", L"Ошибка", MB_ICONERROR);
        return;
    }

    // ---------- Короткий путь ----------
    wchar_t shortInstallPath[MAX_PATH] = {};
    DWORD r = GetShortPathNameW(exeDir.c_str(), shortInstallPath, MAX_PATH);

    std::wstring instPath83 = (r > 0 && r < MAX_PATH) ?
        shortInstallPath : exeDir;

    // ---------- Ключ Hugo97 ----------
    {
        HKEY hKey;
        if (RegCreateKeyExW(
            HKEY_LOCAL_MACHINE,
            L"SOFTWARE\\WOW6432Node\\Hugo97",
            0, nullptr, REG_OPTION_NON_VOLATILE,
            KEY_WRITE | KEY_WOW64_64KEY,
            nullptr, &hKey, nullptr) == ERROR_SUCCESS)
        {
            std::wstring cd = std::wstring(1, driveLetter) + L":";

            auto set = [&](const wchar_t* name, const std::wstring& val)
            {
                RegSetValueExW(
                    hKey, name, 0, REG_SZ,
                    (BYTE*)val.c_str(),
                    DWORD((val.size() + 1) * sizeof(wchar_t)));
            };

            set(L"CDDriveLetter", cd);
            set(L"InstPath", instPath83);
            set(L"Hugo1Sound", L"");
            set(L"Hugo2Sound", L"");

            RegCloseKey(hKey);
        }
    }

    // ---------- Uninstall ----------
    {
        HKEY hUninst;
        if (RegCreateKeyExW(
            HKEY_LOCAL_MACHINE,
            L"SOFTWARE\\WOW6432Node\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Позвоните Кузе",
            0, nullptr, REG_OPTION_NON_VOLATILE,
            KEY_WRITE | KEY_WOW64_64KEY,
            nullptr, &hUninst, nullptr) == ERROR_SUCCESS)
        {
            const wchar_t* displayName = L"Позвоните Кузе";
            RegSetValueExW(
                hUninst, L"DisplayName", 0, REG_SZ,
                (BYTE*)displayName,
                DWORD((wcslen(displayName) + 1) * sizeof(wchar_t)));

            std::wstring uninstallCmd =
                instPath83 + L"\\UNWISE.EXE " + instPath83 + L"\\INSTALL.LOG";

            RegSetValueExW(
                hUninst, L"UninstallString", 0, REG_SZ,
                (BYTE*)uninstallCmd.c_str(),
                DWORD((uninstallCmd.size() + 1) * sizeof(wchar_t)));

            RegCloseKey(hUninst);
        }
    }

    // ---------- SUBST ----------
    {
        std::wstring cmd = L"cmd.exe /c subst ";
        cmd += driveLetter;
        cmd += L": \"";
        cmd += exeDir;
        cmd += L"\"";

        RunHidden(cmd);
    }

    // ---------- Запуск игры ----------
    STARTUPINFOW si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    PROCESS_INFORMATION pi{};

    BOOL ok = CreateProcessW(
        hugo3Path.c_str(),
        nullptr,
        nullptr, nullptr,
        FALSE,
        CREATE_NO_WINDOW,
        nullptr,
        exeDir.c_str(),
        &si, &pi);

    if (ok)
    {
        WaitForSingleObject(pi.hProcess, INFINITE);
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
    }

    // ---------- SUBST удалить ----------
    {
        std::wstring cmd = L"cmd.exe /c subst ";
        cmd += driveLetter;
        cmd += L": /D";
        RunHidden(cmd);
    }

    // ---------- удалить ключи ----------
    RegDeleteKeyW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\WOW6432Node\\Hugo97");
    RegDeleteKeyW(HKEY_LOCAL_MACHINE,
        L"SOFTWARE\\WOW6432Node\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Позвоните Кузе");

    if (!ok)
        MessageBoxW(g_hWnd, L"Ошибка запуска hug3.exe", L"Ошибка", MB_ICONERROR);

    PostMessage(g_hWnd, WM_CLOSE, 0, 0);
}

// ======================= Hugo 1/2 DOSBox =======================
void LaunchDOSGame(const wchar_t* exeName)
{
    std::wstring dir = GetExeDirectory();
    if (!dir.empty() && dir.back() != L'\\') dir += L'\\';

    std::wstring cmd = L"dosbox -noconsole -fullscreen -c \"mount c \\\"" +
        dir + L"\\\" \" -c \"c:\" -c \"" +
        exeName + L"\" -c \"exit\"";

    wchar_t* buffer = new wchar_t[cmd.size() + 1];
    wcscpy_s(buffer, cmd.size() + 1, cmd.c_str());

    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi = {};
    CreateProcessW(nullptr, buffer, nullptr, nullptr,
        FALSE, CREATE_NEW_CONSOLE, nullptr, nullptr, &si, &pi);

    if (pi.hThread) CloseHandle(pi.hThread);
    if (pi.hProcess) CloseHandle(pi.hProcess);
    delete[] buffer;

    PostMessage(g_hWnd, WM_CLOSE, 0, 0);
}

// ======================= Графика =======================
bool LoadBackground()
{
    wchar_t path[MAX_PATH] = {};
    wcscpy_s(path, GetExeDirectory().c_str());
    PathAppendW(path, L"menu.bmp");
    hBg = (HBITMAP)LoadImageW(nullptr, path, IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE);
    return hBg != nullptr;
}

bool CreateBackbuffer(HWND hwnd)
{
    HDC hdc = GetDC(hwnd);
    hBack = CreateCompatibleDC(hdc);
    BITMAPINFO bmi = { sizeof(BITMAPINFOHEADER), W, -H, 1, 32, BI_RGB };
    hBmp = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, nullptr, nullptr, 0);
    ReleaseDC(hwnd, hdc);
    if (!hBmp) return false;
    hOldBmp = (HBITMAP)SelectObject(hBack, hBmp);
    return true;
}

void DrawGlow(HDC dc, const RECT& r, BYTE alpha = 96)
{
    int w = r.right - r.left, h = r.bottom - r.top;
    if (w <= 0 || h <= 0) return;

    BITMAPINFO bmi = { sizeof(BITMAPINFOHEADER), w, -h, 1, 32, BI_RGB };
    void* bits = nullptr;
    HBITMAP tmp = CreateDIBSection(nullptr, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!tmp) return;

    DWORD color = (alpha << 24) | (alpha << 16) | (alpha << 8) | alpha;
    for (int i = 0; i < w * h; ++i)
        ((DWORD*)bits)[i] = color;

    HDC mem = CreateCompatibleDC(dc);
    SelectObject(mem, tmp);
    BLENDFUNCTION bf = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
    AlphaBlend(dc, r.left, r.top, w, h, mem, 0, 0, w, h, bf);

    DeleteDC(mem);
    DeleteObject(tmp);
}

void Redraw(HWND hwnd) { InvalidateRect(hwnd, nullptr, FALSE); }

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_CREATE:
        g_hWnd = hwnd;
        if (!CreateBackbuffer(hwnd)) return -1;
        break;

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(hwnd, &ps);
        HDC src = CreateCompatibleDC(dc);
        SelectObject(src, hBg);
        BitBlt(hBack, 0, 0, W, H, src, 0, 0, SRCCOPY);
        DeleteDC(src);

        for (int i = 0; i < 5; ++i)
            if (btnHover[i]) DrawGlow(hBack, btnRect[i]);

        BitBlt(dc, 0, 0, W, H, hBack, 0, 0, SRCCOPY);
        EndPaint(hwnd, &ps);
    }
    return 0;

    case WM_MOUSEMOVE:
    {
        POINT pt = { LOWORD(lParam), HIWORD(lParam) };
        bool changed = false;
        for (int i = 0; i < 5; ++i)
            if (PtInRect(&btnRect[i], pt) != btnHover[i])
                btnHover[i] = !btnHover[i], changed = true;

        if (changed) Redraw(hwnd);
        TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, hwnd };
        TrackMouseEvent(&tme);
    }
    return 0;

    case WM_MOUSELEAVE:
        memset(btnHover, 0, sizeof(btnHover));
        Redraw(hwnd);
        return 0;

    case WM_LBUTTONDOWN:
    {
        POINT pt = { LOWORD(lParam), HIWORD(lParam) };
        if (PtInRect(&btnRect[4], pt)) DestroyWindow(hwnd);
        else if (PtInRect(&btnRect[0], pt)) std::thread(LaunchDOSGame, L"hugo.exe").detach();
        else if (PtInRect(&btnRect[1], pt)) std::thread(LaunchDOSGame, L"hug2.exe").detach();
        else if (PtInRect(&btnRect[2], pt)) std::thread(LaunchHugo3).detach();
        else if (PtInRect(&btnRect[3], pt))
        {
            std::wstring path = GetExeDirectory() + L"\\hug4.exe";
            ShellExecuteW(hwnd, L"open", path.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
            PostMessage(hwnd, WM_CLOSE, 0, 0);
        }
    }
    return 0;

    case WM_DESTROY:
        if (hBack) { SelectObject(hBack, hOldBmp); DeleteObject(hBmp); DeleteDC(hBack); }
        if (hBg) DeleteObject(hBg);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

// ======================= WinMain =======================
int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int)
{
    // --- Админ ---
    if (!IsRunAsAdmin())
    {
        RunAsAdmin();
        return 0;
    }

    InitCommonControls();

    // --- Ставим совместимость Win95 ---
    std::wstring base = GetExeDirectory();
    SetCompatFlag(base + L"\\hug3.exe");
    SetCompatFlag(base + L"\\hug4.exe");

    // --- Графика ---
    if (!LoadBackground())
    {
        MessageBoxW(nullptr, L"Не найден menu.bmp!", L"Ошибка", MB_ICONERROR);
        return 0;
    }

    WNDCLASSW wc = { 0, WndProc, 0, 0, hInst, nullptr,
        LoadCursor(nullptr, IDC_ARROW), nullptr, nullptr, L"HugoMenu" };
    RegisterClassW(&wc);

    int sx = GetSystemMetrics(SM_CXSCREEN);
    int sy = GetSystemMetrics(SM_CYSCREEN);

    g_hWnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        L"HugoMenu", L"",
        WS_POPUP | WS_VISIBLE,
        (sx - W) / 2, (sy - H) / 2, W, H,
        nullptr, nullptr, hInst, nullptr);

    ShowWindow(g_hWnd, SW_SHOW);
    UpdateWindow(g_hWnd);

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}
