#include "heliumcompressor.h"

HeliumCompressor::HeliumCompressor()
{
	device.setBaudRate(QSerialPort::Baud9600);
	device.setParity(QSerialPort::NoParity);
	device.setDataBits(QSerialPort::Data8);
	device.setStopBits(QSerialPort::OneStop);
	sendingData = false;
}

QByteArray HeliumCompressor::sendRequest(const QByteArray &data) {
	if (sendingData) {
		qDebug() << "the device is busy";
		return QByteArray();
	}
	return sendData(data);
}
