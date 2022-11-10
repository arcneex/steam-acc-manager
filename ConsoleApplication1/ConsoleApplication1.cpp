// ConsoleApplication1.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <string>
#include <vector>
#include <array>
#include <fstream>
#include <Windows.h>
#include <tlhelp32.h>
#include <algorithm>

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

std::string get_steam_reg(const std::string &value) {
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

// check if program was ran with admin permissions
bool is_admin() {
	BOOL fRet = FALSE;
	HANDLE hToken = NULL;
	if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken)) {
		TOKEN_ELEVATION elevation;
		DWORD cbSize = sizeof(TOKEN_ELEVATION);

		if (GetTokenInformation(hToken, TokenElevation, &elevation, sizeof(elevation), &cbSize))
			fRet = elevation.TokenIsElevated;
	}
	if (hToken)
		CloseHandle(hToken);

	return fRet;
}

int main(int argc)
{
	// check if program was ran as admin
	if (argc == 1 && !is_admin()) {
		std::cout << "Please run as admin" << std::endl;
		Sleep(1000);

		return 0;
	}

	std::ifstream file(get_steam_reg("SteamPath") + "\\config\\loginusers.vdf");
	
	if (!file.is_open()) {
		std::cout << "failed to open steam user file" << std::endl;
		Sleep(200);

		return 0;
	}

	std::string str((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

	// simplify
	str.erase(0, 9);
	str.erase(str.end() - 1);
	str.erase(std::remove_if(str.begin(), str.end(), isspace), str.end());
	
	std::vector<std::array<std::string, MAX>> accounts;

	// parse accounts
	for (int i = 0; i < str.size(); ++i) {
		std::array<std::string, MAX> account;

		if (str[i] == '{') {
			int copy_end = 0;
			int insert_val = 0;
			
			while (str[i] != '}') {
				
				while (str[i] != '"' || str[i + 1] != '"') {
					++i;
				} 
				i += 2;
				
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

	auto h = GetStdHandle(STD_OUTPUT_HANDLE);

	for (int i = 0; i < accounts.size(); ++i)
	{
		auto acc = accounts[i];
		SetConsoleTextAttribute(h, 7); std::cout << i << ". ";
		
		acc[MostRecent] == "1" ? SetConsoleTextAttribute(h, 9) : acc[RememberPassword] == "1" ? SetConsoleTextAttribute(h, 2) : SetConsoleTextAttribute(h, 7);

		std::cout << acc[AccountName] << std::endl;
	}

	SetConsoleTextAttribute(h, 7);
		
	int accnum = -1;
	std::cout << "Select an account: " << std::endl;

	while (accnum < 0 || accnum >= accounts.size())
		std::cin >> accnum;

	set_steam_reg("AutoLoginUser", accounts[accnum][AccountName]);
	
	if (is_process_running("steam.exe")) {
		auto steam_shutdown = "start \"\" \"" + get_steam_reg("SteamExe") + "\"" + " -shutdown";
		system(steam_shutdown.c_str());

		while (is_process_running("steam.exe"))
			Sleep(100);
	}
	
	system("start steam://open/main");
	return 0;
}
