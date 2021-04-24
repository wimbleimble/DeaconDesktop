#include "MainWindow.h"
#include "ui_MainWindow.h"
#include <iostream>
#include <QThread>
#include <QMessageBox>

MainWindow::MainWindow(QWidget* parent) :
	QMainWindow{ parent },
	ui{ new Ui::MainWindow },
	_stateMachine{ new QStateMachine() },
	_socket{ new QWebSocket() },
	_serialPorts{ QSerialPortInfo::availablePorts() },
	_selectedPort{ new QSerialPort() },
	_serverTimer{}, _serialTimer{}
{
	ui->setupUi(this);
	populateSerialPortCombo();
	updateSelectedPort();

	QState* idle{ new QState() };
	QState* serverConnect{ new QState() };
	QState* transfer{ new QState() };
	QState* checkContact{ new QState() };
	QState* alertContact{ new QState() };

	QState* serverTimeout{ new QState() };
	QState* serialTimeout{ new QState() };

	idle->addTransition(ui->syncButton, &QPushButton::clicked, serverConnect);
	serverConnect->addTransition(_socket, &QWebSocket::connected, transfer);
	transfer->addTransition(this, &MainWindow::goHome, idle);

	serverConnect->addTransition(&_serverTimer, &QTimer::timeout, serverTimeout);
	checkContact->addTransition(&_serverTimer, &QTimer::timeout, serverTimeout);
	alertContact->addTransition(&_serverTimer, &QTimer::timeout, serverTimeout);
	transfer->addTransition(&_serialTimer, &QTimer::timeout, serialTimeout);
	checkContact->addTransition(&_serialTimer, &QTimer::timeout, serialTimeout);
	alertContact->addTransition(&_serialTimer, &QTimer::timeout, serialTimeout);

	serverTimeout->addTransition(this, &MainWindow::goHome, idle);
	serialTimeout->addTransition(this, &MainWindow::goHome, idle);

	idle->addTransition(ui->checkContactButton, &QPushButton::clicked,
		checkContact);
	idle->addTransition(ui->notifyContactButton, &QPushButton::clicked,
		alertContact);
	checkContact->addTransition(this, &MainWindow::goHome, idle);
	alertContact->addTransition(this, &MainWindow::goHome, idle);

	idle->assignProperty(ui->statusLabel,
		"text", "Select Device and Sync...");
	serverConnect->assignProperty(ui->statusLabel,
		"text", "Connecting to sync server...");
	transfer->assignProperty(ui->statusLabel,
		"text", "Transferring contact data...");
	checkContact->assignProperty(ui->statusLabel,
		"text", "Checking database for contact...");
	alertContact->assignProperty(ui->statusLabel,
		"text", "Alerting database to contact...");

	connect(idle, &QState::entered, this, &MainWindow::enableUi);
	connect(idle, &QState::exited, this, &MainWindow::disableUi);

	connect(serverConnect, &QState::entered, this, &MainWindow::enterServerConnect);
	connect(serverConnect, &QState::exited, this, &MainWindow::exitServerConnect);

	connect(transfer, &QState::entered, this, &MainWindow::enterTransfer);
	connect(transfer, &QState::exited, this, &MainWindow::exitTransfer);

	connect(checkContact, &QState::entered, this, &MainWindow::enterCheckContact);
	connect(checkContact, &QState::exited, this, &MainWindow::exitCheckContact);

	connect(alertContact, &QState::entered, this, &MainWindow::enterAlertContact);
	connect(alertContact, &QState::exited, this, &MainWindow::exitAlertContact);

	connect(serverTimeout, &QState::entered,
		this, [this]() {timeout(0);});
	connect(serialTimeout, &QState::entered,
		this, [this]() {timeout(1);});

	_stateMachine->addState(idle);
	_stateMachine->addState(serverConnect);
	_stateMachine->addState(transfer);
	_stateMachine->addState(checkContact);
	_stateMachine->addState(alertContact);
	_stateMachine->addState(serialTimeout);
	_stateMachine->addState(serverTimeout);
	_stateMachine->setInitialState(idle);
	_stateMachine->start();

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
	delete _stateMachine;
	delete ui;
}

void MainWindow::populateSerialPortCombo()
{
	for (QSerialPortInfo port : _serialPorts)
		ui->selectSerialCombo->addItem(port.portName()
			+ QString(" - ") + port.description());
}

void MainWindow::updateSelectedPort()
{
	if (_serialPorts.size() > 0)
		_selectedPort->setPort(_serialPorts[ui->selectSerialCombo->currentIndex()]);
}

void MainWindow::refreshSerial()
{
	ui->selectSerialCombo->clear();
	_serialPorts = QSerialPortInfo::availablePorts();
	populateSerialPortCombo();
}

void MainWindow::disableUi()
{
	ui->syncButton->setEnabled(false);
	ui->refreshSerialButton->setEnabled(false);
	ui->selectSerialCombo->setEnabled(false);
	ui->checkContactButton->setEnabled(false);
	ui->notifyContactButton->setEnabled(false);
}

void MainWindow::enableUi()
{
	ui->syncButton->setEnabled(true);
	ui->refreshSerialButton->setEnabled(true);
	ui->checkContactButton->setEnabled(true);
	ui->notifyContactButton->setEnabled(true);
	ui->selectSerialCombo->setEnabled(true);
	ui->progressBar->setValue(0);
}

void MainWindow::enterServerConnect()
{
	_socket->open(_url);
	_serverTimer.start(_serverTimeout);
	ui->progressBar->setValue(1);
}

void MainWindow::exitServerConnect()
{
	_serverTimer.stop();
}

void MainWindow::enterTransfer()
{
	updateSelectedPort();
	_selectedPort->open(QIODevice::ReadWrite);
	_selectedPort->write("sync\n");

	connect(_selectedPort, &QSerialPort::readyRead,
		this, &MainWindow::retrieveTimestamp);

	_serialTimer.start(_serialTimeout);
}

void MainWindow::retrieveTimestamp()
{
	if (_selectedPort->canReadLine())
	{
		ui->progressBar->setValue(2);
		_serialTimer.stop();
		QString msg{ _selectedPort->readLine() };
		if (msg.contains("ts:"))
		{
			_timeStamp = msg.mid(3).toInt();
			disconnect(_selectedPort, &QSerialPort::readyRead,
				this, &MainWindow::retrieveTimestamp);
			connect(_selectedPort, &QSerialPort::readyRead,
				this, &MainWindow::retrieveUUID);
		}
	}
}

void MainWindow::retrieveUUID()
{
	if (_selectedPort->canReadLine())
	{
		ui->progressBar->setValue(3);
		QString msg{ _selectedPort->readLine() };
		if (msg.contains("uuid:"))
		{
			_socket->sendTextMessage(msg);
			disconnect(_selectedPort, &QSerialPort::readyRead,
				this, &MainWindow::retrieveUUID);
			connect(_selectedPort, &QSerialPort::readyRead,
				this, &MainWindow::retrieveData);
		}
	}
}

void MainWindow::retrieveData()
{
	if (_selectedPort->canReadLine());
	{
		ui->progressBar->setValue(4);
		QString msg{ _selectedPort->readLine() };
		if (msg.contains("done"))
		{
			ui->progressBar->setValue(5);
			_socket->sendTextMessage("done\n");
			emit goHome();
		}
		else
			_socket->sendTextMessage(msg);
	}
}

void MainWindow::exitTransfer()
{
	disconnect(_selectedPort, &QSerialPort::readyRead,
		this, &MainWindow::retrieveData);
	_socket->close();
	_selectedPort->close();
	_serialTimer.stop();
}

void MainWindow::enterCheckContact()
{
	_socket->open(_url);
	_serverTimer.start(_serverTimeout);
	connect(_socket, &QWebSocket::connected,
		this, &MainWindow::requestUUID);
}

void MainWindow::requestUUID()
{
	_serverTimer.stop();
	disconnect(_socket, &QWebSocket::connected,
		this, &MainWindow::requestUUID);
	updateSelectedPort();
	_selectedPort->open(QIODevice::ReadWrite);
	_selectedPort->write("uuid\n");
	_serialTimer.start(_serialTimeout);
	connect(_selectedPort, &QSerialPort::readyRead,
		this, &MainWindow::checkContact);
}

void MainWindow::checkContact()
{
	if (_selectedPort->canReadLine())
	{
		QString msg{ _selectedPort->readLine() };
		if (msg.contains("uuid:"))
		{
			_serialTimer.stop();
			disconnect(_selectedPort, &QSerialPort::readyRead,
				this, &MainWindow::checkContact);
			_socket->sendTextMessage("chk:" + msg.mid(5));
			connect(_socket, &QWebSocket::textMessageReceived,
				this, &MainWindow::receiveContact);
		}
	}
}

void MainWindow::receiveContact(const QString& msg)
{
	QMessageBox msgBox;
	if (msg == "y")
	{
		msgBox.setText("You have had contact with coronavirus.");
		msgBox.setInformativeText("Apply with the NHS for a test.");
	}
	else if (msg == "n")
	{
		msgBox.setText("You have not had contact.");
		msgBox.setInformativeText(
			"No further action needs to be taken by you at the current time."
		);
	}
	else
	{
		msgBox.setText("Error communicating with server.");
		msgBox.setInformativeText("Check internet connection and try again.");
	}

	msgBox.exec();
	emit goHome();
}


void MainWindow::exitCheckContact()
{
	_socket->close();
	_selectedPort->close();
	_serialTimer.stop();
	_serverTimer.stop();
	disconnect(_socket, &QWebSocket::connected,
		this, &MainWindow::requestUUID);
	disconnect(_selectedPort, &QSerialPort::readyRead,
		this, &MainWindow::checkContact);
	disconnect(_socket, &QWebSocket::textMessageReceived,
		this, &MainWindow::receiveContact);
}

void MainWindow::enterAlertContact()
{
	QMessageBox msgBox;
	msgBox.setText("Are you sure?");
	msgBox.setInformativeText(
		"Press yes to confirm you have tested positive for coronavirus.");
	msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
	int ret{ msgBox.exec() };
	switch (ret)
	{
	case QMessageBox::Yes:
		_socket->open(_url);
		_serverTimer.start(_serverTimeout);
		connect(_socket, &QWebSocket::connected,
			this, &MainWindow::requestUUIDAgain);
		break;
	case QMessageBox::No:
		emit goHome();
		break;
	}
}

void MainWindow::requestUUIDAgain()
{
	_serverTimer.stop();
	disconnect(_socket, &QWebSocket::connected,
		this, &MainWindow::requestUUIDAgain);
	updateSelectedPort();
	_selectedPort->open(QIODevice::ReadWrite);
	_selectedPort->write("uuid\n");
	_serialTimer.start(_serialTimeout);
	connect(_selectedPort, &QSerialPort::readyRead,
		this, &MainWindow::alertContact);
}

void MainWindow::alertContact()
{
	if (_selectedPort->canReadLine())
	{
		QString msg{ _selectedPort->readLine() };
		if (msg.contains("uuid:"))
		{
			_serialTimer.stop();
			disconnect(_selectedPort, &QSerialPort::readyRead,
				this, &MainWindow::alertContact);
			_socket->sendTextMessage("uhoh:" + msg.mid(5));
			emit goHome();
		}
	}
}

void MainWindow::exitAlertContact()
{
	_socket->close();
	_selectedPort->close();
	_serverTimer.stop();
	_serialTimer.stop();
	disconnect(_socket, &QWebSocket::connected,
		this, &MainWindow::checkContact);
	disconnect(_socket, &QWebSocket::connected,
		this, &MainWindow::alertContact);
}


void MainWindow::timeout(int type)
{
	QMessageBox msgBox;
	switch (type)
	{
	case 0:
		msgBox.setText("Server timed out.");
		msgBox.setInformativeText("Check internet connection.");
		break;
	case 1:
		msgBox.setText("Serial connection timed out.");
		msgBox.setInformativeText("Check device is connected and correct serial port is selected.");
		break;
	default:
		msgBox.setText("wait what");
	}
	msgBox.exec();

	emit goHome();

}

void MainWindow::socketError(QAbstractSocket::SocketError err)
{
	std::cout << "socket error: " << _socket->errorString().toStdString() << '\n';
}

void MainWindow::serialError(QSerialPort::SerialPortError err)
{
	if (_selectedPort->errorString() != "No error")
		std::cout << "Serial error: " << _selectedPort->errorString().toStdString() << '\n';
}