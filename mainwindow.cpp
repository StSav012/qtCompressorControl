#include "mainwindow.h"
#include "ui_mainwindow.h"

int bestMatchItem(QList<QSerialPortInfo> &ports, const QString &text) {
	QString fullPortName;
	int bestMatchIndex = 0;										// we try to recover the device used
	int bestMatchCount = 0;										// even if the port has changed
	for (int i = 0; i < ports.length(); ++i) {					// since the last time.
		fullPortName = ports.at(i).portName();
		if (!ports.at(i).description().isEmpty()) {
			fullPortName += QString(" — ") + ports.at(i).description();
		}
		if (text == fullPortName) {								// we compare the device list items
			bestMatchIndex = i;									// to the item selected last time.
			break;												// the device description helps much.
		}
		else {
			int mc = 0;											// matching letters counter
			for (int j = 0; j < fullPortName.length()
						 && j < text.length(); ++j) {
				if (fullPortName.at(j) == text.at(j)) {
					++mc;
				}
			}
			if (mc > bestMatchCount) {
				bestMatchCount = mc;
				bestMatchIndex = i;
			}
		}
	}
	return bestMatchIndex;
}

int bestMatchItem(const QComboBox *box, const QSerialPortInfo &port) {
	QString fullPortName;
	fullPortName = port.portName();
	if (!port.description().isEmpty()) {
		fullPortName += QString(" — ") + port.description();
	}
	int bestMatchIndex = 0;										// we try to recover the device used
	int bestMatchCount = 0;										// even if the port has changed
	for (int i = 0; i < box->count(); ++i) {					// since the last time.
		if (fullPortName == box->itemText(i)) {					// we compare the device list items
			bestMatchIndex = i;									// to the item selected last time.
			break;												// the device description helps much.
		}
		else {
			int mc = 0;											// matching letters counter
			for (int j = 0; j < box->itemText(i).length()
						 && j < fullPortName.length(); ++j) {
				if (box->itemText(i).at(j) == fullPortName.at(j)) {
					++mc;
				}
			}
			if (mc > bestMatchCount) {
				bestMatchCount = mc;
				bestMatchIndex = i;
			}
		}
	}
	return bestMatchIndex;
}

unsigned int crc16(const QByteArray &data)
{
	unsigned int crc = 0xffff;
	foreach (char c, data) {
		crc ^= (unsigned int)c;
		for (int i = 0; i < 8; ++i) {
			if (crc & 1) {
				crc >>= 1;
				crc ^= 0xA001;
			}
			else {
				crc >>= 1;
			}
		}
	}
	return crc;
}

QString crc16(const QString &string) {
	unsigned int crc = crc16(string.toLocal8Bit());
	char crcString[5] = {0};
	for (unsigned char j = 0; j < 4; ++j) {
		crcString[3-j] = ((crc & 0x0f) < 0xa)?((crc & 0x0f) + '0'):((crc & 0x0f) - 0xa + 'A');
		crc >>= 4;
	}
	return QString(crcString);
}

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), ui(new Ui::MainWindow) {
	isLoaded = false;
	ui->setupUi(this);
	// Lock File setup
	lock = new QLockFile(".~lock");
	lock->setStaleLockTime(0);
	while(!lock->tryLock(1)) {
		QMessageBox mb(QMessageBox::Warning, ui->centralWidget->windowTitle(),
					   tr("Another instance of the program appears to be running. You may proceed but it might cause problems with both instances"),
					   QMessageBox::Abort | QMessageBox::Ignore,
					   this);
		if (mb.exec() == QMessageBox::Abort) {
			exit(0);
		}
		else {
			lock->removeStaleLockFile();
		}
	}
	// Detect USB
	detectUSB();
	// Restore settings from the previous run
	loadSettings();
	// Prepare Status Bar
	operatedHours = new QLabel(this);
	ui->statusBar->addPermanentWidget(operatedHours, 0);

	isLoaded = true;

	// Timer setup
	timer = new QTimer(this);
	connect(timer, SIGNAL(timeout()), this, SLOT(on_timeout()));
	on_timeout();
	timer->start(1000);
}

MainWindow::~MainWindow() {
	lock->unlock();
	saveSettings();
	delete ui;
}

void MainWindow::detectUSB() {
	ports = QSerialPortInfo::availablePorts();
	foreach (const QSerialPortInfo &info, ports) {
		if (info.description().isEmpty()) {
			ui->list_devices->addItem(info.portName());
		}
		else {
			ui->list_devices->addItem(info.portName() + QString(" — ") + info.description());
		}
	}
}

void MainWindow::loadSettings() {
	QSettings s(QSettings::IniFormat, QSettings::UserScope, "SavSoft", "Helium Compressor Control");
	s.beginGroup("settings");
	QString lastDeviceName;
	int bestMatchIndex;
	lastDeviceName = s.value("port name", QString("USB-SERIAL")).toString();
	bestMatchIndex = bestMatchItem(ports, lastDeviceName);
	device.setPort(ports.at(bestMatchIndex));
	bestMatchIndex = bestMatchItem(ui->list_devices, ports.at(bestMatchIndex));
	ui->list_devices->setCurrentIndex(bestMatchIndex);
	qDebug() << "compressor is now connected to" << device.portName();

	QDesktopWidget* desktop = QApplication::desktop();
	this->move(0.5*(desktop->width()-this->size().width()),
			   0.5*(desktop->height()-this->size().height()));		// Fallback: Center the window
	QVariant window;
	window = s.value("window geometry");
	if (!window.isNull()) {
		restoreGeometry(window.toByteArray());
	}
	window = s.value("window state");
	if (!window.isNull()) {
		restoreState(window.toByteArray());
	}
	ui->button_topmost->setChecked(s.value("topmost").toBool());
	s.endGroup();
}

void MainWindow::saveSettings() {
	QSettings s(QSettings::IniFormat, QSettings::UserScope, "SavSoft", "Helium Compressor Control");
	s.beginGroup("settings");
	s.setValue("port name", ui->list_devices->currentText());
	s.setValue("window geometry", saveGeometry());
	s.setValue("window state", saveState());
	s.setValue("topmost", ui->button_topmost->isChecked());
	s.endGroup();
}

void MainWindow::getTemperatures() {
	QString cmd = "$TEA";
	QStringList response;
	do {
		response = QString(device
						   .sendRequest((cmd + crc16(cmd) + QChar('\r')).toLocal8Bit()))
				   .split(QChar(','));
		if (response.length() != 6) {
			qDebug() << "invalid response:" << response.join(',');
			return;
		}
		if (response.at(0) != cmd) {
			qDebug() << "invalid response:" << response.join(',');
			return;
		}
		if (crc16(response.join(',').left(21)) + QChar('\r') != response.last()) {
			qDebug() << "invalid response:" << response.join(',');
			return;
		}
	} while (response.at(0) == QString("$???"));
	// finally
	ui->lcd_temperature_1->display(response.at(1));
	ui->lcd_temperature_2->display(response.at(2));
	ui->lcd_temperature_3->display(response.at(3));
}

void MainWindow::getPressures() {
	QString cmd = "$PRA";
	QStringList response;
	do {
		response = QString(device
						   .sendRequest((cmd + crc16(cmd) + QChar('\r')).toLocal8Bit()))
				   .split(QChar(','));
		if (response.length() != 4) {
			qDebug() << "invalid response:" << response.join(',');
			return;
		}
		if (response.at(0) != cmd) {
			qDebug() << "invalid response:" << response.join(',');
			return;
		}
		if (crc16(response.join(',').left(13)) + QChar('\r') != response.last()) {
			qDebug() << "invalid response:" << response.join(',');
			return;
		}
	} while (response.at(0) == QString("$???"));
	// finally
	ui->lcd_pressure_1->display(response.at(1));
	ui->lcd_pressure_1->setStatusTip(tr("%1 Pa").arg(ui->lcd_pressure_1->value() * 6894.757293168));
}

void MainWindow::getStatus() {
	QString cmd = "$STA";
	QStringList response;
	do {
		response = QString(device
						   .sendRequest((cmd + crc16(cmd) + QChar('\r')).toLocal8Bit()))
				   .split(QChar(','));
		if (response.length() != 3) {
			qDebug() << "invalid response:" << response.join(',');
			return;
		}
		if (response.at(0) != cmd) {
			qDebug() << "invalid response:" << response.join(',');
			return;
		}
		if (crc16(response.join(',').left(10)) + QChar('\r') != response.last()) {
			qDebug() << "invalid response:" << response.join(',');
			return;
		}
	} while (response.at(0) == QString("$???"));
	// finally
	QBitArray state(16);
	unsigned char i = 0, j;
	foreach (char c, response.at(1).toLocal8Bit()) {
		if (c < '0' || (c > '9' && c < 'A') || c > 'F') {
			qDebug() << "invalid response:" << response.join(',');
			return;
		}
		if (c >= 'A') {
			c -= 'A';
		}
		if (c >= '0') {
			c -= '0';
		}
		for (j = 0; c && j < 4; ++j) {
			state.setBit(12 - 4 * i + j, c & (1 << j));
		}
		++i;
	}
	static unsigned char prevStateNumber = 42;
	unsigned char stateNumber = state.at(9) +
								(state.at(10) << 1) +
								(state.at(11) << 2);
	if (stateNumber != prevStateNumber) {								// state changed
		ui->statusBar->showMessage((QStringList()
									<< tr("Local Off")
									<< tr("Local On")
									<< tr("Remote Off")
									<< tr("Remote On")
									<< tr("Cold Head Run")
									<< tr("Cold Head Pause")
									<< tr("Fault Off")
									<< tr("Oil Fault Off")).at(stateNumber));
		ui->label_state_oilFaultOff->setEnabled(stateNumber == 7);
		ui->label_state_faultOff->setEnabled(stateNumber == 6);
		ui->label_state_coldHeadPause->setEnabled(stateNumber == 5);
		ui->label_state_coldHeadRun->setEnabled(stateNumber == 4);
		ui->label_state_localOn->setEnabled(stateNumber == 1);
		prevStateNumber = stateNumber;
	}
	ui->label_state_solenoidOn->setEnabled(state.testBit(8));
	ui->label_state_pressureAlarm->setEnabled(state.testBit(7));
	ui->label_state_oilLevelAlarm->setEnabled(state.testBit(6));
	ui->label_state_waterFlowAlarm->setEnabled(state.testBit(5));
	ui->label_state_waterTemperatureAlarm->setEnabled(state.testBit(4));
	ui->label_state_heliumTemperatureAlarm->setEnabled(state.testBit(3));
	ui->label_state_powerAlarm->setEnabled(state.testBit(2));
	ui->label_state_motorTemperatureAlarm->setEnabled(state.testBit(1));
	ui->label_state_systemOn->setEnabled(state.testBit(0));
	// read only indicator
	ui->button_on->setDisabled(state.testBit(15));
	ui->button_reset->setDisabled(state.testBit(15));
	ui->button_startColdHead->setDisabled(state.testBit(15));
	ui->button_stopColdHead->setDisabled(state.testBit(15));
}

void MainWindow::getID() {
	QString cmd = "$ID1";
	QStringList response;
	do {
		response = QString(device
						   .sendRequest((cmd + crc16(cmd) + QChar('\r')).toLocal8Bit()))
				   .split(QChar(','));
		if (response.length() != 5) {								// WTF??! In the manual, it's 4.
			qDebug() << "invalid response:" << response.join(',');
			return;
		}
		if (response.at(0) != cmd) {
			qDebug() << "invalid response:" << response.join(',');
			return;
		}
		if (response.at(1).length() != 3) {
			qDebug() << "invalid response:" << response.join(',');
			return;
		}
		if (response.at(2).length() != 8) {
			qDebug() << "invalid response:" << response.join(',');
			return;
		}
		if (crc16(response.join(',').left(22)) + QChar('\r') != response.last()) {
			qDebug() << "invalid response:" << response.join(',');
			return;
		}
	} while (response.at(0) == QString("$???"));
	// finally
	operatedHours->setText(tr("%1 h").arg(response.at(2)));
}

void MainWindow::on_list_devices_currentIndexChanged(int index)
{
	if (isLoaded) {
		int bestMatchIndex;
		bestMatchIndex = bestMatchItem(ports, ui->list_devices->itemText(index));
		device.setPort(ports.at(bestMatchIndex));
		bestMatchIndex = bestMatchItem(ui->list_devices, ports.at(bestMatchIndex));
		ui->list_devices->setCurrentIndex(bestMatchIndex);
		qDebug() << "compressor is connected to" << ports.at(bestMatchIndex).portName();
	}
}

void MainWindow::on_button_on_clicked(bool checked)
{
	if (!checked) {
		QString cmd = "$ON1";
		QStringList response;
		do {
			response = QString(device
							   .sendRequest((cmd + crc16(cmd) + QChar('\r')).toLocal8Bit()))
					   .split(QChar(','));
			if (response.length() != 2) {
				qDebug() << "invalid response:" << response.join(',');
				return;
			}
			if (response.at(0) != cmd) {
				qDebug() << "invalid response:" << response.join(',');
				return;
			}
			if (crc16(response.join(',').left(5)) + QChar('\r') != response.last()) {
				qDebug() << "invalid response:" << response.join(',');
				return;
			}
		} while (response.at(0) == QString("$???"));
		ui->button_on->setText(tr("Turn Off"));
	}
	else {
		QString cmd = "$OFF";
		QStringList response;
		do {
			response = QString(device
							   .sendRequest((cmd + crc16(cmd) + QChar('\r')).toLocal8Bit()))
					   .split(QChar(','));
			if (response.length() != 2) {
				qDebug() << "invalid response:" << response.join(',');
				return;
			}
			if (response.at(0) != cmd) {
				qDebug() << "invalid response:" << response.join(',');
				return;
			}
			if (crc16(response.join(',').left(5)) + QChar('\r') != response.last()) {
				qDebug() << "invalid response:" << response.join(',');
				return;
			}
		} while (response.at(0) == QString("$???"));
		ui->button_on->setText(tr("Turn On"));
	}
}

void MainWindow::on_timeout()
{
	getTemperatures();
	getPressures();
	getStatus();
	getID();
}

void MainWindow::on_button_reset_clicked()
{
	if (QMessageBox::question(this, ui->button_reset->text(), tr("Are you sure?")) == QMessageBox::Yes) {
		QString cmd = "$RS1";
		QStringList response;
		do {
			response = QString(device
							   .sendRequest((cmd + crc16(cmd) + QChar('\r')).toLocal8Bit()))
					   .split(QChar(','));
			if (response.length() != 2) {
				qDebug() << "invalid response:" << response.join(',');
				return;
			}
			if (response.at(0) != cmd) {
				qDebug() << "invalid response:" << response.join(',');
				return;
			}
			if (crc16(response.join(',').left(5)) + QChar('\r') != response.last()) {
				qDebug() << "invalid response:" << response.join(',');
				return;
			}
		} while (response.at(0) == QString("$???"));
	}
}

void MainWindow::on_button_startColdHead_clicked()
{
	if (QMessageBox::question(this, ui->button_startColdHead->text(), tr("Are you sure?")) == QMessageBox::Yes) {
		QString cmd;
		if (ui->button_on->isChecked()) {
			cmd = QString("$POF");
		}
		else {
			cmd = QString("$CHR");
		}
		QStringList response;
		do {
			response = QString(device
							   .sendRequest((cmd + crc16(cmd) + QChar('\r')).toLocal8Bit()))
					   .split(QChar(','));
			if (response.length() != 2) {
				qDebug() << "invalid response:" << response.join(',');
				return;
			}
			if (response.at(0) != cmd) {
				qDebug() << "invalid response:" << response.join(',');
				return;
			}
			if (crc16(response.join(',').left(5)) + QChar('\r') != response.last()) {
				qDebug() << "invalid response:" << response.join(',');
				return;
			}
		} while (response.at(0) == QString("$???"));
	}
}

void MainWindow::on_button_stopColdHead_clicked()
{
	if (QMessageBox::question(this, ui->button_reset->text(), tr("Are you sure?")) == QMessageBox::Yes) {
		QString cmd = "$CHP";
		QStringList response;
		do {
			response = QString(device
							   .sendRequest((cmd + crc16(cmd) + QChar('\r')).toLocal8Bit()))
					   .split(QChar(','));
			if (response.length() != 2) {
				qDebug() << "invalid response:" << response.join(',');
				return;
			}
			if (response.at(0) != cmd) {
				qDebug() << "invalid response:" << response.join(',');
				return;
			}
			if (crc16(response.join(',').left(5)) + QChar('\r') != response.last()) {
				qDebug() << "invalid response:" << response.join(',');
				return;
			}
		} while (response.at(0) == QString("$???"));
	}
}

void MainWindow::on_button_topmost_toggled(bool checked)
{
#if QT_VERSION < QT_VERSION_CHECK(5, 9, 0)
	if (checked != windowFlags().testFlag(Qt::WindowStaysOnTopHint)) {
		setWindowFlags(windowFlags() ^ Qt::WindowStaysOnTopHint);
		if (isHidden()) {
			show();
		}
	}
#else
	setWindowFlag(Qt::WindowStaysOnTopHint, checked);
	if (isHidden()) {
		show();
	}
#endif
}
