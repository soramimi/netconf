#include "Win32NetworkConfig.h"

#define _WIN32_DCOM
#include <Wbemidl.h>
#include <comdef.h>
#include <cstdlib>
#include <cwctype>
#include <iostream>
#include <map>

namespace {

std::wstring normalizeGuid(std::wstring value)
{
	for (wchar_t &ch : value) {
		ch = static_cast<wchar_t>(std::towupper(ch));
	}
	return value;
}

std::wstring getStringProperty(IWbemClassObject *pclsObj, wchar_t const *name)
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

ULONG getUInt32Property(IWbemClassObject *pclsObj, wchar_t const *name)
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

bool getBoolProperty(IWbemClassObject *pclsObj, wchar_t const *name)
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

bool getUInt64Property(IWbemClassObject *pclsObj, wchar_t const *name, ULONGLONG &value)
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

std::vector<std::wstring> getStringArrayProperty(IWbemClassObject *pclsObj, wchar_t const *name)
{
	VARIANT vtProp;
	VariantInit(&vtProp);
	std::vector<std::wstring> values;
	HRESULT hr = pclsObj->Get(name, 0, &vtProp, 0, 0);
	if (SUCCEEDED(hr) && (vtProp.vt & VT_ARRAY) && vtProp.parray) {
		LONG lower = 0;
		LONG upper = -1;
		if (SUCCEEDED(SafeArrayGetLBound(vtProp.parray, 1, &lower)) && SUCCEEDED(SafeArrayGetUBound(vtProp.parray, 1, &upper))) {
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

void printStringList(wchar_t const *label, std::vector<std::wstring> const &values)
{
	if (values.empty()) {
		std::wcout << label << L": (null)" << std::endl;
		return;
	}
	for (std::wstring const &value : values) {
		std::wcout << label << L": " << value << std::endl;
	}
}

void printConfiguration(Win32NetworkConfig::AdapterConfiguration const &config)
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
	printStringList(L"ConfigurationDNSServer", config.dnsServers);
}

void printStringProperty(wchar_t const *label, std::wstring const &value)
{
	std::wcout << label << L": " << (value.empty() ? L"(null)" : value) << std::endl;
}

void printUInt64Property(wchar_t const *label, ULONGLONG value, bool hasValue)
{
	std::wcout << label << L": ";
	if (hasValue) {
		std::wcout << value;
	} else {
		std::wcout << L"(null)";
	}
	std::wcout << std::endl;
}

void printMsftNetAdapter(Win32NetworkConfig::MsftNetAdapter const &adapter)
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

void printArrayProperty(IWbemClassObject *pclsObj, wchar_t const *name)
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

void printProperty(IWbemClassObject *pclsObj, wchar_t const *propName)
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

HRESULT extractReturnValue(IWbemClassObject *pOutParams)
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

void reportMethodError(HRESULT hr, wchar_t const *methodName)
{
	std::wcerr << methodName << L" failed. Error code = 0x" << std::hex << hr << std::endl;
	if (hr == E_ACCESSDENIED) {
		std::wcerr << L"Administrator privileges are required to change network settings." << std::endl;
	}
}

} // namespace

// Win32NetworkConfig::Private

struct Win32NetworkConfig::Private {

	IWbemLocator *pLoc = nullptr;
	IWbemServices *pSvc = nullptr;
	IWbemServices *pStdSvc = nullptr;

	bool connectNamespace(wchar_t const *namespacePath, IWbemServices **service);
	static SAFEARRAY *createStringSafeArray(std::vector<std::wstring> const &strings);
	HRESULT callEnableStatic(BSTR objPath, std::wstring const &ip, std::wstring const &subnet);
	HRESULT callSetGateways(BSTR objPath, std::wstring const &gateway);

	template <typename Func> void enumerate(IWbemServices *service, wchar_t const *wql, Func callback)
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

	template <typename Func> void enumerate(wchar_t const *wql, Func callback)
	{
		enumerate<Func>(pSvc, wql, callback);
	}
};

bool Win32NetworkConfig::Private::connectNamespace(wchar_t const *namespacePath, IWbemServices **service)
{
	HRESULT hres = pLoc->ConnectServer(
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

SAFEARRAY *Win32NetworkConfig::Private::createStringSafeArray(std::vector<std::wstring> const &strings)
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

HRESULT Win32NetworkConfig::Private::callEnableStatic(BSTR objPath, std::wstring const &ip, std::wstring const &subnet)
{
	IWbemClassObject *pClass = nullptr;
	HRESULT hr = pSvc->GetObject(bstr_t("Win32_NetworkAdapterConfiguration"), 0, NULL, &pClass, NULL);
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
	hr = pSvc->ExecMethod(objPath, bstr_t("EnableStatic"), 0, NULL, pInParams, &pOutParams, NULL);
	pInParams->Release();

	if (SUCCEEDED(hr) && pOutParams) {
		hr = extractReturnValue(pOutParams);
		pOutParams->Release();
	}
	return hr;
}

HRESULT Win32NetworkConfig::Private::callSetGateways(BSTR objPath, std::wstring const &gateway)
{
	IWbemClassObject *pClass = nullptr;
	HRESULT hr = pSvc->GetObject(bstr_t("Win32_NetworkAdapterConfiguration"), 0, NULL, &pClass, NULL);
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
	hr = pSvc->ExecMethod(objPath, bstr_t("SetGateways"), 0, NULL, pInParams, &pOutParams, NULL);
	pInParams->Release();

	if (SUCCEEDED(hr) && pOutParams) {
		hr = extractReturnValue(pOutParams);
		pOutParams->Release();
	}
	return hr;
}

// Win32NetworkConfig

Win32NetworkConfig::Win32NetworkConfig()
	: m(new Private)
{
}

Win32NetworkConfig::~Win32NetworkConfig()
{
	close();
	delete m;
}

bool Win32NetworkConfig::open()
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
		(LPVOID *)&m->pLoc);
	if (FAILED(hres)) {
		std::cerr << "CoCreateInstance(WbemLocator) failed. Error code = 0x" << std::hex << hres << std::endl;
		CoUninitialize();
		return false;
	}

	if (!m->connectNamespace(L"ROOT\\CIMV2", &m->pSvc)) {
		close();
		return false;
	}

	if (!m->connectNamespace(L"ROOT\\StandardCimv2", &m->pStdSvc)) {
		close();
		return false;
	}

	return true;
}

void Win32NetworkConfig::close()
{
	if (m->pStdSvc) {
		m->pStdSvc->Release();
		m->pStdSvc = nullptr;
	}
	if (m->pSvc) {
		m->pSvc->Release();
		m->pSvc = nullptr;
	}
	if (m->pLoc) {
		m->pLoc->Release();
		m->pLoc = nullptr;
	}
}

std::map<std::wstring, Win32NetworkConfig::AdapterConfiguration> Win32NetworkConfig::query_Win32_NetworkAdapterConfiguration()
{
	std::map<std::wstring, AdapterConfiguration> configurations;
	m->enumerate(L"SELECT * FROM Win32_NetworkAdapterConfiguration",
		[&](IWbemClassObject *pclsObj) {
			AdapterConfiguration config;
			config.settingId = getStringProperty(pclsObj, L"SettingID");
			config.index = getUInt32Property(pclsObj, L"Index");
			config.interfaceIndex = getUInt32Property(pclsObj, L"InterfaceIndex");
			config.macAddress = getStringProperty(pclsObj, L"MACAddress");
			config.ipAddresses = getStringArrayProperty(pclsObj, L"IPAddress");
			config.defaultGateways = getStringArrayProperty(pclsObj, L"DefaultIPGateway");
			config.subnets = getStringArrayProperty(pclsObj, L"IPSubnet");
			config.dnsServers = getStringArrayProperty(pclsObj, L"DNSServerSearchOrder");
			config.description = getStringProperty(pclsObj, L"Description");
			config.dhcpEnabled = getBoolProperty(pclsObj, L"DHCPEnabled");
			if (!config.settingId.empty()) {
				configurations[normalizeGuid(config.settingId)] = config;
			}
		});
	return configurations;
}

std::vector<Win32NetworkConfig::MsftNetAdapter> Win32NetworkConfig::query_MSFT_NetAdapter(std::map<std::wstring, AdapterConfiguration> const &configurations)
{
	std::vector<MsftNetAdapter> netadapters;
	m->enumerate(m->pStdSvc, L"SELECT * FROM MSFT_NetAdapter",
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
	return netadapters;
}

Win32NetworkConfig::DnsConfig Win32NetworkConfig::query_DNS_server(AdapterConfiguration const &configuration) const
{
	DnsConfig dns;
	if (!configuration.dnsServers.empty()) {
		dns.ipv4.preferredDnsServer = configuration.dnsServers[0];
	}
	if (configuration.dnsServers.size() > 1) {
		dns.ipv4.alternateDnsServer = configuration.dnsServers[1];
	}
	return dns;
}

void Win32NetworkConfig::list_interfaces()
{
	std::map<std::wstring, AdapterConfiguration> configurations = query_Win32_NetworkAdapterConfiguration();
	std::vector<MsftNetAdapter> netadapters = query_MSFT_NetAdapter(configurations);

	std::wcout << L"--- MSFT Net Adapter ---" << std::endl;
	for (MsftNetAdapter const &adapter : netadapters) {
		printMsftNetAdapter(adapter);
		std::wcout << L"-------------------------" << std::endl;
	}
}

bool Win32NetworkConfig::change_address(std::wstring const &mac, std::wstring const &ip, std::wstring const &subnet, std::wstring const &gateway)
{
	std::wstring query = L"SELECT * FROM Win32_NetworkAdapterConfiguration WHERE MACAddress = '" + mac + L"'";
	bool found = false;

	m->enumerate(query.c_str(), [&](IWbemClassObject *pclsObj) {
		if (found) return;

		VARIANT vtPath;
		VariantInit(&vtPath);
		HRESULT hr = pclsObj->Get(L"__PATH", 0, &vtPath, 0, 0);
		if (SUCCEEDED(hr) && vtPath.vt == VT_BSTR) {
			BSTR objPath = SysAllocString(vtPath.bstrVal);

			hr = m->callEnableStatic(objPath, ip, subnet);
			if (SUCCEEDED(hr)) {
				hr = m->callSetGateways(objPath, gateway);
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
