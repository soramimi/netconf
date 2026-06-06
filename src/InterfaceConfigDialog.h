#ifndef INTERFACECONFIGDIALOG_H
#define INTERFACECONFIGDIALOG_H

#include "Win32NetworkConfig.h"

#include <QButtonGroup>
#include <QDialog>

namespace Ui {
class InterfaceConfigDialog;
}

class InterfaceConfigDialog : public QDialog {
	Q_OBJECT
public:
	struct Config {
		bool obtain_an_ip_address_automatically = true;
		QString ip_address;
		QString subnet_mask;
		QString default_gateway;

		bool obtain_dns_server_address_automatically = true;
		QString preferred_dns_server;
		QString alternate_dns_server;
	};

private:
	Ui::InterfaceConfigDialog *ui;
	Config config_ = {};

	QButtonGroup a_;
	QButtonGroup b_;

	void exchange(bool save);
	void reflectUI();

public:
	explicit InterfaceConfigDialog(Config const &config, QWidget *parent = nullptr);
	~InterfaceConfigDialog();

	Config const &config() const;

private slots:
	void on_radioButton_obtain_an_ip_address_automatically_clicked();
	void on_radioButton_use_the_following_ip_address_clicked();
	void on_radioButton_obtain_dns_server_address_automatically_clicked();
	void on_radioButton_use_the_following_dns_server_address_clicked();

	// QDialog interface

public slots:
	void done(int);
};

#endif // INTERFACECONFIGDIALOG_H
