#include "InterfaceConfigDialog.h"
#include "ui_InterfaceConfigDialog.h"

InterfaceConfigDialog::InterfaceConfigDialog(Config const &config, QWidget *parent)
	: QDialog(parent)
	, ui(new Ui::InterfaceConfigDialog)
	, config_(config)
{
	ui->setupUi(this);

	a_.addButton(ui->radioButton_obtain_an_ip_address_automatically);
	a_.addButton(ui->radioButton_use_the_following_ip_address);

	b_.addButton(ui->radioButton_obtain_dns_server_address_automatically);
	b_.addButton(ui->radioButton_use_the_following_dns_server_address);

	exchange(false);
}

InterfaceConfigDialog::~InterfaceConfigDialog()
{
	delete ui;
}

InterfaceConfigDialog::Config const &InterfaceConfigDialog::config() const
{
	return config_;
}

void InterfaceConfigDialog::exchange(bool save)
{
	if (save) {
		config_.obtain_an_ip_address_automatically = ui->radioButton_obtain_an_ip_address_automatically->isChecked();
		config_.ip_address = ui->lineEdit_ip_address->text();
		config_.subnet_mask = ui->lineEdit_subnet_mask->text();
		config_.default_gateway = ui->lineEdit_default_gateway->text();

		config_.obtain_dns_server_address_automatically = ui->radioButton_obtain_dns_server_address_automatically->isChecked();
		config_.preferred_dns_server = ui->lineEdit_preferred_dns_server->text();
		config_.alternate_dns_server = ui->lineEdit_alternate_dns_server->text();
	} else {
		ui->radioButton_obtain_an_ip_address_automatically->setChecked(config_.obtain_an_ip_address_automatically);
		ui->radioButton_use_the_following_ip_address->setChecked(!config_.obtain_an_ip_address_automatically);
		ui->lineEdit_ip_address->setText(config_.ip_address);
		ui->lineEdit_subnet_mask->setText(config_.subnet_mask);
		ui->lineEdit_default_gateway->setText(config_.default_gateway);

		ui->radioButton_obtain_dns_server_address_automatically->setChecked(config_.obtain_dns_server_address_automatically);
		ui->radioButton_use_the_following_dns_server_address->setChecked(!config_.obtain_dns_server_address_automatically);
		ui->lineEdit_preferred_dns_server->setText(config_.preferred_dns_server);
		ui->lineEdit_alternate_dns_server->setText(config_.alternate_dns_server);

		reflectUI();
	}
}

void InterfaceConfigDialog::done(int status)
{
	if (status == QDialog::Accepted) {
		exchange(true);
	}
	QDialog::done(status);
}

void InterfaceConfigDialog::reflectUI()
{
	if (ui->radioButton_obtain_an_ip_address_automatically->isChecked()) {
		ui->radioButton_obtain_dns_server_address_automatically->setEnabled(true);
		ui->radioButton_obtain_dns_server_address_automatically->setCheckable(true);
	} else {
		ui->radioButton_obtain_dns_server_address_automatically->setEnabled(false);
		ui->radioButton_obtain_dns_server_address_automatically->setCheckable(false);
		ui->radioButton_use_the_following_dns_server_address->setChecked(true);
	}

	if (ui->radioButton_obtain_an_ip_address_automatically->isChecked()) {
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

void InterfaceConfigDialog::on_radioButton_obtain_an_ip_address_automatically_clicked()
{
	reflectUI();
}

void InterfaceConfigDialog::on_radioButton_use_the_following_ip_address_clicked()
{
	reflectUI();
}

void InterfaceConfigDialog::on_radioButton_obtain_dns_server_address_automatically_clicked()
{
	reflectUI();
}

void InterfaceConfigDialog::on_radioButton_use_the_following_dns_server_address_clicked()
{
	reflectUI();
}
