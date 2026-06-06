#include "MainWindow.h"
#include "ui_MainWindow.h"
#include "Win32NetworkConfig.h"

#include <QMessageBox>

MainWindow::MainWindow(QWidget *parent)
	: QMainWindow(parent)
	, ui(new Ui::MainWindow)
	, nc_(std::make_shared<Win32NetworkConfig>())
{
	ui->setupUi(this);
}

MainWindow::~MainWindow()
{
	delete ui;
}

void MainWindow::show()
{
	QMainWindow::show();

	if (!nc_->open()) {
		QMessageBox::critical(this, "Error", "Failed to open network configuration.");
	}

	QStringList columns = {
		"Name",
		"IP Addresses",
		"Subnets",
		"Default Gateways",
		"DHCP Enabled",
		"MAC Address",
		"Description",
	};

	ui->tableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
	ui->tableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);

	ui->tableWidget->setColumnCount(columns.size());
	ui->tableWidget->setHorizontalHeaderLabels(columns);

	std::map<std::wstring, Win32NetworkConfig::AdapterConfiguration> configurations = nc_->query_Win32_NetworkAdapterConfiguration();
	std::vector<Win32NetworkConfig::MsftNetAdapter> adapters = nc_->query_MSFT_NetAdapter(configurations);
	std::sort(adapters.begin(), adapters.end(), [](const Win32NetworkConfig::MsftNetAdapter &a, const Win32NetworkConfig::MsftNetAdapter &b) {
		return a.name < b.name;
	});

	auto join = [&](std::vector<std::wstring> const &list, std::wstring const &sep)-> std::wstring {
		std::wstring result;
		for (size_t i = 0; i < list.size(); ++i) {
			if (i > 0) result += sep;
			result += list[i];
		}
		return result;
	};

	ui->tableWidget->setRowCount(static_cast<int>(adapters.size()));
	for (size_t i = 0; i < adapters.size(); ++i) {
		const auto &adapter = adapters[i];
		const auto &config = adapter.configuration;

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
		ui->tableWidget->setItem(int(i), col++, new QTableWidgetItem(config.dhcpEnabled ? "Yes" : "No"));
		ui->tableWidget->setItem(int(i), col++, new QTableWidgetItem(QString::fromStdWString(config.macAddress)));
		ui->tableWidget->setItem(int(i), col++, new QTableWidgetItem(QString::fromStdWString(config.description)));
	}

	ui->tableWidget->resizeColumnsToContents();
	ui->tableWidget->horizontalHeader()->setStretchLastSection(true);
}
