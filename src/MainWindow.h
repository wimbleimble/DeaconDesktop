#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QDate>
#include <QWebSocket>
#include <QTimer>
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

private:
	enum class State
	{
		Idle,
		Connect,
		SerialSync,
		WebSync,
		Contact,
		ContactTrue,
		ContactFalse,
		MAX_STATES
	};

	static constexpr char stateLUT[(int)State::MAX_STATES][64]{
		"Select Device and Sync...",
		"Connecting to sync server...",
		"Syncing with device...",
		"Uploading data...",
		"Analysing contact...",
		"You got the virus :(.",
		"You're safe.... FOR NOW >:)"
	};

	struct Data
	{
		int UUID;
		int RSSI;
		QDate timestamp;

	};

	Ui::MainWindow* ui;
	QWebSocket* _socket;
	QList<QSerialPortInfo> _serialPorts;
	QSerialPort* _selectedPort;
	State _state;
	QTimer _timer;
	int _timeStamp;

	void populateSerialPortCombo();
	void refreshSerial();
	void sync();
	void setState(State state);
	void updateSelectedPort();

	//retrieves data from arduino and returns
	void serialHandshake();
	void retrieveData();

	//sends data to server
	void uploadData(const std::vector<Data>& data);

	//asks server to cross reference data and determine if there has been
	//contact. returns true if yes
	void checkContact();
	void disableUi();
	void enableUi();

	void onConnect();
	void contactCallback(const QString& msg);

	//function pointers, because i fucking hate myself
	//there has to be an easier way of doing this????
	void connectSocketRead(void (MainWindow::* func)(const QString&));
	void disconnectSocketRead(void (MainWindow::* func)(const QString&));
	void connectTimeout(void (MainWindow::* func)());
	void disconnectTimeout(void (MainWindow::* func)());

	void socketError(QAbstractSocket::SocketError err);
	void serialError(QSerialPort::SerialPortError err);
	void serialTimeout();
	void socketTimeout();
	void echo(const QString& msg);


};

#endif // MAINWINDOW_H
