#include "MainWindow.h"
#include "InterfaceConfigDialog.h"
#include "Win32NetworkConfig.h"
#include "ui_MainWindow.h"

#include <QMessageBox>

struct MainWindow::Private {
	std::shared_ptr<Win32NetworkConfig> netconfig;
	std::map<std::wstring, Win32NetworkConfig::AdapterConfiguration> configurations;
	std::vector<Win32NetworkConfig::MsftNetAdapter> adapters;
	bool opened = false;
};

MainWindow::MainWindow(QWidget *parent)
	: QMainWindow(parent)
	, ui(new Ui::MainWindow)
	, m(new Private)
{
	ui->setupUi(this);
	m->netconfig = std::make_shared<Win32NetworkConfig>();
}

MainWindow::~MainWindow()
{
	if (m) {
		if (m->netconfig) {
			m->netconfig->close();
		}
		delete m;
		m = nullptr;
	}
	delete ui;
}

void MainWindow::show()
{
	QMainWindow::show();

	if (!m->opened && !m->netconfig->open()) {
		QMessageBox::critical(this, "Error", "Failed to open network configuration.");
		return;
	}
	m->opened = true;

	QStringList columns = {
		"Name",
		"IP Addresses",
		"Subnets",
		"Default Gateways",
		"DNS Servers",
		"DHCP Enabled",
		"MAC Address",
		"Description",
	};

	ui->tableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
	ui->tableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);

	ui->tableWidget->setColumnCount(columns.size());
	ui->tableWidget->setHorizontalHeaderLabels(columns);

	reloadAdapters();
}

void MainWindow::reloadAdapters()
{
	m->configurations = m->netconfig->query_Win32_NetworkAdapterConfiguration();
	m->adapters = m->netconfig->query_MSFT_NetAdapter(m->configurations);
	std::sort(m->adapters.begin(), m->adapters.end(), [](Win32NetworkConfig::MsftNetAdapter const &a, Win32NetworkConfig::MsftNetAdapter const &b) {
		return a.name < b.name;
	});

	auto join = [&](std::vector<std::wstring> const &list, std::wstring const &sep) -> std::wstring {
		std::wstring result;
		for (size_t i = 0; i < list.size(); ++i) {
			if (i > 0) result += sep;
			result += list[i];
		}
		return result;
	};

	ui->tableWidget->setRowCount(static_cast<int>(m->adapters.size()));
	for (size_t i = 0; i < m->adapters.size(); ++i) {
		auto const &adapter = m->adapters[i];
		auto const &config = adapter.configuration;

		int col = 0;
		ui->tableWidget->setItem(int(i), col++, new QTableWidgetItem(QString::fromStdWString(adapter.name)));
		QString ipaddr;
		if (config.ipAddresses.size() == 1) {
			ipaddr = QString::fromStdWString(config.ipAddresses[0]);
		} else if (config.ipAddresses.size() > 1) {
			ipaddr = QString::fromStdWString(config.ipAddresses[0]);
			ipaddr += " and " + QString::number(config.ipAddresses.size() - 1) + " more";
		}
		ui->tableWidget->setItem(int(i), col++, new QTableWidgetItem(ipaddr));
		ui->tableWidget->setItem(int(i), col++, new QTableWidgetItem(QString::fromStdWString(join(config.subnets, L", "))));
		ui->tableWidget->setItem(int(i), col++, new QTableWidgetItem(QString::fromStdWString(join(config.defaultGateways, L", "))));
		ui->tableWidget->setItem(int(i), col++, new QTableWidgetItem(QString::fromStdWString(join(config.dnsServers, L", "))));
		ui->tableWidget->setItem(int(i), col++, new QTableWidgetItem(config.dhcpEnabled ? "Yes" : "No"));
		ui->tableWidget->setItem(int(i), col++, new QTableWidgetItem(QString::fromStdWString(config.macAddress)));
		ui->tableWidget->setItem(int(i), col++, new QTableWidgetItem(QString::fromStdWString(config.description)));
	}

	ui->tableWidget->resizeColumnsToContents();
	ui->tableWidget->horizontalHeader()->setStretchLastSection(true);
}

void MainWindow::on_tableWidget_itemDoubleClicked(QTableWidgetItem *item)
{
	InterfaceConfigDialog::Config config;
	int row = item->row();
	auto const &adapter = m->adapters[row];
	config.obtain_an_ip_address_automatically = adapter.configuration.dhcpEnabled;
	if (!adapter.configuration.ipAddresses.empty()) {
		config.ip_address = QString::fromStdWString(adapter.configuration.ipAddresses[0]);
	}
	if (!adapter.configuration.subnets.empty()) {
		config.subnet_mask = QString::fromStdWString(adapter.configuration.subnets[0]);
	}
	if (!adapter.configuration.defaultGateways.empty()) {
		config.default_gateway = QString::fromStdWString(adapter.configuration.defaultGateways[0]);
	}
	Win32NetworkConfig::DnsConfig dns = m->netconfig->query_DNS_server(adapter.configuration);
	config.obtain_dns_server_address_automatically = dns.ipv4.preferredDnsServer.empty();
	config.preferred_dns_server = QString::fromStdWString(dns.ipv4.preferredDnsServer);
	config.alternate_dns_server = QString::fromStdWString(dns.ipv4.alternateDnsServer);

	InterfaceConfigDialog dlg(config, this);
	if (dlg.exec() == QDialog::Accepted) {
		auto const &new_config = dlg.config();
		std::wstring mac = adapter.configuration.macAddress;
		std::wstring ip = new_config.ip_address.toStdWString();
		std::wstring subnet = new_config.subnet_mask.toStdWString();
		std::wstring gateway = new_config.default_gateway.toStdWString();

		bool success = m->netconfig->change_address(mac, ip, subnet, gateway);
		if (!success) {
			QMessageBox::critical(this, "Error", "Failed to change IP address.");
		} else {
			QMessageBox::information(this, "Success", "IP address changed successfully.");
			reloadAdapters();
		}
	}
}
