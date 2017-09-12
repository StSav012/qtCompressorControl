#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QtGlobal>
#include <QMessageBox>
#include <QMainWindow>
#include <QDesktopWidget>
#include <QResource>
#include <QDebug>
#include <QSettings>
#include <QLockFile>
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QBitArray>

#include "heliumcompressor.h"

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
	Q_OBJECT

public:
	explicit MainWindow(QWidget *parent = 0);
	~MainWindow();

private slots:
	void on_list_devices_currentIndexChanged(int index);

	void on_button_on_clicked(bool checked);

	void on_button_reset_clicked();

	void on_button_startColdHead_clicked();

	void on_button_stopColdHead_clicked();

	void on_button_topmost_toggled(bool checked);

private:
	Ui::MainWindow *ui;
	HeliumCompressor device;
	QLabel *operatedHours;

	void detectUSB();
	void loadSettings();
	void saveSettings();

	void getTemperatures();
	void getPressures();
	void getStatus();
	void getID();

public:
	QList<QSerialPortInfo> ports;
	QTimer *timer;
	QLockFile *lock;
	bool isLoaded;

public slots:
	void on_timeout(void);

};

using namespace std;

#endif // MAINWINDOW_H
