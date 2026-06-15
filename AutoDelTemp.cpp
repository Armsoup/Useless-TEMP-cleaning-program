#define _CRT_SECURE_NO_WARNINGS
#include <iostream>
#include <filesystem>
#include <Windows.h>
#include <Lmcons.h>
namespace fs = std::filesystem;
using namespace std;

uintmax_t MaxSize = 300 * 1024 * 1024;
string TempPath = getenv("TEMP");

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
	while (true) {
		Sleep(300000);
		if (fs::exists(TempPath) && fs::is_directory(TempPath)) {
			uintmax_t totalSize = 0;
			for (const auto& entry : fs::recursive_directory_iterator(TempPath, fs::directory_options::skip_permission_denied)) {
				try {
					if (fs::is_regular_file(entry.path())) {
						totalSize += fs::file_size(entry.path());
					}
				}
				catch (...) {}
			}
			if (totalSize >= MaxSize) {
				for (const auto& entry : fs::directory_iterator(TempPath, fs::directory_options::skip_permission_denied)) {
					try {
						fs::remove_all(entry.path());
					}
					catch (const fs::filesystem_error& e) {
					}
				}
			}
		}
	}
	return 0;
}