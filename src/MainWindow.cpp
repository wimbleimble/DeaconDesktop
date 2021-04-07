#include "MainWindow.h"
#include "ui_MainWindow.h"
#include <iostream>
#include <QThread>

MainWindow::MainWindow(QWidget* parent) :
	QMainWindow{ parent },
	ui{ new Ui::MainWindow },
	_socket{ new QWebSocket() },
	_serialPorts{ QSerialPortInfo::availablePorts() },
	_selectedPort{ new QSerialPort() },
	_state{ State::Idle }
{
	ui->setupUi(this);
	if (_serialPorts.size() > 0)
		_selectedPort->setPort(_serialPorts[ui->selectSerialCombo->currentIndex()] );

	populateSerialPortCombo();
	
	connect(ui->syncButton, &QPushButton::clicked,
			this, &MainWindow::sync);
	connect(ui->refreshSerialButton, &QPushButton::clicked,
			this, &MainWindow::refreshSerial);
	connect(_socket, QOverload<QAbstractSocket::SocketError>::of(&QWebSocket::error),
		this, &MainWindow::socketError);
	connect(_selectedPort, &QSerialPort::errorOccurred,
			this, &MainWindow::serialError);
}

MainWindow::~MainWindow()
{
	delete _socket;
	delete _selectedPort;
	delete ui;
}

void MainWindow::populateSerialPortCombo()
{
	for (QSerialPortInfo port : _serialPorts)
		ui->selectSerialCombo->addItem(port.portName()
			+ QString(" - ") + port.description());

}

void MainWindow::refreshSerial()
{
	ui->selectSerialCombo->clear();
	_serialPorts = QSerialPortInfo::availablePorts();
	populateSerialPortCombo();
}

void MainWindow::setState(State state)
{
	_state = state;
	ui->statusLabel->setText(QString(stateLUT[(int)state]));

	if (state == State::Idle)
		enableUi();
}

void MainWindow::disableUi()
{
	ui->syncButton->setEnabled(false);
	ui->refreshSerialButton->setEnabled(false);
	ui->selectSerialCombo->setEnabled(false);
}

void MainWindow::enableUi()
{
	ui->syncButton->setEnabled(true);
	ui->refreshSerialButton->setEnabled(true);
	ui->selectSerialCombo->setEnabled(true);
}

void MainWindow::sync()
{
	connect(_socket, &QWebSocket::connected,
		this, &MainWindow::onConnect);
	disableUi();

	_socket->open(QUrl("ws://localhost:6969"));
	
	setState(State::Connect);
	std::cout << "connecting to server...\n";
}

void MainWindow::onConnect()
{
	std::cout << "Successfully connected.\n";
	connect(_selectedPort, &QSerialPort::readyRead,
		this, &MainWindow::retrieveData);

	connectTimeout(&MainWindow::serialTimeout);
	_timer.start(1000);
}

void MainWindow::retrieveData()
{
	_timer.stop();
	setState(State::SerialSync);
	std::cout << "Retrieving data from arduino...\n";

	disconnect(_selectedPort, &QSerialPort::readyRead,
		this, &MainWindow::retrieveData);
	uploadData(std::vector<Data>(3, { 1234, 1234, QDate() }));
}

void MainWindow::uploadData(const std::vector<Data>& data)
{
	setState(State::WebSync);
	std::cout << "Uploading data to server...\n";
	for (Data a : data)
		_socket->sendTextMessage(QString(a.UUID));
	_socket->sendTextMessage("Done...");

	checkContact();
}

void MainWindow::checkContact()
{
	setState(State::Contact);
	std::cout << "Checking contact...";

	connectSocketRead(&MainWindow::contactCallback);
	_socket->sendTextMessage("HEY HEY YOU YOU AM I DEAD????\n");
}

void MainWindow::contactCallback(const QString& msg)
{
	if (msg == "yes run\n") setState(State::ContactTrue);
	else if (msg == "not yet\n") setState(State::ContactFalse);
	else
	{
		setState(State::Idle);
		std::cout << "Error commuicating with server\n";
	}
	disconnectSocketRead(&MainWindow::contactCallback);
}

void MainWindow::echo(const QString& msg)
{
	std::cout << msg.toStdString() << '\n';
	
}
void MainWindow::socketError(QAbstractSocket::SocketError err)
{
	std::cout << "socket error: " <<_socket->errorString().toStdString() << '\n';
	setState(State::Idle);
}

void MainWindow::serialError(QSerialPort::SerialPortError err)
{
	std::cout << "Serial error: " << _selectedPort->errorString().toStdString() << '\n';
	setState(State::Idle);
}

void MainWindow::serialTimeout()
{
	std::cout << "Unable to communicate with beacon, serial timed out.\n";
	setState(State::Idle);
}

void MainWindow::socketTimeout()
{
	std::cout << "Unable to communicate with server, timed out.\n";
	_socket->close();
	setState(State::Idle);
}

void MainWindow::connectSocketRead(void (MainWindow::*func)(const QString&))
{
	connect(_socket, &QWebSocket::textMessageReceived, this, func);
}

void MainWindow::disconnectSocketRead(void (MainWindow::*func)(const QString&))
{
	disconnect(_socket, &QWebSocket::textMessageReceived, this, func);
}

void MainWindow::connectTimeout(void (MainWindow::*func)())
{
	connect(&_timer, &QTimer::timeout, this, func);
}

void MainWindow::disconnectTimeout(void (MainWindow::*func)())
{
	disconnect(&_timer, &QTimer::timeout, this, func);
}

constexpr char MainWindow::stateLUT[(int)State::MAX_STATES][64];