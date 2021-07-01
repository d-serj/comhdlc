#include "comhdlc.h"
#include "minihdlc.h"

#include <QSerialPort>
#include <QDebug>

QSerialPort* comhdlc::serial_port = nullptr;

comhdlc::comhdlc(QString comName)
{
    if (comName.isEmpty())
    {
        return;
    }

    this->com_port_name = comName;

    serial_port = new QSerialPort;

    if (serial_port->isOpen())
    {
        qDebug() << "Serial port " << serial_port->portName() << " is already opened";
        return;
    }

    if (comName.isEmpty())
    {
        qDebug() << "ComPort is empty";
        return;
    }

    serial_port->setPortName(comName);
    serial_port->setBaudRate(QSerialPort::Baud9600);
    if (serial_port->open(QIODevice::ReadWrite) == false)
    {
        qDebug() << comName << " cannot be opened";
        qDebug() << "Error: " << serial_port->error();
    }
    else
    {
        qDebug() << comName << " is opened";
        minihdlc_init(send_byte, process_buffer);
        connect(serial_port, &QSerialPort::readyRead, this, &comhdlc::comport_data_available);

        timer = new QTimer(this);
        connect(timer, &QTimer::timeout, this, &comhdlc::send_handshake);
        const quint16 timeout_ms = 100;
        timer->start(timeout_ms);
        qDebug() << "QTimer has started with " << timeout_ms << " ms timeout";
    }
}

comhdlc::~comhdlc()
{
    if (timer != nullptr)
    {
        timer->stop();
        delete timer;
        timer = nullptr;
    }

    if (serial_port == nullptr)
    {
        return;
    }

    if (serial_port->isOpen())
    {
        qDebug() << "Serial port " << serial_port->portName() << " closed";
        serial_port->close();
    }

    delete serial_port;
    serial_port = nullptr;
}

bool comhdlc::is_connected(void)
{
    if (serial_port != nullptr)
    {
        return serial_port->isOpen();
    }

    return false;
}

void comhdlc::comport_data_available()
{
    char byte = 0;
    while(serial_port->read(&byte, 1) > 0)
    {
        minihdlc_char_receiver((quint8)byte);
    }
}

void comhdlc::send_byte(uint8_t data)
{
    serial_port->write((const char*)&data, 1);
}

void comhdlc::process_buffer(const uint8_t *buff, uint16_t buff_len)
{
    QString output;
    for (int i = 0; i < buff_len; ++i)
    {
        QString valueInHex = QString("%1").arg(buff[i] , 0, 16);
        output += valueInHex;
        output += ' ';
    }

    qDebug() << "Received data: " << output << " with length:" << buff_len;
}

void comhdlc::send_data(const QByteArray &data)
{
    minihdlc_send_frame((const uint8_t*)data.constData(), data.size());
}

void comhdlc::send_handshake()
{
    Q_ASSERT(timer != nullptr);
    quint8 raw[] = { 0xBE, 0xEF };
    QByteArray data(reinterpret_cast<char*>(raw), 2);
    send_data(data);
}