#include "Win32NetworkConfig.h"

#include <Windows.h>
#include <iostream>
#include <locale>

#pragma comment(lib, "wbemuuid.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")

static void setupOutputLocale()
{
	SetConsoleOutputCP(CP_UTF8);
	try {
		std::locale utf8Locale(".UTF-8");
		std::locale::global(utf8Locale);
		std::wcout.imbue(utf8Locale);
		std::wcerr.imbue(utf8Locale);
	} catch (std::runtime_error const &) {
		std::locale systemLocale("");
		std::locale::global(systemLocale);
		std::wcout.imbue(systemLocale);
		std::wcerr.imbue(systemLocale);
	}
}

// --- main2 (列挙処理) ---

int main2()
{
	setupOutputLocale();

	Win32NetworkConfig nc;
	if (!nc.open()) {
		return 1;
	}
	nc.list_interfaces();
	nc.close();
	return 0;
}

// --- main (IPアドレス変更) ---

int main(int argc, char *argv[])
{
	HRESULT hres = CoInitializeEx(0, COINIT_MULTITHREADED);
	if (!SUCCEEDED(hres)) {
		std::cerr << "Failed to initialize COM library. Error code = 0x" << std::hex << hres << std::endl;
		return 1;
	}

	setupOutputLocale();

	Win32NetworkConfig nc;
	if (!nc.open()) {
		return 1;
	}

	bool ok = false;

#if 1
	nc.list_interfaces();
	ok = true;
#else
	if (argc != 5) {
		std::cerr << "Usage: netconf <MAC> <IP> <Subnet> <Gateway>" << std::endl;
		std::cerr << "Example: netconf \"00:1A:2B:3C:4D:5E\" 192.168.1.50 255.255.255.0 192.168.1.1" << std::endl;
		return 1;
	}

	std::wstring mac(argv[1], argv[1] + strlen(argv[1]));
	std::wstring ip(argv[2], argv[2] + strlen(argv[2]));
	std::wstring subnet(argv[3], argv[3] + strlen(argv[3]));
	std::wstring gateway(argv[4], argv[4] + strlen(argv[4]));

	ok = nc.change_address(mac, ip, subnet, gateway);
#endif

	nc.close();

	CoUninitialize();

	return ok ? 0 : 1;
}
