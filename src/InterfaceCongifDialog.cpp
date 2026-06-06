#include "InterfaceCongifDialog.h"
#include "ui_InterfaceCongifDialog.h"

InterfaceCongifDialog::InterfaceCongifDialog(const Config &config, QWidget *parent)
	: QDialog(parent)
	, ui(new Ui::InterfaceCongifDialog)
	, config_(config)
{
	ui->setupUi(this);

	exchange(false);
}

InterfaceCongifDialog::~InterfaceCongifDialog()
{
	delete ui;
}

const InterfaceCongifDialog::Config &InterfaceCongifDialog::config() const
{
	return config_;
}

void InterfaceCongifDialog::exchange(bool save)
{
	if (save) {
		config_.obtain_an_ipaddress_automatically = ui->radioButton_obtain_an_ipaddress_automatically->isChecked();
		config_.ip_address = ui->lineEdit_ip_address->text();
		config_.subnet_mask = ui->lineEdit_subnet_mask->text();
		config_.default_gateway = ui->lineEdit_default_gateway->text();

		config_.obtain_dns_server_address_automatically = ui->radioButton_obtain_dns_server_address_automatically->isChecked();
		config_.preferred_dns_server = ui->lineEdit_preferred_dns_server->text();
		config_.alternate_dns_server = ui->lineEdit_alternate_dns_server->text();
	} else {
		ui->radioButton_obtain_an_ipaddress_automatically->setChecked(config_.obtain_an_ipaddress_automatically);
		ui->lineEdit_ip_address->setText(config_.ip_address);
		ui->lineEdit_subnet_mask->setText(config_.subnet_mask);
		ui->lineEdit_default_gateway->setText(config_.default_gateway);

		ui->radioButton_obtain_dns_server_address_automatically->setChecked(config_.obtain_dns_server_address_automatically);
		ui->lineEdit_preferred_dns_server->setText(config_.preferred_dns_server);
		ui->lineEdit_alternate_dns_server->setText(config_.alternate_dns_server);

		reflect_ui();
	}
}

void InterfaceCongifDialog::done(int status)
{
	if (status == QDialog::Accepted) {
		exchange(true);
	}
	QDialog::done(status);
}

void InterfaceCongifDialog::reflect_ui()
{
	if (ui->radioButton_obtain_an_ipaddress_automatically->isChecked()) {
		ui->frame_ip_address->setEnabled(false);
	} else if (ui->radioButton_use_the_following_ip_address->isChecked()) {
		ui->frame_ip_address->setEnabled(true);
	}

	if (ui->radioButton_obtain_dns_server_address_automatically->isChecked()) {
		ui->frame_dns_server->setEnabled(false);
	} else if (ui->radioButton_use_the_following_dns_server_address->isChecked()) {
		ui->frame_dns_server->setEnabled(true);
	}
}

void InterfaceCongifDialog::on_radioButton_obtain_an_ipaddress_automatically_clicked()
{

}

void InterfaceCongifDialog::on_radioButton_use_the_following_ip_address_clicked()
{

}

void InterfaceCongifDialog::on_radioButton_obtain_dns_server_address_automatically_clicked()
{

}

void InterfaceCongifDialog::on_radioButton_use_the_following_dns_server_address_clicked()
{

}

