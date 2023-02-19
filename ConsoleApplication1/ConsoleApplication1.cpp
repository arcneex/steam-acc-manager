#include <iostream>
#include <string>
#include <vector>
#include <array>
#include <fstream>
#include <Windows.h>
#include <tlhelp32.h>
#include <algorithm>
#include <chrono>
#include "resource.h"

#define ID_TRAY_ICON 1001
#define ID_TRAY_EXIT 1002

enum : int
{
	AccountName,
	PersonaName,
	RememberPassword,
	WantsOfflineMode,
	SkipOfflineModeWarning,
	AllowAutoLogin,
	MostRecent,
	Timestamp,

	MAX
};

std::string get_steam_reg(const std::string& value) {
	HKEY hKey;
	DWORD dwType = REG_SZ;
	DWORD dwSize = 1024;
	char szData[1024] = { 0 };
	std::string steam_exe;

	if (RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\Valve\\Steam", 0, KEY_READ, &hKey) == ERROR_SUCCESS)
	{
		if (RegQueryValueExA(hKey, value.c_str(), NULL, &dwType, (LPBYTE)szData, &dwSize) == ERROR_SUCCESS)
		{
			steam_exe = szData;
		}
		RegCloseKey(hKey);
	}
	return steam_exe;
}

// a function that sets a value in steam registery to given value
void set_steam_reg(const std::string& value, const std::string& value_to_set) {
	HKEY hKey;

	if (RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\Valve\\Steam", 0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS) {
		RegSetValueExA(hKey, value.c_str(), 0, REG_SZ, (const BYTE*)value_to_set.c_str(), value_to_set.size());
		RegCloseKey(hKey);
	}
}

bool is_process_running(const char* process_name) {
	PROCESSENTRY32 entry;
	entry.dwSize = sizeof(PROCESSENTRY32);

	auto snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, NULL);
	if (Process32First(snapshot, &entry) == TRUE)
	{
		while (Process32Next(snapshot, &entry) == TRUE)
		{
			if (strcmp(entry.szExeFile, process_name) == 0)
			{
				CloseHandle(snapshot);
				return true;
			}
		}
	}
	CloseHandle(snapshot);
	return false;
}


LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	// Register the window class.
	const char CLASS_NAME[] = "Tray Icon Window Class";

	WNDCLASS wc = { 0 };

	wc.lpfnWndProc = WndProc;
	wc.hInstance = hInstance;
	wc.lpszClassName = CLASS_NAME;

	RegisterClass(&wc);

	// Create the window.
	HWND hWnd = CreateWindowEx(
		0,
		CLASS_NAME,
		"Tray Icon Example",
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
		NULL,
		NULL,
		hInstance,
		NULL
	);

	if (hWnd == NULL) {
		return 0;
	}

	// Create the tray icon.
	NOTIFYICONDATA nid = { 0 };

	nid.cbSize = sizeof(NOTIFYICONDATA);
	nid.hWnd = hWnd;
	nid.uID = ID_TRAY_ICON;
	nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
	nid.uCallbackMessage = WM_USER;
	nid.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON1));
	lstrcpyn(nid.szTip, "Steam Account Manager", sizeof(nid.szTip));

	Shell_NotifyIcon(NIM_ADD, &nid);

	// Message loop.
	MSG msg = { 0 };

	while (GetMessage(&msg, NULL, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	Shell_NotifyIcon(NIM_DELETE, &nid);

	return msg.wParam;
}

HBITMAP CreateRedCircleBitmap(COLORREF rgb, int width, int height)
{
	HDC hdcScreen = GetDC(NULL);
	HDC hdc = CreateCompatibleDC(hdcScreen);
	HBITMAP hbm = CreateCompatibleBitmap(hdcScreen, width, height);
	HGDIOBJ hOld = SelectObject(hdc, hbm);

	
	// Fill the bitmap with white color
	HBRUSH hbrBack = (HBRUSH)SelectObject(hdc, GetStockObject(HOLLOW_BRUSH));
	auto r = RECT{ 0, 0, width, height };
	FillRect(hdc, &r, hbrBack);
	DeleteObject(hbrBack);

	HBRUSH hbrFill = CreateSolidBrush(/*RGB(255, 0, 0)*/rgb);
	HBRUSH hbrOld = (HBRUSH)SelectObject(hdc, hbrFill);

	// Draw the red circle
	FillRect(hdc, &r, hbrFill);

	SelectObject(hdc, hbrOld);
	DeleteObject(hbrFill);

	SelectObject(hdc, hOld);
	DeleteDC(hdc);

	ReleaseDC(NULL, hdcScreen);

	return hbm;
}

std::string removeUnicodeAndSpaces(std::string& str) {
	std::string result;

	// Remove Unicode and space characters using std::remove_if
	std::remove_copy_if(str.begin(), str.end(), std::back_inserter(result), [](const char c) {
		return !isascii(c) || isspace(c);
		});

	return result;
}

bool isMoreThan30DaysOld(const std::string& unixTimestampStr)
{
	// Convert the Unix timestamp string to a time_t value
	time_t unixTimestamp = std::stoi(unixTimestampStr);

	// Get the current time
	auto now = std::chrono::system_clock::now();
	auto nowTime = std::chrono::system_clock::to_time_t(now);

	// Calculate the difference between the current time and the Unix timestamp
	double diffInSeconds = std::difftime(nowTime, unixTimestamp);

	// Calculate the number of seconds in 30 days
	const int secondsIn30Days = 30 * 24 * 60 * 60;

	// Check if the difference is greater than 30 days
	return diffInSeconds > secondsIn30Days;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message) {
	case WM_USER:
		switch (lParam) {
		case WM_RBUTTONDOWN: {
			std::ifstream file(get_steam_reg("SteamPath") + "\\config\\loginusers.vdf");

			if (!file.is_open()) {
				MessageBoxA(hWnd, "failed to open steam user file", 0, 0);

				return -1;
			}

			std::string str((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

			// simplify
			str.erase(0, 9);
			str.erase(str.end() - 1);
			str = removeUnicodeAndSpaces(str);

			std::vector<std::array<std::string, MAX>> accounts;

			// parse accounts
			for (int i = 0; i < str.size(); ++i) {
				std::array<std::string, MAX> account;

				if (str[i] == '{') {
					int copy_end = 0;
					int insert_val = 0;

					while (str[i] != '}') {

						while (str[i] != '\0' && (str[i] != '"' || str[i + 1] != '"')) {
							++i;
						}
						i += 2;

						if (i > str.size() - 1)
							break;

						while (str[i] != '"') {
							++i;
							++copy_end;
						}

						if (copy_end) {
							account[insert_val++ % MAX] = str.substr(i - copy_end, copy_end);
							copy_end = 0;
							i += 2;
							if (insert_val % MAX == 0)
								accounts.push_back(account);
						}
					}
				}
			}

			std::sort(accounts.begin(), accounts.end(), [](const std::array<std::string, MAX>& a, const std::array<std::string, MAX>& b) {
				return a[AccountName] < b[AccountName];
			});

			POINT pt;
			GetCursorPos(&pt);

			auto iconSize = 12;
			auto blueBitmap = CreateRedCircleBitmap(RGB(0, 0, 255), iconSize, iconSize);
			auto greenBitmap = CreateRedCircleBitmap(RGB(0, 225, 0), iconSize, iconSize);
			auto grayBitmap = CreateRedCircleBitmap(RGB(150, 150, 150), iconSize, iconSize);

			HMENU hMenu = CreatePopupMenu();

			for (int i = 0; i < accounts.size(); ++i)
			{
				auto account = accounts[i];
				auto bitmap = account[MostRecent] == "1" ? blueBitmap : account[AllowAutoLogin] == "1" ? greenBitmap : grayBitmap;
				auto timestamp = account[Timestamp];

				if (isMoreThan30DaysOld(timestamp))
				{
					bitmap = grayBitmap;
				}

				AppendMenu(hMenu, MF_STRING, 2000 + i, account[AccountName].c_str());
				SetMenuItemBitmaps(hMenu, 2000 + i, MF_BYCOMMAND, bitmap, bitmap);
			}

			AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
			AppendMenu(hMenu, MF_STRING, ID_TRAY_EXIT, "Exit");

			SetForegroundWindow(hWnd);

			int cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD, pt.x, pt.y, 0, hWnd, NULL);

			if (cmd == ID_TRAY_EXIT) 
			{
				DestroyWindow(hWnd);
			}
			else if (cmd >= 2000)
			{
				auto account = accounts[cmd - 2000];

				set_steam_reg("AutoLoginUser", account[AccountName]);

				if (is_process_running("steam.exe")) {
					ShellExecute(NULL, "open", get_steam_reg("SteamExe").c_str(), "-shutdown", NULL, SW_HIDE);

					while (is_process_running("steam.exe"))
						Sleep(100);
				}

				ShellExecute(NULL, "open", "steam://open/main", NULL, NULL, SW_SHOWNORMAL);
			}

			break;
		}
		}

		break;

	case WM_DESTROY:
		PostQuitMessage(0);
		break;

	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}

	return 0;
}
