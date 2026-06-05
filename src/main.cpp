#define _WIN32_DCOM
#include <Wbemidl.h>
#include <comdef.h>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>
#include <windows.h>

#pragma comment(lib, "wbemuuid.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")

class NetworkConfig {
public:
	NetworkConfig() = default;

	~NetworkConfig()
	{
		close();
	}

	bool open()
	{
		HRESULT hres = CoInitializeEx(0, COINIT_MULTITHREADED);
		if (SUCCEEDED(hres)) {
			m_initialized = true;
		}
		if (FAILED(hres)) {
			std::cerr << "CoInitializeEx failed. Error code = 0x" << std::hex << hres << std::endl;
			return false;
		}

		hres = CoInitializeSecurity(
					NULL,
					-1,
					NULL,
					NULL,
					RPC_C_AUTHN_LEVEL_DEFAULT,
					RPC_C_IMP_LEVEL_IMPERSONATE,
					NULL,
					EOAC_NONE,
					NULL);
		if (FAILED(hres) && hres != RPC_E_TOO_LATE) {
			std::cerr << "CoInitializeSecurity failed. Error code = 0x" << std::hex << hres << std::endl;
			CoUninitialize();
			return false;
		}

		hres = CoCreateInstance(
					CLSID_WbemLocator,
					0,
					CLSCTX_INPROC_SERVER,
					IID_IWbemLocator,
					(LPVOID *)&m_pLoc);
		if (FAILED(hres)) {
			std::cerr << "CoCreateInstance(WbemLocator) failed. Error code = 0x" << std::hex << hres << std::endl;
			CoUninitialize();
			return false;
		}

		hres = m_pLoc->ConnectServer(
					_bstr_t(L"ROOT\\CIMV2"),
					NULL,
					NULL,
					0,
					NULL,
					0,
					0,
					&m_pSvc);
		if (FAILED(hres)) {
			std::cerr << "ConnectServer failed. Error code = 0x" << std::hex << hres << std::endl;
			m_pLoc->Release();
			m_pLoc = nullptr;
			CoUninitialize();
			return false;
		}

		hres = CoSetProxyBlanket(
					m_pSvc,
					RPC_C_AUTHN_WINNT,
					RPC_C_AUTHZ_NONE,
					NULL,
					RPC_C_AUTHN_LEVEL_CALL,
					RPC_C_IMP_LEVEL_IMPERSONATE,
					NULL,
					EOAC_NONE);
		if (FAILED(hres)) {
			std::cerr << "CoSetProxyBlanket failed. Error code = 0x" << std::hex << hres << std::endl;
			m_pSvc->Release();
			m_pSvc = nullptr;
			m_pLoc->Release();
			m_pLoc = nullptr;
			CoUninitialize();
			return false;
		}

		return true;
	}

	void close()
	{
		if (m_pSvc) {
			m_pSvc->Release();
			m_pSvc = nullptr;
		}
		if (m_pLoc) {
			m_pLoc->Release();
			m_pLoc = nullptr;
		}
		if (m_initialized) {
			CoUninitialize();
			m_initialized = false;
		}
	}

	void list_interfaces()
	{
		std::wcout << L"--- Network Adapter Configuration ---" << std::endl;
		enumerate(L"SELECT * FROM Win32_NetworkAdapterConfiguration",
				  [&](IWbemClassObject *pclsObj) {
			printProperty(pclsObj, L"Index");
			printProperty(pclsObj, L"MACAddress");
			printArrayProperty(pclsObj, L"IPAddress");
			printArrayProperty(pclsObj, L"DefaultIPGateway");
			printArrayProperty(pclsObj, L"IPSubnet");
			printProperty(pclsObj, L"Description");
			printBoolProperty(pclsObj, L"DHCPEnabled");
			std::wcout << L"-------------------------" << std::endl;
		});

		std::wcout << std::endl
				   << L"--- Network Adapter (Connected) ---" << std::endl;
		enumerate(L"SELECT * FROM Win32_NetworkAdapter",
				  [&](IWbemClassObject *pclsObj) {
			printProperty(pclsObj, L"Index");
			printProperty(pclsObj, L"Name");
			printProperty(pclsObj, L"ProductName");
			printProperty(pclsObj, L"Manufacturer");
			printProperty(pclsObj, L"AdapterType");
			printProperty(pclsObj, L"Status");
			printProperty(pclsObj, L"NetConnectionID");
			printBoolProperty(pclsObj, L"NetEnabled");
			printProperty64(pclsObj, L"Speed");
			std::wcout << L"-------------------------" << std::endl;
		});
	}

	bool change_address(std::wstring const &mac, std::wstring const &ip, std::wstring const &subnet, std::wstring const &gateway)
	{
		std::wstring query = L"SELECT * FROM Win32_NetworkAdapterConfiguration WHERE MACAddress = '" + mac + L"'";
		bool found = false;

		enumerate(query.c_str(), [&](IWbemClassObject *pclsObj) {
			if (found) return;

			VARIANT vtPath;
			VariantInit(&vtPath);
			HRESULT hr = pclsObj->Get(L"__PATH", 0, &vtPath, 0, 0);
			if (SUCCEEDED(hr) && vtPath.vt == VT_BSTR) {
				BSTR objPath = SysAllocString(vtPath.bstrVal);

				hr = callEnableStatic(objPath, ip, subnet);
				if (SUCCEEDED(hr)) {
					hr = callSetGateways(objPath, gateway);
					if (SUCCEEDED(hr)) {
						std::wcout << L"Static IP configured successfully for MAC " << mac << std::endl;
						found = true;
					} else {
						reportMethodError(hr, L"SetGateways");
					}
				} else {
					reportMethodError(hr, L"EnableStatic");
				}
				SysFreeString(objPath);
			}
			VariantClear(&vtPath);
		});

		if (!found) {
			std::wcerr << L"Adapter with MAC address " << mac << L" not found or configuration failed." << std::endl;
		}
		return found;
	}

private:
	IWbemLocator *m_pLoc = nullptr;
	IWbemServices *m_pSvc = nullptr;
	bool m_initialized = true;

	static SAFEARRAY *createStringSafeArray(std::vector<std::wstring> const &strings)
	{
		SAFEARRAYBOUND sab;
		sab.lLbound = 0;
		sab.cElements = static_cast<ULONG>(strings.size());
		SAFEARRAY *psa = SafeArrayCreate(VT_BSTR, 1, &sab);
		if (!psa) return nullptr;
		for (LONG i = 0; i < static_cast<LONG>(strings.size()); ++i) {
			BSTR bstr = SysAllocString(strings[i].c_str());
			SafeArrayPutElement(psa, &i, bstr);
			SysFreeString(bstr);
		}
		return psa;
	}

	template <typename Func>
	void enumerate(wchar_t const *wql, Func callback)
	{
		IEnumWbemClassObject *pEnumerator = nullptr;
		HRESULT hres = m_pSvc->ExecQuery(
					bstr_t("WQL"),
					bstr_t(wql),
					WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
					NULL,
					&pEnumerator);
		if (FAILED(hres)) {
			std::cerr << "ExecQuery failed. Error code = 0x" << std::hex << hres << std::endl;
			return;
		}

		IWbemClassObject *pclsObj = nullptr;
		ULONG uReturn = 0;
		while (pEnumerator) {
			(void)pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn);
			if (uReturn == 0) break;
			callback(pclsObj);
			pclsObj->Release();
		}
		pEnumerator->Release();
	}

	static void printProperty(IWbemClassObject *pclsObj, wchar_t const *name)
	{
		printProperty(pclsObj, name, name);
	}

	static void printProperty(IWbemClassObject *pclsObj, wchar_t const *propName, wchar_t const *label)
	{
		VARIANT vtProp;
		VariantInit(&vtProp);
		HRESULT hr = pclsObj->Get(propName, 0, &vtProp, 0, 0);
		if (FAILED(hr)) {
			VariantClear(&vtProp);
			return;
		}
		std::wcout << label << L": ";
		switch (vtProp.vt) {
		case VT_NULL:
			std::wcout << "(null)";
			break;
		case VT_BSTR:
			std::wcout << vtProp.bstrVal;
			break;
		case VT_I1:
			std::wcout << static_cast<int>(vtProp.cVal);
			break;
		case VT_I2:
			std::wcout << vtProp.iVal;
			break;
		case VT_I4:
			std::wcout << vtProp.lVal;
			break;
		case VT_I8:
			std::wcout << vtProp.llVal;
			break;
		case VT_UI1:
			std::wcout << static_cast<unsigned int>(vtProp.bVal);
			break;
		case VT_UI2:
			std::wcout << vtProp.uiVal;
			break;
		case VT_UI4:
			std::wcout << vtProp.ulVal;
			break;
		case VT_UI8:
			std::wcout << vtProp.ullVal;
			break;
		case VT_R4:
			std::wcout << vtProp.fltVal;
			break;
		case VT_R8:
			std::wcout << vtProp.dblVal;
			break;
		case VT_BOOL:
			std::wcout << (vtProp.boolVal == VARIANT_TRUE ? L"Yes" : L"No");
			break;
		default:
			std::wcout << L"<type=" << vtProp.vt << L">";
			break;
		}
		std::wcout << std::endl;
		VariantClear(&vtProp);
	}

	static void printBoolProperty(IWbemClassObject *pclsObj, wchar_t const *name)
	{
		printBoolProperty(pclsObj, name, name);
	}

	static void printBoolProperty(IWbemClassObject *pclsObj, wchar_t const *propName, wchar_t const *label)
	{
		VARIANT vtProp;
		VariantInit(&vtProp);
		HRESULT hr = pclsObj->Get(propName, 0, &vtProp, 0, 0);
		if (SUCCEEDED(hr) && vtProp.vt == VT_BOOL) {
			std::wcout << label << L": " << (vtProp.boolVal == VARIANT_TRUE ? L"Yes" : L"No") << std::endl;
		}
		VariantClear(&vtProp);
	}

	static void printArrayProperty(IWbemClassObject *pclsObj, wchar_t const *name)
	{
		VARIANT vtProp;
		VariantInit(&vtProp);
		HRESULT hr = pclsObj->Get(name, 0, &vtProp, 0, 0);
		if (SUCCEEDED(hr) && (vtProp.vt & VT_ARRAY)) {
			SAFEARRAY *pArray = vtProp.parray;
			BSTR *pBstr;
			SafeArrayAccessData(pArray, (void **)&pBstr);
			LONG lLower, lUpper;
			SafeArrayGetLBound(pArray, 1, &lLower);
			SafeArrayGetUBound(pArray, 1, &lUpper);
			for (LONG i = lLower; i <= lUpper; ++i) {
				std::wcout << name << L": " << pBstr[i] << std::endl;
			}
			SafeArrayUnaccessData(pArray);
		}
		VariantClear(&vtProp);
	}

	static void printProperty64(IWbemClassObject *pclsObj, wchar_t const *name)
	{
		VARIANT vtProp;
		VariantInit(&vtProp);
		HRESULT hr = pclsObj->Get(name, 0, &vtProp, 0, 0);
		if (SUCCEEDED(hr)) {
			if (vtProp.vt == VT_BSTR) {
				std::wcout << name << L": " << vtProp.bstrVal << std::endl;
			} else if (vtProp.vt == VT_I8 || vtProp.vt == VT_UI8) {
				std::wcout << name << L": " << vtProp.ullVal << std::endl;
			}
		}
		VariantClear(&vtProp);
	}

	static HRESULT extractReturnValue(IWbemClassObject *pOutParams)
	{
		VARIANT ret;
		VariantInit(&ret);
		HRESULT hr = pOutParams->Get(L"ReturnValue", 0, &ret, NULL, 0);
		if (SUCCEEDED(hr)) {
			ULONG rv = 0;
			if (ret.vt == VT_UI4)
				rv = ret.ulVal;
			else if (ret.vt == VT_I4)
				rv = static_cast<ULONG>(ret.lVal);
			else
				rv = 1;
			if (rv != 0) hr = HRESULT_FROM_WIN32(rv);
		}
		VariantClear(&ret);
		return hr;
	}

	HRESULT callEnableStatic(BSTR objPath, std::wstring const &ip, std::wstring const &subnet)
	{
		IWbemClassObject *pClass = nullptr;
		HRESULT hr = m_pSvc->GetObject(bstr_t("Win32_NetworkAdapterConfiguration"), 0, NULL, &pClass, NULL);
		if (FAILED(hr)) return hr;

		IWbemClassObject *pInParamsDef = nullptr;
		hr = pClass->GetMethod(L"EnableStatic", 0, &pInParamsDef, NULL);
		pClass->Release();
		if (FAILED(hr)) return hr;

		IWbemClassObject *pInParams = nullptr;
		hr = pInParamsDef->SpawnInstance(0, &pInParams);
		pInParamsDef->Release();
		if (FAILED(hr)) return hr;

		{
			VARIANT var;
			VariantInit(&var);
			var.vt = VT_ARRAY | VT_BSTR;
			std::vector<std::wstring> v = { ip };
			var.parray = createStringSafeArray(v);
			hr = pInParams->Put(L"IPAddress", 0, &var, 0);
			VariantClear(&var);
			if (FAILED(hr)) {
				pInParams->Release();
				return hr;
			}
		}

		{
			VARIANT var;
			VariantInit(&var);
			var.vt = VT_ARRAY | VT_BSTR;
			std::vector<std::wstring> v = { subnet };
			var.parray = createStringSafeArray(v);
			hr = pInParams->Put(L"SubnetMask", 0, &var, 0);
			VariantClear(&var);
			if (FAILED(hr)) {
				pInParams->Release();
				return hr;
			}
		}

		IWbemClassObject *pOutParams = nullptr;
		hr = m_pSvc->ExecMethod(objPath, bstr_t("EnableStatic"), 0, NULL, pInParams, &pOutParams, NULL);
		pInParams->Release();

		if (SUCCEEDED(hr) && pOutParams) {
			hr = extractReturnValue(pOutParams);
			pOutParams->Release();
		}
		return hr;
	}

	HRESULT callSetGateways(BSTR objPath, std::wstring const &gateway)
	{
		IWbemClassObject *pClass = nullptr;
		HRESULT hr = m_pSvc->GetObject(bstr_t("Win32_NetworkAdapterConfiguration"), 0, NULL, &pClass, NULL);
		if (FAILED(hr)) return hr;

		IWbemClassObject *pInParamsDef = nullptr;
		hr = pClass->GetMethod(L"SetGateways", 0, &pInParamsDef, NULL);
		pClass->Release();
		if (FAILED(hr)) return hr;

		IWbemClassObject *pInParams = nullptr;
		hr = pInParamsDef->SpawnInstance(0, &pInParams);
		pInParamsDef->Release();
		if (FAILED(hr)) return hr;

		{
			VARIANT var;
			VariantInit(&var);
			var.vt = VT_ARRAY | VT_BSTR;
			std::vector<std::wstring> v = { gateway };
			var.parray = createStringSafeArray(v);
			hr = pInParams->Put(L"DefaultIPGateway", 0, &var, 0);
			VariantClear(&var);
			if (FAILED(hr)) {
				pInParams->Release();
				return hr;
			}
		}

		IWbemClassObject *pOutParams = nullptr;
		hr = m_pSvc->ExecMethod(objPath, bstr_t("SetGateways"), 0, NULL, pInParams, &pOutParams, NULL);
		pInParams->Release();

		if (SUCCEEDED(hr) && pOutParams) {
			hr = extractReturnValue(pOutParams);
			pOutParams->Release();
		}
		return hr;
	}

	static void reportMethodError(HRESULT hr, wchar_t const *methodName)
	{
		std::wcerr << methodName << L" failed. Error code = 0x" << std::hex << hr << std::endl;
		if (hr == E_ACCESSDENIED) {
			std::wcerr << L"Administrator privileges are required to change network settings." << std::endl;
		}
	}
};

// --- main2 (列挙処理) ---

int main2()
{
	NetworkConfig nc;
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
	NetworkConfig nc;
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

	return ok ? 0 : 1;
}
