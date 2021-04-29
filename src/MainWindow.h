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

	static constexpr int _serverTimeout{ 5000 };
	static constexpr int _serialTimeout{ 20000 };

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
	QTimer _serverTimer;
	QTimer _serialTimer;
	int _timeStamp;

	void populateSerialPortCombo();
	void refreshSerial();

	void disableUi();
	void enableUi();

	void enterServerConnect();
	void exitServerConnect();

	void enterCheckContact();
	void checkContact();
	void requestUUID();			// im so sorry
	void receiveContact(const QString& msg);
	void exitCheckContact();

	void enterAlertContact();
	void requestUUIDAgain();	// i really am so so sorry
	void alertContact();
	void exitAlertContact();

	void enterTransfer();
	void retrieveTimestamp();
	void retrieveUUID();		// im going to hell for this
	void retrieveData();
	void exitTransfer();

	void updateSelectedPort();


	void timeout(int type);

	void socketError(QAbstractSocket::SocketError err);
	void serialError(QSerialPort::SerialPortError err);
};

#endif // MAINWINDOW_H
