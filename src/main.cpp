#define _WIN32_DCOM
#include <Wbemidl.h>
#include <comdef.h>
#include <cstdlib>
#include <cwctype>
#include <cstring>
#include <iostream>
#include <locale>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>
#include <windows.h>

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

class NetworkConfig {
public:
	NetworkConfig() = default;

	~NetworkConfig()
	{
		close();
	}

	bool open()
	{
		HRESULT hres;
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

		if (!connectNamespace(L"ROOT\\CIMV2", &m_pSvc)) {
			close();
			return false;
		}

		if (!connectNamespace(L"ROOT\\StandardCimv2", &m_pStdSvc)) {
			close();
			return false;
		}

		return true;
	}

	void close()
	{
		if (m_pStdSvc) {
			m_pStdSvc->Release();
			m_pStdSvc = nullptr;
		}
		if (m_pSvc) {
			m_pSvc->Release();
			m_pSvc = nullptr;
		}
		if (m_pLoc) {
			m_pLoc->Release();
			m_pLoc = nullptr;
		}
	}

	void list_interfaces()
	{
		std::map<std::wstring, AdapterConfiguration> configurations;
		enumerate(L"SELECT * FROM Win32_NetworkAdapterConfiguration",
				  [&](IWbemClassObject *pclsObj) {
			AdapterConfiguration config;
			config.settingId = getStringProperty(pclsObj, L"SettingID");
			config.index = getUInt32Property(pclsObj, L"Index");
			config.interfaceIndex = getUInt32Property(pclsObj, L"InterfaceIndex");
			config.macAddress = getStringProperty(pclsObj, L"MACAddress");
			config.ipAddresses = getStringArrayProperty(pclsObj, L"IPAddress");
			config.defaultGateways = getStringArrayProperty(pclsObj, L"DefaultIPGateway");
			config.subnets = getStringArrayProperty(pclsObj, L"IPSubnet");
			config.description = getStringProperty(pclsObj, L"Description");
			config.dhcpEnabled = getBoolProperty(pclsObj, L"DHCPEnabled");
			if (!config.settingId.empty()) {
				configurations[normalizeGuid(config.settingId)] = config;
			}
		});

		std::vector<MsftNetAdapter> netadapters;
		enumerate(m_pStdSvc, L"SELECT * FROM MSFT_NetAdapter",
				  [&](IWbemClassObject *pclsObj) {
			MsftNetAdapter adapter;
			adapter.name = getStringProperty(pclsObj, L"Name");
			adapter.interfaceDescription = getStringProperty(pclsObj, L"InterfaceDescription");
			adapter.interfaceGuid = getStringProperty(pclsObj, L"InterfaceGuid");
			adapter.interfaceIndex = getUInt32Property(pclsObj, L"InterfaceIndex");
			adapter.status = getStringProperty(pclsObj, L"Status");
			adapter.mediaConnectState = getUInt32Property(pclsObj, L"MediaConnectState");
			adapter.hardwareInterface = getBoolProperty(pclsObj, L"HardwareInterface");
			adapter.virtualAdapter = getBoolProperty(pclsObj, L"Virtual");
			adapter.hidden = getBoolProperty(pclsObj, L"Hidden");
			adapter.connectorPresent = getBoolProperty(pclsObj, L"ConnectorPresent");
			adapter.permanentAddress = getStringProperty(pclsObj, L"PermanentAddress");
			adapter.hasReceiveLinkSpeed = getUInt64Property(pclsObj, L"ReceiveLinkSpeed", adapter.receiveLinkSpeed);
			adapter.hasTransmitLinkSpeed = getUInt64Property(pclsObj, L"TransmitLinkSpeed", adapter.transmitLinkSpeed);
			adapter.pnpDeviceId = getStringProperty(pclsObj, L"PnPDeviceID");

			auto config = configurations.find(normalizeGuid(adapter.interfaceGuid));
			if (config != configurations.end()) {
				adapter.hasConfiguration = true;
				adapter.configuration = config->second;
			}
			netadapters.push_back(adapter);
		});

		std::wcout << L"--- MSFT Net Adapter ---" << std::endl;
		for (MsftNetAdapter const &adapter : netadapters) {
			printMsftNetAdapter(adapter);
			std::wcout << L"-------------------------" << std::endl;
		}
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
	IWbemServices *m_pStdSvc = nullptr;

	struct AdapterConfiguration {
		std::wstring settingId;
		ULONG index = 0;
		ULONG interfaceIndex = 0;
		std::wstring macAddress;
		std::vector<std::wstring> ipAddresses;
		std::vector<std::wstring> defaultGateways;
		std::vector<std::wstring> subnets;
		std::wstring description;
		bool dhcpEnabled = false;
	};

	struct MsftNetAdapter {
		std::wstring name;
		std::wstring interfaceDescription;
		std::wstring interfaceGuid;
		ULONG interfaceIndex = 0;
		std::wstring status;
		ULONG mediaConnectState = 0;
		bool hardwareInterface = false;
		bool virtualAdapter = false;
		bool hidden = false;
		bool connectorPresent = false;
		std::wstring permanentAddress;
		ULONGLONG receiveLinkSpeed = 0;
		bool hasReceiveLinkSpeed = false;
		ULONGLONG transmitLinkSpeed = 0;
		bool hasTransmitLinkSpeed = false;
		std::wstring pnpDeviceId;
		AdapterConfiguration configuration;
		bool hasConfiguration = false;
	};

	bool connectNamespace(wchar_t const *namespacePath, IWbemServices **service)
	{
		HRESULT hres = m_pLoc->ConnectServer(
					_bstr_t(namespacePath),
					NULL,
					NULL,
					0,
					NULL,
					0,
					0,
					service);
		if (FAILED(hres)) {
			std::wcerr << L"ConnectServer(" << namespacePath << L") failed. Error code = 0x" << std::hex << hres << std::endl;
			return false;
		}

		hres = CoSetProxyBlanket(
					*service,
					RPC_C_AUTHN_WINNT,
					RPC_C_AUTHZ_NONE,
					NULL,
					RPC_C_AUTHN_LEVEL_CALL,
					RPC_C_IMP_LEVEL_IMPERSONATE,
					NULL,
					EOAC_NONE);
		if (FAILED(hres)) {
			std::wcerr << L"CoSetProxyBlanket(" << namespacePath << L") failed. Error code = 0x" << std::hex << hres << std::endl;
			(*service)->Release();
			*service = nullptr;
			return false;
		}

		return true;
	}

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
		enumerate(m_pSvc, wql, callback);
	}

	template <typename Func>
	void enumerate(IWbemServices *service, wchar_t const *wql, Func callback)
	{
		IEnumWbemClassObject *pEnumerator = nullptr;
		if (!service) return;
		HRESULT hres = service->ExecQuery(
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

	static std::wstring normalizeGuid(std::wstring value)
	{
		for (wchar_t &ch : value) {
			ch = static_cast<wchar_t>(std::towupper(ch));
		}
		return value;
	}

	static std::wstring getStringProperty(IWbemClassObject *pclsObj, wchar_t const *name)
	{
		VARIANT vtProp;
		VariantInit(&vtProp);
		std::wstring value;
		HRESULT hr = pclsObj->Get(name, 0, &vtProp, 0, 0);
		if (SUCCEEDED(hr) && vtProp.vt == VT_BSTR && vtProp.bstrVal) {
			value = vtProp.bstrVal;
		}
		VariantClear(&vtProp);
		return value;
	}

	static ULONG getUInt32Property(IWbemClassObject *pclsObj, wchar_t const *name)
	{
		VARIANT vtProp;
		VariantInit(&vtProp);
		ULONG value = 0;
		HRESULT hr = pclsObj->Get(name, 0, &vtProp, 0, 0);
		if (SUCCEEDED(hr)) {
			switch (vtProp.vt) {
			case VT_UI4:
				value = vtProp.ulVal;
				break;
			case VT_I4:
				value = static_cast<ULONG>(vtProp.lVal);
				break;
			case VT_UI2:
				value = vtProp.uiVal;
				break;
			case VT_I2:
				value = static_cast<ULONG>(vtProp.iVal);
				break;
			default:
				break;
			}
		}
		VariantClear(&vtProp);
		return value;
	}

	static bool getBoolProperty(IWbemClassObject *pclsObj, wchar_t const *name)
	{
		VARIANT vtProp;
		VariantInit(&vtProp);
		bool value = false;
		HRESULT hr = pclsObj->Get(name, 0, &vtProp, 0, 0);
		if (SUCCEEDED(hr) && vtProp.vt == VT_BOOL) {
			value = (vtProp.boolVal == VARIANT_TRUE);
		}
		VariantClear(&vtProp);
		return value;
	}

	static bool getUInt64Property(IWbemClassObject *pclsObj, wchar_t const *name, ULONGLONG &value)
	{
		VARIANT vtProp;
		VariantInit(&vtProp);
		bool found = false;
		HRESULT hr = pclsObj->Get(name, 0, &vtProp, 0, 0);
		if (SUCCEEDED(hr)) {
			switch (vtProp.vt) {
			case VT_UI8:
				value = vtProp.ullVal;
				found = true;
				break;
			case VT_I8:
				value = static_cast<ULONGLONG>(vtProp.llVal);
				found = true;
				break;
			case VT_UI4:
				value = vtProp.ulVal;
				found = true;
				break;
			case VT_I4:
				value = static_cast<ULONGLONG>(vtProp.lVal);
				found = true;
				break;
			case VT_BSTR:
				if (vtProp.bstrVal && vtProp.bstrVal[0] != L'\0') {
					wchar_t *end = nullptr;
					ULONGLONG parsed = std::wcstoull(vtProp.bstrVal, &end, 10);
					if (end && *end == L'\0') {
						value = parsed;
						found = true;
					}
				}
				break;
			default:
				break;
			}
		}
		VariantClear(&vtProp);
		return found;
	}

	static std::vector<std::wstring> getStringArrayProperty(IWbemClassObject *pclsObj, wchar_t const *name)
	{
		VARIANT vtProp;
		VariantInit(&vtProp);
		std::vector<std::wstring> values;
		HRESULT hr = pclsObj->Get(name, 0, &vtProp, 0, 0);
		if (SUCCEEDED(hr) && (vtProp.vt & VT_ARRAY) && vtProp.parray) {
			LONG lower = 0;
			LONG upper = -1;
			if (SUCCEEDED(SafeArrayGetLBound(vtProp.parray, 1, &lower)) &&
					SUCCEEDED(SafeArrayGetUBound(vtProp.parray, 1, &upper))) {
				for (LONG i = lower; i <= upper; ++i) {
					BSTR item = nullptr;
					if (SUCCEEDED(SafeArrayGetElement(vtProp.parray, &i, &item)) && item) {
						values.emplace_back(item);
						SysFreeString(item);
					}
				}
			}
		}
		VariantClear(&vtProp);
		return values;
	}

	static void printStringList(wchar_t const *label, std::vector<std::wstring> const &values)
	{
		if (values.empty()) {
			std::wcout << label << L": (null)" << std::endl;
			return;
		}
		for (std::wstring const &value : values) {
			std::wcout << label << L": " << value << std::endl;
		}
	}

	static void printConfiguration(AdapterConfiguration const &config)
	{
		std::wcout << L"ConfigurationSettingID: " << config.settingId << std::endl;
		std::wcout << L"ConfigurationIndex: " << config.index << std::endl;
		std::wcout << L"ConfigurationInterfaceIndex: " << config.interfaceIndex << std::endl;
		std::wcout << L"ConfigurationDescription: "
				   << (config.description.empty() ? L"(null)" : config.description)
				   << std::endl;
		std::wcout << L"ConfigurationMACAddress: "
				   << (config.macAddress.empty() ? L"(null)" : config.macAddress)
				   << std::endl;
		std::wcout << L"ConfigurationDHCPEnabled: " << (config.dhcpEnabled ? L"Yes" : L"No") << std::endl;
		printStringList(L"ConfigurationIPAddress", config.ipAddresses);
		printStringList(L"ConfigurationDefaultIPGateway", config.defaultGateways);
		printStringList(L"ConfigurationIPSubnet", config.subnets);
	}

	static void printStringProperty(wchar_t const *label, std::wstring const &value)
	{
		std::wcout << label << L": " << (value.empty() ? L"(null)" : value) << std::endl;
	}

	static void printUInt64Property(wchar_t const *label, ULONGLONG value, bool hasValue)
	{
		std::wcout << label << L": ";
		if (hasValue) {
			std::wcout << value;
		} else {
			std::wcout << L"(null)";
		}
		std::wcout << std::endl;
	}

	static void printMsftNetAdapter(MsftNetAdapter const &adapter)
	{
		printStringProperty(L"Name", adapter.name);
		printStringProperty(L"InterfaceDescription", adapter.interfaceDescription);
		printStringProperty(L"InterfaceGuid", adapter.interfaceGuid);
		std::wcout << L"InterfaceIndex: " << adapter.interfaceIndex << std::endl;
		printStringProperty(L"Status", adapter.status);
		std::wcout << L"MediaConnectState: " << adapter.mediaConnectState << std::endl;
		std::wcout << L"HardwareInterface: " << (adapter.hardwareInterface ? L"Yes" : L"No") << std::endl;
		std::wcout << L"Virtual: " << (adapter.virtualAdapter ? L"Yes" : L"No") << std::endl;
		std::wcout << L"Hidden: " << (adapter.hidden ? L"Yes" : L"No") << std::endl;
		std::wcout << L"ConnectorPresent: " << (adapter.connectorPresent ? L"Yes" : L"No") << std::endl;
		printStringProperty(L"PermanentAddress", adapter.permanentAddress);
		printUInt64Property(L"ReceiveLinkSpeed", adapter.receiveLinkSpeed, adapter.hasReceiveLinkSpeed);
		printUInt64Property(L"TransmitLinkSpeed", adapter.transmitLinkSpeed, adapter.hasTransmitLinkSpeed);
		printStringProperty(L"PnPDeviceID", adapter.pnpDeviceId);
		if (adapter.hasConfiguration) {
			printConfiguration(adapter.configuration);
		} else {
			std::wcout << L"Configuration: (not found)" << std::endl;
		}
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

	static void printProperty(IWbemClassObject *pclsObj, wchar_t const *propName)
	{
		VARIANT vtProp;
		VariantInit(&vtProp);
		HRESULT hr = pclsObj->Get(propName, 0, &vtProp, 0, 0);
		if (FAILED(hr)) {
			VariantClear(&vtProp);
			return;
		}
		std::wcout << propName << L": ";
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
	setupOutputLocale();

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
	HRESULT hres = CoInitializeEx(0, COINIT_MULTITHREADED);
	if (!SUCCEEDED(hres)) {
		std::cerr << "Failed to initialize COM library. Error code = 0x" << std::hex << hres << std::endl;
		return 1;
	}

	setupOutputLocale();

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

	CoUninitialize();

	return ok ? 0 : 1;
}
