#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QDate>
#include <QWebSocket>
#include <QTimer>
#include <QStateMachine>
#include <string>
#include <vector>

namespace Ui {
	class MainWindow;
}

class MainWindow : public QMainWindow
{
	Q_OBJECT

public:
	explicit MainWindow(QWidget* parent = nullptr);
	~MainWindow();

signals:
	void transferComplete();
	void goHome();

private:

	struct Data
	{
		int UUID;
		int RSSI;
		QDate timestamp;
	};
	const QUrl _url{ "ws://localhost:6969" };
	Ui::MainWindow* ui;
	QStateMachine* _stateMachine;
	QWebSocket* _socket;
	QList<QSerialPortInfo> _serialPorts;
	QSerialPort* _selectedPort;
	QTimer _timer;
	int _timeStamp;

	void populateSerialPortCombo();
	void refreshSerial();

	void disableUi();
	void enableUi();

	void enterServerConnect();
	void exitServerConnect();

	void enterCheckContact();
	void checkContact();
	void receiveContact(const QString& msg);
	void exitCheckContact();

	void enterAlertContact();
	void alertContact();
	void exitAlertContact();

	void enterTransfer();
	void serialHandshake();
	void retrieveData();
	void exitTransfer();

	void updateSelectedPort();


	void timeout(int type);

	void socketError(QAbstractSocket::SocketError err);
	void serialError(QSerialPort::SerialPortError err);
	void serialTimeout();
	void socketTimeout();
};

#endif // MAINWINDOW_H
