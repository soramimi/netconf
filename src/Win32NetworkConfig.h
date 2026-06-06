#ifndef WIN32NETWORKCONFIG_H
#define WIN32NETWORKCONFIG_H

#include <string>
#include <vector>
#include <map>

class Win32NetworkConfig {
public:

	struct AdapterConfiguration {
		std::wstring settingId;
		unsigned long index = 0;
		unsigned long interfaceIndex = 0;
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
		unsigned long interfaceIndex = 0;
		std::wstring status;
		unsigned long mediaConnectState = 0;
		bool hardwareInterface = false;
		bool virtualAdapter = false;
		bool hidden = false;
		bool connectorPresent = false;
		std::wstring permanentAddress;
		unsigned long long receiveLinkSpeed = 0;
		bool hasReceiveLinkSpeed = false;
		unsigned long long transmitLinkSpeed = 0;
		bool hasTransmitLinkSpeed = false;
		std::wstring pnpDeviceId;
		Win32NetworkConfig::AdapterConfiguration configuration;
		bool hasConfiguration = false;
	};

	struct DnsConfig {
		struct {
			std::wstring preferredDnsServer;
			std::wstring alternateDnsServer;
		} ipv4;
	};

private:
	struct Private;
	Private *m;
public:
	Win32NetworkConfig();
	~Win32NetworkConfig();
	bool open();
	void close();
	std::map<std::wstring, AdapterConfiguration> query_Win32_NetworkAdapterConfiguration();
	std::vector<Win32NetworkConfig::MsftNetAdapter> query_MSFT_NetAdapter(const std::map<std::wstring, AdapterConfiguration> &configurations);
	void list_interfaces();
	bool change_address(std::wstring const &mac, std::wstring const &ip, std::wstring const &subnet, std::wstring const &gateway);
};

#endif // WIN32NETWORKCONFIG_H
