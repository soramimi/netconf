#ifndef WIN32NETWORKCONFIG_H
#define WIN32NETWORKCONFIG_H

#include <map>
#include <string>
#include <vector>

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
		std::vector<std::wstring> dnsServers;
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
		bool obtainAutomatically = true;
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
	std::vector<Win32NetworkConfig::MsftNetAdapter> query_MSFT_NetAdapter(std::map<std::wstring, AdapterConfiguration> const &configurations);
	DnsConfig query_DNS_server(AdapterConfiguration const &configuration) const;
	void list_interfaces();
	bool change_address(std::wstring const &mac, std::wstring const &ip, std::wstring const &subnet, std::wstring const &gateway);
	bool change_DNS_server(std::wstring const &mac, DnsConfig const &dns);
};

#endif // WIN32NETWORKCONFIG_H
