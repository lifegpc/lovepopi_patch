#include <Windows.h>
#include "config.hpp"
#include "detours.h"
#include <stdio.h>
#include "wchar_util.h"
#include "vfs.hpp"
#include "str_util.h"
#include "fileop.h"
#include <fcntl.h>

static HFONT(WINAPI *TrueCreateFontW)(int nHeight, int nWidth, int nEscapement, int nOrientation, int fnWeight, DWORD dwItalic, DWORD dwUnderline, DWORD dwStrikeOut, DWORD dwCharSet, DWORD dwOutPrecision, DWORD dwClipPrecision, DWORD dwQuality, DWORD dwPitchAndFamily, LPCWSTR lpFaceName) = CreateFontW;
static HFONT(WINAPI *TrueCreateFontA)(int nHeight, int nWidth, int nEscapement, int nOrientation, int fnWeight, DWORD dwItalic, DWORD dwUnderline, DWORD dwStrikeOut, DWORD dwCharSet, DWORD dwOutPrecision, DWORD dwClipPrecision, DWORD dwQuality, DWORD dwPitchAndFamily, LPCSTR lpFaceName) = CreateFontA;
static HANDLE(WINAPI *TrueCreateFileW)(LPCWSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes, HANDLE hTemplateFile) = CreateFileW;
static BOOL(WINAPI *TrueReadFile)(HANDLE hFile, LPVOID lpBuffer, DWORD nNumberOfBytesToRead, LPDWORD lpNumberOfBytesRead, LPOVERLAPPED lpOverlapped) = ReadFile;
static BOOL(WINAPI *TrueCloseHandle)(HANDLE hObject) = CloseHandle;
static DWORD(WINAPI *TrueGetFileSize)(HANDLE hFile, LPDWORD lpFileSizeHigh) = GetFileSize;
static decltype(GetFileSizeEx) *TrueGetFileSizeEx = GetFileSizeEx;
static DWORD(WINAPI *TrueSetFilePointer)(HANDLE hFile, LONG lDistanceToMove, PLONG lpDistanceToMoveHigh, DWORD dwMoveMethod) = SetFilePointer;
static decltype(SetFilePointerEx) *TrueSetFilePointerEx = SetFilePointerEx;
static decltype(GetFileType) *TrueGetFileType = GetFileType;
static decltype(GetFileAttributesW) *TrueGetFileAttributesW = GetFileAttributesW;
static decltype(GetFileAttributesExW) *TrueGetFileAttributesExW = GetFileAttributesExW;
static decltype(MultiByteToWideChar) *TrueMultiByteToWideChar = MultiByteToWideChar;

static Config config;
static std::wstring defaultFont;
static VFS vfs;
static HMODULE hDll = NULL;
static std::unordered_map<std::string, std::string> strings;

void insertString(std::wstring key, std::string value) {
    std::string jis;
    if (wchar_util::wstr_to_str(jis, key, 932, 0)) {
        strings[jis] = value;
    }
}

typedef char*(*ConvertWideToMultibyte)(LPSTR result, LPCWSTR source, int cp);

ConvertWideToMultibyte GetHandle() {
    HMODULE hModule = GetModuleHandleA(NULL);
    return (ConvertWideToMultibyte)((char*)hModule + 0xf58c0);
}

static ConvertWideToMultibyte h = nullptr;

char* convert(LPSTR result, LPCWSTR source, int cp) {
    if (!h) return nullptr;
    return h(result, source, 1);
}

HFONT WINAPI HookedCreateFontW(int nHeight, int nWidth, int nEscapement, int nOrientation, int fnWeight, DWORD dwItalic, DWORD dwUnderline, DWORD dwStrikeOut, DWORD dwCharSet, DWORD dwOutPrecision, DWORD dwClipPrecision, DWORD dwQuality, DWORD dwPitchAndFamily, LPCWSTR lpFaceName) {
    std::wstring name(lpFaceName);
    if (name == L"MS Gothic" || name == L"ＭＳ ゴシック") {
        lpFaceName = defaultFont.c_str();
    }
    return TrueCreateFontW(nHeight, nWidth, nEscapement, nOrientation, fnWeight, dwItalic, dwUnderline, dwStrikeOut, dwCharSet, dwOutPrecision, dwClipPrecision, dwQuality, dwPitchAndFamily, lpFaceName);
}

HFONT WINAPI HookedCreateFontA(int nHeight, int nWidth, int nEscapement, int nOrientation, int fnWeight, DWORD dwItalic, DWORD dwUnderline, DWORD dwStrikeOut, DWORD dwCharSet, DWORD dwOutPrecision, DWORD dwClipPrecision, DWORD dwQuality, DWORD dwPitchAndFamily, LPCSTR lpFaceName) {
    UINT cp[] = { CP_UTF8, CP_OEMCP, CP_ACP, 932 };
    std::wstring font;
    for (int i = 0; i < 4; i++) {
        if (wchar_util::str_to_wstr(font, lpFaceName, cp[i])) {
            if (font == L"MS Gothic" || font == L"ＭＳ ゴシック") {
                font = defaultFont;
            }
            return TrueCreateFontW(nHeight, nWidth, nEscapement, nOrientation, fnWeight, dwItalic, dwUnderline, dwStrikeOut, dwCharSet, dwOutPrecision, dwClipPrecision, dwQuality, dwPitchAndFamily, font.c_str());
        }
    }
    if (!strcmp(lpFaceName, "MS Gothic")) {
        lpFaceName = "Microsoft YaHei";
    }
    return TrueCreateFontA(nHeight, nWidth, nEscapement, nOrientation, fnWeight, dwItalic, dwUnderline, dwStrikeOut, dwCharSet, dwOutPrecision, dwClipPrecision, dwQuality, dwPitchAndFamily, lpFaceName);
}

HANDLE WINAPI HookedCreateFileW(LPCWSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes, HANDLE hTemplateFile) {
    if (vfs.ContainsFile(lpFileName)) {
        return vfs.CreateFileW(lpFileName);
    }
    std::string name;
    std::wstring tmp;
    if (wchar_util::wstr_to_str(name, lpFileName, CP_UTF8)) {
        std::string fName = fileop::basename(name);
        std::string dir = fileop::dirname(name);
        std::string pDir = fileop::basename(dir);
        std::string newPath;
        if (str_util::tolower(fName) == "bgi.gdb") {
            newPath = fileop::join(dir, "BGI.deepseek-r1.gdb");
        }
        if (str_util::tolower(pDir) == "userdata") {
            newPath = fileop::join(dir + ".deepseek-r1", fName);
        }
        if (!newPath.empty()) {
            if (wchar_util::str_to_wstr(tmp, newPath, CP_UTF8)) {
                lpFileName = tmp.c_str();
            }
        }
    }
    return TrueCreateFileW(lpFileName, dwDesiredAccess, dwShareMode, lpSecurityAttributes, dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile);
}

BOOL WINAPI HookedReadFile(HANDLE hFile, LPVOID lpBuffer, DWORD nNumberOfBytesToRead, LPDWORD lpNumberOfBytesRead, LPOVERLAPPED lpOverlapped) {
    if (vfs.ContainsHandle(hFile)) {
        if (lpOverlapped) {
            SetLastError(ERROR_INVALID_PARAMETER);
            return FALSE;
        }
        return vfs.ReadFile(hFile, lpBuffer, nNumberOfBytesToRead, lpNumberOfBytesRead);
    }
    return TrueReadFile(hFile, lpBuffer, nNumberOfBytesToRead, lpNumberOfBytesRead, lpOverlapped);
}

BOOL WINAPI HookedCloseHandle(HANDLE hObject) {
    if (vfs.ContainsHandle(hObject)) {
        vfs.CloseHandle(hObject);
        return TRUE;
    }
    return TrueCloseHandle(hObject);
}

DWORD WINAPI HookedGetFileSize(HANDLE hFile, LPDWORD lpFileSizeHigh) {
    if (vfs.ContainsHandle(hFile)) {
        return vfs.GetFileSize(hFile, lpFileSizeHigh);
    }
    return TrueGetFileSize(hFile, lpFileSizeHigh);
}

BOOL WINAPI HookedGetFileSizeEx(HANDLE hFile, PLARGE_INTEGER lpFileSize) {
    if (vfs.ContainsHandle(hFile)) {
        return vfs.GetFileSizeEx(hFile, lpFileSize);
    }
    return TrueGetFileSizeEx(hFile, lpFileSize);
}

DWORD WINAPI HookedSetFilePointer(HANDLE hFile, LONG lDistanceToMove, PLONG lpDistanceToMoveHigh, DWORD dwMoveMethod) {
    if (vfs.ContainsHandle(hFile)) {
        return vfs.SetFilePointer(hFile, lDistanceToMove, lpDistanceToMoveHigh, dwMoveMethod);
    }
    return TrueSetFilePointer(hFile, lDistanceToMove, lpDistanceToMoveHigh, dwMoveMethod);
}

BOOL WINAPI HookedSetFilePointerEx(HANDLE hFile, LARGE_INTEGER liDistanceToMove, PLARGE_INTEGER lpNewFilePointer, DWORD dwMoveMethod) {
    if (vfs.ContainsHandle(hFile)) {
        return vfs.SetFilePointerEx(hFile, liDistanceToMove, lpNewFilePointer, dwMoveMethod);
    }
    return TrueSetFilePointerEx(hFile, liDistanceToMove, lpNewFilePointer, dwMoveMethod);
}

DWORD WINAPI HookedGetFileType(HANDLE hFile) {
    if (vfs.ContainsHandle(hFile)) {
        return FILE_TYPE_DISK;
    }
    return TrueGetFileType(hFile);
}

DWORD WINAPI HookedGetFileAttributesW(LPCWSTR lpFileName) {
    if (vfs.ContainsFile(lpFileName)) {
        return FILE_ATTRIBUTE_READONLY;
    }
    std::string name;
    std::wstring tmp;
    if (wchar_util::wstr_to_str(name, lpFileName, CP_UTF8)) {
        std::string fName = fileop::basename(name);
        std::string dir = fileop::dirname(name);
        std::string pDir = fileop::basename(dir);
        std::string newPath;
        if (str_util::tolower(fName) == "bgi.gdb") {
            newPath = fileop::join(dir, "BGI.deepseek-r1.gdb");
        }
        if (str_util::tolower(pDir) == "userdata") {
            newPath = fileop::join(dir + ".deepseek-r1", fName);
        }
        if (!newPath.empty()) {
            if (wchar_util::str_to_wstr(tmp, newPath, CP_UTF8)) {
                lpFileName = tmp.c_str();
            }
        }
    }
    return TrueGetFileAttributesW(lpFileName);
}

BOOL WINAPI HookedGetFileAttributesExW(LPCWSTR lpFileName, GET_FILEEX_INFO_LEVELS fInfoLevelId, LPVOID lpFileInformation) {
    if (vfs.ContainsFile(lpFileName)) {
        return vfs.GetFileAttributesExW(lpFileName, fInfoLevelId, lpFileInformation);
    }
    std::string name;
    std::wstring tmp;
    if (wchar_util::wstr_to_str(name, lpFileName, CP_UTF8)) {
        std::string fName = fileop::basename(name);
        std::string dir = fileop::dirname(name);
        std::string pDir = fileop::basename(dir);
        std::string newPath;
        if (str_util::tolower(fName) == "bgi.gdb") {
            newPath = fileop::join(dir, "BGI.deepseek-r1.gdb");
        }
        if (str_util::tolower(pDir) == "userdata") {
            newPath = fileop::join(dir + ".deepseek-r1", fName);
        }
        if (!newPath.empty()) {
            if (wchar_util::str_to_wstr(tmp, newPath, CP_UTF8)) {
                lpFileName = tmp.c_str();
            }
        }
    }
    return TrueGetFileAttributesExW(lpFileName, fInfoLevelId, lpFileInformation);
}

// /// 哲也 的 jis编码
// const char s1[] = { 147, 78, 150, 231, 0 };
// /// 八乙女 的 jis编码
// const char s2[] = { 148, 170, 137, 179, 143, 151, 0 };

// void CheckJis(int& jis_start, int& index, std::string& tmp, size_t& len) {
//     if (jis_start == -1) return;
//     std::string jisStr = tmp.substr(jis_start, index - jis_start);
//     std::wstring tmp2;
//     std::string result;
//     if (wchar_util::str_to_wstr(tmp2, jisStr, 932) && wchar_util::wstr_to_str(result, tmp2, CP_UTF8)) {
//         tmp = tmp.substr(0, jis_start) + result + tmp.substr(index, len - index);
//         auto diff = result.length() - jisStr.length();
//         len += diff;
//         index += diff;
//         jis_start = -1;
//     }
// }

// 处理混合着CP932和UTF-8的字符串
// int WINAPI HookedMultiByteToWideChar(UINT CodePage, DWORD dwFlags, LPCSTR lpMultiByteStr, int cbMultiByte, LPWSTR lpWideCharStr, int cchWideChar) {
//     if (!(dwFlags & MB_ERR_INVALID_CHARS) && (CodePage == 932 || CodePage == CP_UTF8)) {
//         int re = TrueMultiByteToWideChar(CodePage, MB_ERR_INVALID_CHARS, lpMultiByteStr, cbMultiByte, NULL, 0);
//         if (!re && CodePage == 932) {
//             re = TrueMultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, lpMultiByteStr, cbMultiByte, NULL, 0);
//         }
//         if (!re) {
//             auto len = cbMultiByte == -1 ? strlen(lpMultiByteStr) : cbMultiByte;
//             std::string tmp(lpMultiByteStr, len);
//             tmp = str_util::str_replace(tmp, s1, "哲也");
//             tmp = str_util::str_replace(tmp, s2, "八乙女");
//             return TrueMultiByteToWideChar(CP_UTF8, dwFlags, tmp.c_str(), tmp.length(), lpWideCharStr, cchWideChar);
//         }
//     }
//     if (lpMultiByteStr && (strstr(lpMultiByteStr, s1) || strstr(lpMultiByteStr, s2))) {
//         std::string str(lpMultiByteStr, cbMultiByte == -1 ? strlen(lpMultiByteStr) : cbMultiByte);
//         str = str_util::str_replace(str, s1, "哲也");
//         str = str_util::str_replace(str, s2, "八乙女");
//         lpMultiByteStr = str.c_str();
//         return TrueMultiByteToWideChar(CP_UTF8, dwFlags, lpMultiByteStr, str.length(), lpWideCharStr, cchWideChar);
//     }
//     if (!(dwFlags & MB_ERR_INVALID_CHARS) && (CodePage == 932 || CodePage == CP_UTF8)) {
//         int re = TrueMultiByteToWideChar(CodePage, MB_ERR_INVALID_CHARS, lpMultiByteStr, cbMultiByte, NULL, 0);
//         if (!re && CodePage == 932) {
//             re = TrueMultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, lpMultiByteStr, cbMultiByte, NULL, 0);
//         }
//         if (!re) {
//             auto len = cbMultiByte == -1 ? strlen(lpMultiByteStr) : cbMultiByte;
//             std::string tmp(lpMultiByteStr, len);
//             int jis_start = -1;
//             int index = 0;
//             while (index < len) {
//                 unsigned char ch = tmp[index];
//                 if (ch < 128) {
//                     CheckJis(jis_start, index, tmp, len);
//                     index++;
//                     continue;
//                 } else if (ch < 192) {
//                 } else if (ch < 224 && index < len - 1) {
//                     unsigned char ch2 = tmp[index + 1];
//                     if (ch2 >= 128 && ch2 < 192) {
//                         CheckJis(jis_start, index, tmp, len);
//                         index += 2;
//                         continue;
//                     }
//                 } else if (ch < 240 && index < len - 2) {
//                     unsigned char ch2 = tmp[index + 1];
//                     unsigned char ch3 = tmp[index + 2];
//                     if (ch2 >= 128 && ch3 >= 128 && ch2 < 192 && ch3 < 192) {
//                         CheckJis(jis_start, index, tmp, len);
//                         index += 3;
//                         continue;
//                     }
//                 } else if (ch < 248 && index < len - 3) {
//                     unsigned char ch2 = tmp[index + 1];
//                     unsigned char ch3 = tmp[index + 2];
//                     unsigned char ch4 = tmp[index + 3];
//                     if (ch2 >= 128 && ch3 >= 128 && ch4 >= 128 && ch2 < 192 && ch3 < 192 && ch4 < 192) {
//                         CheckJis(jis_start, index, tmp, len);
//                         index += 4;
//                         continue;
//                     }
//                 }
//                 if (jis_start == -1) {
//                     jis_start = index;
//                 }
//                 index += 2;
//             }
//             CheckJis(jis_start, index, tmp, len);
//             int re = TrueMultiByteToWideChar(CP_UTF8, dwFlags, tmp.c_str(), len, lpWideCharStr, cchWideChar);
//             if (re) {
//                 lpWideCharStr[re] = 0;
//             }
//             return re;
//         }
//     }
//     return TrueMultiByteToWideChar(CodePage, dwFlags, lpMultiByteStr, cbMultiByte, lpWideCharStr, cchWideChar);
// }

int WINAPI HookedMultiByteToWideChar(UINT CodePage, DWORD dwFlags, LPCSTR lpMultiByteStr, int cbMultiByte, LPWSTR lpWideCharStr, int cchWideChar) {
    if (CodePage == 932) {
        if (lpMultiByteStr) {
            auto len = cbMultiByte < 0 ? strlen(lpMultiByteStr) : cbMultiByte;
            std::string tmp(lpMultiByteStr, len);
            auto finded = strings.find(tmp);
            if (finded != strings.end()) {
                auto& s = finded->second;
                auto re = TrueMultiByteToWideChar(CP_UTF8, dwFlags, s.c_str(), s.length(), lpWideCharStr, cchWideChar);
                if (lpWideCharStr && re) {
                    lpWideCharStr[re] = 0;
                }
                return re;
            }
        }
    }
    return TrueMultiByteToWideChar(CodePage, dwFlags, lpMultiByteStr, cbMultiByte, lpWideCharStr, cchWideChar);
}

extern "C" __declspec(dllexport) void Attach() {
    /// 文本其实位于 function2
    /// 但是傻逼SMEE搞了个改文本会导致存档不兼容的奇妙操作
    insertString(L"寮母を続ける", "继续当宿舍管理员");
    insertString(L"寮を出て勉強を始める", "搬出宿舍开始学习");
    wchar_t curExe[MAX_PATH];
    GetModuleFileNameW(NULL, curExe, MAX_PATH);
    std::string exe;
    if (wchar_util::wstr_to_str(exe, curExe, CP_UTF8)) {
        std::string dir = fileop::join(fileop::dirname(exe), "UserData.deepseek-r1");
        std::wstring wdir;
        if (wchar_util::str_to_wstr(wdir, dir, CP_UTF8)) {
            if (!CreateDirectoryW(wdir.c_str(), NULL) && GetLastError() != ERROR_ALREADY_EXISTS) {
                MessageBoxW(NULL, L"无法创建目录。", L"错误", MB_ICONERROR);
            }
        }
    }
    config.Load("deepseek-r1.config.txt");
    if (!wchar_util::str_to_wstr(defaultFont, config.configs["defaultFont"], CP_UTF8)) {
        defaultFont = L"微软雅黑";
    }
    if (defaultFont.empty()) {
        defaultFont = L"微软雅黑";
    }
    vfs.AddArchiveWithErrorMsg("deepseek-r1.scn.xp3");
    vfs.AddArchiveWithErrorMsg("deepseek-r1.data.xp3");
    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());
    h = GetHandle();
    DetourAttach(&h, convert);
    DetourAttach(&TrueCreateFontW, HookedCreateFontW);
    DetourAttach(&TrueCreateFontA, HookedCreateFontA);
    DetourAttach(&TrueCreateFileW, HookedCreateFileW);
    DetourAttach(&TrueReadFile, HookedReadFile);
    DetourAttach(&TrueCloseHandle, HookedCloseHandle);
    DetourAttach(&TrueGetFileSize, HookedGetFileSize);
    DetourAttach(&TrueGetFileSizeEx, HookedGetFileSizeEx);
    DetourAttach(&TrueSetFilePointer, HookedSetFilePointer);
    DetourAttach(&TrueSetFilePointerEx, HookedSetFilePointerEx);
    DetourAttach(&TrueGetFileType, HookedGetFileType);
    DetourAttach(&TrueGetFileAttributesW, HookedGetFileAttributesW);
    DetourAttach(&TrueGetFileAttributesExW, HookedGetFileAttributesExW);
    DetourAttach(&TrueMultiByteToWideChar, HookedMultiByteToWideChar);
    DetourTransactionCommit();
#if _DEBUG
    while( !::IsDebuggerPresent() )
    ::Sleep( 1000 );
#endif
}

extern "C" __declspec(dllexport) void Detach() {
    if (!h) return;
    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());
    DetourDetach(&h, convert);
    DetourDetach(&TrueCreateFontW, HookedCreateFontW);
    DetourDetach(&TrueCreateFontA, HookedCreateFontA);
    DetourDetach(&TrueCreateFileW, HookedCreateFileW);
    DetourDetach(&TrueReadFile, HookedReadFile);
    DetourDetach(&TrueCloseHandle, HookedCloseHandle);
    DetourDetach(&TrueGetFileSize, HookedGetFileSize);
    DetourDetach(&TrueGetFileSizeEx, HookedGetFileSizeEx);
    DetourDetach(&TrueSetFilePointer, HookedSetFilePointer);
    DetourDetach(&TrueSetFilePointerEx, HookedSetFilePointerEx);
    DetourDetach(&TrueGetFileType, HookedGetFileType);
    DetourDetach(&TrueGetFileAttributesW, HookedGetFileAttributesW);
    DetourDetach(&TrueGetFileAttributesExW, HookedGetFileAttributesExW);
    DetourDetach(&TrueMultiByteToWideChar, HookedMultiByteToWideChar);
    DetourTransactionCommit();
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID rev) {
    switch (reason) {
    case DLL_PROCESS_ATTACH:
        hDll = hModule;
        Attach();
        break;
    case DLL_PROCESS_DETACH:
        Detach();
        break;
    }
    return TRUE;
}
