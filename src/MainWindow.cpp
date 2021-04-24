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
	_selectedPort{ new QSerialPort() }
{
	ui->setupUi(this);
	populateSerialPortCombo();
	updateSelectedPort();

	QState* idle{ new QState() };
	QState* serverConnect{ new QState() };
	QState* serverTimeout{ new QState() };
	QState* transfer{ new QState() };
	QState* transferTimeout{ new QState() };
	QState* checkContact{ new QState() };
	QState* alertContact{ new QState() };

	idle->addTransition(ui->syncButton, &QPushButton::clicked, serverConnect);
	serverConnect->addTransition(_socket, &QWebSocket::connected, transfer);
	transfer->addTransition(this, &MainWindow::goHome, idle);

	serverConnect->addTransition(&_timer, &QTimer::timeout, serverTimeout);
	transfer->addTransition(&_timer, &QTimer::timeout, transferTimeout);

	serverTimeout->addTransition(this, &MainWindow::goHome, idle);
	transferTimeout->addTransition(this, &MainWindow::goHome, idle);

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

	connect(serverTimeout, &QState::entered,
		this, [this]() {timeout(0);});
	connect(transferTimeout, &QState::entered,
		this, [this]() {timeout(1);});

	connect(checkContact, &QState::entered, this, &MainWindow::enterCheckContact);
	connect(checkContact, &QState::exited, this, &MainWindow::exitCheckContact);

	connect(alertContact, &QState::entered, this, &MainWindow::enterAlertContact);
	connect(alertContact, &QState::exited, this, &MainWindow::exitAlertContact);

	_stateMachine->addState(idle);
	_stateMachine->addState(serverConnect);
	_stateMachine->addState(serverTimeout);
	_stateMachine->addState(transfer);
	_stateMachine->addState(transferTimeout);
	_stateMachine->addState(checkContact);
	_stateMachine->addState(alertContact);
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
	std::cout << "connecting to server...\n";
	_timer.start(5000);
	ui->progressBar->setValue(1);
}

void MainWindow::exitServerConnect()
{
	std::cout << "Successfully connected.\n";
	_timer.stop();
}

void MainWindow::enterTransfer()
{
	ui->progressBar->setValue(2);
	updateSelectedPort();
	_selectedPort->open(QIODevice::ReadWrite);
	_selectedPort->write("sync\n");

	connect(_selectedPort, &QSerialPort::readyRead,
		this, &MainWindow::serialHandshake);

	_timer.start(1000);
}

void MainWindow::serialHandshake()
{
	if (_selectedPort->canReadLine())
	{
		_timer.stop();
		QString msg{ _selectedPort->readLine() };
		if (msg.contains("ts:"))
		{
			_timeStamp = msg.right(3).toInt();
			std::cout << "Time Stamp: " << _timeStamp << '\n';
			disconnect(_selectedPort, &QSerialPort::readyRead,
				this, &MainWindow::serialHandshake);
			connect(_selectedPort, &QSerialPort::readyRead,
				this, &MainWindow::retrieveData);
		}
	}
}

void MainWindow::retrieveData()
{
	_timer.stop();

	if (_selectedPort->canReadLine());
	{
		QString msg{ _selectedPort->readLine() };
		if (msg.contains("done"))
		{
			std::cout << "done\n";
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
	_timer.stop();
}

void MainWindow::enterCheckContact()
{
	_socket->open(_url);
	_timer.start(5000);
	connect(_socket, &QWebSocket::connected,
		this, &MainWindow::checkContact);
}

void MainWindow::checkContact()
{
	_timer.stop();
	connect(_socket, &QWebSocket::textMessageReceived,
		this, &MainWindow::receiveContact);
	_socket->sendTextMessage("chk\n");

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
	_timer.stop();
	disconnect(_socket, &QWebSocket::connected,
		this, &MainWindow::checkContact);
	disconnect(_socket, &QWebSocket::textMessageReceived,
		this, &MainWindow::checkContact);
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
		_timer.start(5000);
		connect(_socket, &QWebSocket::connected,
			this, &MainWindow::alertContact);
		break;
	case QMessageBox::No:
		emit goHome();
		break;
	}
}

void MainWindow::exitAlertContact()
{
	_socket->close();
	_timer.stop();
	disconnect(_socket, &QWebSocket::connected,
		this, &MainWindow::checkContact);
	disconnect(_socket, &QWebSocket::connected,
		this, &MainWindow::alertContact);
}

void MainWindow::alertContact()
{
	_socket->sendTextMessage("uhoh\n");
	_timer.stop();
	emit goHome();
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
	std::cout << "Serial error: " << _selectedPort->errorString().toStdString() << '\n';
}

void MainWindow::serialTimeout()
{
	std::cout << "Unable to communicate with beacon, serial timed out.\n";
	_selectedPort->close();
	_timer.stop();
}

void MainWindow::socketTimeout()
{
	std::cout << "Unable to communicate with server, timed out.\n";
	_socket->close();
	_timer.stop();
}