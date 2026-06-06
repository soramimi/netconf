#ifndef INTERFACECONGIFDIALOG_H
#define INTERFACECONGIFDIALOG_H

#include <QDialog>

namespace Ui {
class InterfaceCongifDialog;
}

class InterfaceCongifDialog : public QDialog {
	Q_OBJECT
public:
	struct Config {
		bool obtain_an_ipaddress_automatically;
		QString ip_address;
		QString subnet_mask;
		QString default_gateway;

		bool obtain_dns_server_address_automatically;
		QString preferred_dns_server;
		QString alternate_dns_server;
	};
private:
	Ui::InterfaceCongifDialog *ui;
	Config config_ = {};

	void exchange(bool save);
	void reflect_ui();
public:
	explicit InterfaceCongifDialog(Config const &config, QWidget *parent = nullptr);
	~InterfaceCongifDialog();

	Config const &config() const;

private slots:
	void on_radioButton_obtain_an_ipaddress_automatically_clicked();
	void on_radioButton_use_the_following_ip_address_clicked();
	void on_radioButton_obtain_dns_server_address_automatically_clicked();
	void on_radioButton_use_the_following_dns_server_address_clicked();

	// QDialog interface
public slots:
	void done(int);
};

#endif // INTERFACECONGIFDIALOG_H
