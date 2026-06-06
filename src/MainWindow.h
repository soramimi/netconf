#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <memory>

class Win32NetworkConfig;

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow {
	Q_OBJECT
private:
	Ui::MainWindow *ui;
	std::shared_ptr<Win32NetworkConfig> nc_;

public:
	explicit MainWindow(QWidget *parent = nullptr);
	~MainWindow();

	void show();
};

#endif // MAINWINDOW_H
