// Copyright © Armsoup 2026
#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#include <iostream>
#include <filesystem>
#include <string>
#include <Windows.h>
#include <shellapi.h>
#include <psapi.h>
#include <cstdlib>
#include <sstream>
#include <thread>
#include <atomic>
#include "resource.h"
namespace fs = std::filesystem;
using namespace std;

#define WM_TRAYICON (WM_USER + 1)
#define IDM_CLEAN 1001
#define IDM_EXIT 1002

atomic<uintmax_t> totalSize = 0;

NOTIFYICONDATAW nidW = { 0 };
HWND hWndHidden = NULL;

void RemoveTrayIcon() {
	nidW.cbSize = sizeof(NOTIFYICONDATAW);
	nidW.hWnd = hWndHidden;
	nidW.uID = 1;
	Shell_NotifyIconW(NIM_DELETE, &nidW);
}

void ForceClean() {
	static atomic_flag isCleaning = ATOMIC_FLAG_INIT;
	if (isCleaning.test_and_set(memory_order_acquire)) return;
	string TempPath = getenv("TEMP");
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
	isCleaning.clear(memory_order_release);
}

void CheckRAM() {
	PROCESS_MEMORY_COUNTERS pmc;
	GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc));

	SIZE_T currentRAM = pmc.WorkingSetSize;
	SIZE_T limitRAM = 100 * 1024 * 1024;

	if (currentRAM > limitRAM) {
		EmptyWorkingSet(GetCurrentProcess());
		Sleep(1000);
		if (currentRAM > limitRAM * 1.5) {
			STARTUPINFOA si = { sizeof(si) };
			PROCESS_INFORMATION pi;
			char exePath[MAX_PATH];
			GetModuleFileNameA(NULL, exePath, MAX_PATH);

			RemoveTrayIcon();
			CreateProcessA(exePath, NULL, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi);
			CloseHandle(pi.hProcess);
			CloseHandle(pi.hThread);

			exit(0);
		}
	}
}

uintmax_t MaxSize = 300 * 1024 * 1024;
string TempPath = getenv("TEMP");

void CheckTEMP() {
	totalSize = 0;
	if (fs::exists(TempPath) && fs::is_directory(TempPath)) {
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
			ForceClean();
		}
	}
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
	if (message == WM_TRAYICON) {
		if (lParam == WM_RBUTTONUP) {
			POINT pt;
			GetCursorPos(&pt);

			HMENU hMenu = CreatePopupMenu();
			wostringstream command;
			CheckTEMP();
			command << L"Size TEMP: " << totalSize / 1024 / 1024 << L"/" << MaxSize / 1024 / 1024 << L" MB";
			AppendMenuW(hMenu, MF_STRING, 0, command.str().c_str());
			AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
			AppendMenuW(hMenu, MF_STRING, IDM_CLEAN, L"Clean now");
			AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
			AppendMenuW(hMenu, MF_STRING, IDM_EXIT, L"Exit :(");

			SetForegroundWindow(hWnd);
			int cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_RIGHTBUTTON, pt.x, pt.y, 0, hWnd, NULL);
			DestroyMenu(hMenu);

			if (cmd == IDM_CLEAN) {
				thread(ForceClean).detach();
			}
			else if (cmd == IDM_EXIT) {
				RemoveTrayIcon();
				exit(0);
			}
		}
	}
	return DefWindowProcW(hWnd, message, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
	int argc;
	LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);

	if (argv != NULL) {
		for (int i = 1; i < argc; ++i) {
			wstring arg = argv[i];

			if (arg == L"--clean-now") {
				ForceClean();
				if (AttachConsole(ATTACH_PARENT_PROCESS)) {
					freopen("CONOUT$", "w", stdout);
					printf("\nOK!\n");
					PostMessageA(GetForegroundWindow(), WM_KEYDOWN, VK_RETURN, 0);
					fclose(stdout);
					FreeConsole();
				}
				else {
					system("echo OK! & pause");
				}
				LocalFree(argv);
				exit(0);
			}
			else if (arg == L"--size" && (i + 1 < argc)) {
				wstring val = argv[++i];
				MaxSize = stoull(val) * 1024 * 1024;
				if (AttachConsole(ATTACH_PARENT_PROCESS)) {
					freopen("CONOUT$", "w", stdout);
					printf("\nOK! MaxSize: %lld MB\n", MaxSize / 1024 / 1024);
					PostMessageA(GetForegroundWindow(), WM_KEYDOWN, VK_RETURN, 0);
					fclose(stdout);
					FreeConsole();
				}
				else {
					DWORD64 Size = MaxSize / 1024 / 1024;
					ostringstream command;
					command << "echo OK! MaxSize: " << Size << " MB & pause";
					system(command.str().c_str());
				}
			}
		}
		LocalFree(argv);
	}

	WNDCLASSEXW wc = { 0 };
	wc.cbSize = sizeof(WNDCLASSEXW);
	wc.lpfnWndProc = WndProc;
	wc.hInstance = hInstance;
	wc.lpszClassName = L"AutoDelTempTrayClass";
	RegisterClassExW(&wc);

	hWndHidden = CreateWindowExW(0, wc.lpszClassName, L"", 0, 0, 0, 0, 0, HWND_MESSAGE, NULL, hInstance, NULL);

	nidW.cbSize = sizeof(NOTIFYICONDATAW);
	nidW.hWnd = hWndHidden;
	nidW.uID = 1;
	nidW.uFlags = NIF_ICON | NIF_TIP | NIF_MESSAGE;
	nidW.uCallbackMessage = WM_TRAYICON;
	nidW.hIcon = (HICON)LoadImageW(hInstance, MAKEINTRESOURCEW(IDI_ICON1), IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR);


	wstring tipText = L"AutoDelTemp Active";
	tipText.copy(nidW.szTip, tipText.size());
	Shell_NotifyIconW(NIM_ADD, &nidW);

	thread monitorThread([=]() {
		DWORD lastCheck = GetTickCount64();
		while (true) {
			if (GetTickCount64() - lastCheck > 300000) {
				lastCheck = GetTickCount64();
				try {
					CheckRAM();
					CheckTEMP();
				}
				catch (...) {}
			}
			Sleep(10);
		}
		});
	monitorThread.detach();

	MSG msg;
	while (GetMessageW(&msg, NULL, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessageW(&msg);
	}

	RemoveTrayIcon();
	return 0;
}