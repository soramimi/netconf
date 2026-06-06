# netconf

netconf is a small Windows desktop utility for viewing and editing network interface configuration.

The project was created because the standard dotted IP address edit control is hard to use. For many network configuration tasks, a normal single-line edit control is simpler and more comfortable, especially when copying, pasting, or replacing an entire address.

## Features

- Lists Windows network interfaces in a Qt Widgets table.
- Uses `MSFT_NetAdapter` for the interface list, which closely matches the adapters shown in Windows network connection views.
- Uses `Win32_NetworkAdapterConfiguration` for IP configuration details.
- Links adapters and configuration records by `MSFT_NetAdapter.InterfaceGuid` and `Win32_NetworkAdapterConfiguration.SettingID`.
- Shows IP addresses, subnet masks, default gateways, DNS servers, DHCP state, MAC address, and adapter description.
- Opens an edit dialog by double-clicking an interface row.
- Updates static IPv4 address, subnet mask, default gateway, and DNS server settings through WMI.
- Supports DNS automatic mode by clearing `DNSServerSearchOrder`.

## Implementation Notes

The WMI-related code is isolated in `Win32NetworkConfig`.

- `ROOT\StandardCimv2:MSFT_NetAdapter` is used for adapter enumeration.
- `ROOT\CIMV2:Win32_NetworkAdapterConfiguration` is used for IP, gateway, DNS, and DHCP-related configuration.
- Static IP changes use `EnableStatic`.
- Gateway changes use `SetGateways`.
- DNS changes use `SetDNSServerSearchOrder`.

The GUI is built with Qt Widgets:

- `MainWindow` displays the adapter table and reloads data through `reloadAdapters()`.
- `InterfaceConfigDialog` provides normal single-line edit controls for IP address, subnet mask, gateway, and DNS servers.

## Build

This project uses qmake.

```powershell
cd qmake\build\Desktop_Qt_6_11_0_MSVC2022_64bit-Debug
nmake
```

The executable is written to:

```text
_bin\netconf.exe
```

## Permissions

Viewing adapter information should work as a normal user. Applying IP, gateway, or DNS changes generally requires running the application with administrator privileges.

## Platform

This project is Windows-only. It depends on WMI, COM, and Qt Widgets.
