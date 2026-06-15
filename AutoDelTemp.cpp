#define _CRT_SECURE_NO_WARNINGS
#include <iostream>
#include <filesystem>
#include <Windows.h>
#include <psapi.h>
namespace fs = std::filesystem;
using namespace std;

void CheckRAM() {
    PROCESS_MEMORY_COUNTERS pmc;
    GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc));

    SIZE_T currentRAM = pmc.WorkingSetSize;
    SIZE_T limitRAM = 3 * 1024 * 1024;

    if (currentRAM > limitRAM) {
        EmptyWorkingSet(GetCurrentProcess());
        Sleep(1000);
        if (currentRAM > limitRAM * 1.5) {
            STARTUPINFOA si = { sizeof(si) };
            PROCESS_INFORMATION pi;
            char exePath[MAX_PATH];
            GetModuleFileNameA(NULL, exePath, MAX_PATH);

            CreateProcessA(exePath, NULL, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi);
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);

            exit(0);
        }
    }
}

uintmax_t MaxSize = 300 * 1024 * 1024;
string TempPath = getenv("TEMP");

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
	while (true) {
		SleepEx(50000, TRUE);
		CheckRAM();
		SleepEx(250000, TRUE);
		if (fs::exists(TempPath) && fs::is_directory(TempPath)) {
			uintmax_t totalSize = 0;
			for (const auto& entry : fs::recursive_directory_iterator(TempPath, fs::directory_options::skip_permission_denied)) {
				try {
					if (fs::is_symlink(entry.path())) continue;
					if (fs::is_regular_file(entry.path())) {
						totalSize += fs::file_size(entry.path());
					}
				}
				catch (...) {}
			}
            if (totalSize >= MaxSize) {
                for (const auto& entry : fs::recursive_directory_iterator(TempPath, fs::directory_options::skip_permission_denied)) {
                    try {
                        if (fs::is_symlink(entry.path())) continue;

                        if (fs::is_regular_file(entry.path())) {
                            string pathStr = entry.path().string();
                            DWORD attributes = GetFileAttributesA(pathStr.c_str());

                            if (attributes != INVALID_FILE_ATTRIBUTES) {
                                attributes &= ~(FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_READONLY);
                                if (attributes == 0) attributes = FILE_ATTRIBUTE_NORMAL;
                                SetFileAttributesA(pathStr.c_str(), attributes);
                            }
                        }
                    }
                    catch (...) {}
                }

                for (const auto& entry : fs::directory_iterator(TempPath, fs::directory_options::skip_permission_denied)) {
                    try {
                        if (fs::is_symlink(entry.path())) continue;

                        string pathStr = entry.path().string();
                        DWORD attributes = GetFileAttributesA(pathStr.c_str());

                        if (attributes != INVALID_FILE_ATTRIBUTES) {
                            attributes &= ~(FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_READONLY);
                            if (attributes == 0) attributes = FILE_ATTRIBUTE_NORMAL;
                            SetFileAttributesA(pathStr.c_str(), attributes);
                        }
                        fs::remove_all(entry.path());
                    }
                    catch (...) {}
                }
            }
		}
	}
	return 0;
}