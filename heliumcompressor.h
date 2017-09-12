#ifndef HELIUMCOMPRESSOR_H
#define HELIUMCOMPRESSOR_H

#include <QString>
#include <QList>
#include <QSerialPortInfo>
#include <QDebug>

#include "../qtpressurecontrol/serialdevice.h"

class HeliumCompressor : public SerialDevice
{
public:
	HeliumCompressor();

	QByteArray sendRequest(const QByteArray &data);

private:
	bool sendingData;
};

#endif // HELIUMCOMPRESSOR_H
