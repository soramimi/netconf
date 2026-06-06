#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <memory>

class Win32NetworkConfig;

namespace Ui {
class MainWindow;
}

class QTableWidgetItem;

class MainWindow : public QMainWindow {
	Q_OBJECT
private:
	Ui::MainWindow *ui;

	struct Private;
	Private *m;

public:
	explicit MainWindow(QWidget *parent = nullptr);
	~MainWindow();

	void show();
	void reloadAdapters();
private slots:
	void on_tableWidget_itemDoubleClicked(QTableWidgetItem *item);
};

#endif // MAINWINDOW_H
