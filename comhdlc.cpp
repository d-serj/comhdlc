#include "comhdlc.h"
#include "minihdlc.h"

#include <QSerialPort>
#include <QDebug>
#include <QDataStream>
#include <QVector>
#include <QByteArray>
#include <QByteArrayList>

QSerialPort* comhdlc::serial_port = nullptr;
bool comhdlc::answer_received     = false;
quint64 comhdlc::bytes_written    = 0;
QByteArray comhdlc::expected_answer;

comhdlc::comhdlc(QString comName)
    : com_port_name(comName)
{
    if (com_port_name.isEmpty())
    {
        return;
    }

    serial_port = new QSerialPort(this);

    if (serial_port->isOpen())
    {
        qDebug() << "Serial port " << serial_port->portName() << " is already opened";
        return;
    }

    serial_port->setPortName(com_port_name);
    serial_port->setBaudRate(QSerialPort::Baud9600);
    if (serial_port->open(QIODevice::ReadWrite) == false)
    {
        qDebug() << com_port_name << " cannot be opened";
        qDebug() << "Error: " << serial_port->error();
    }
    else
    {
        qDebug() << com_port_name << " is opened";
        serial_port->clear(QSerialPort::AllDirections);
        minihdlc_init(send_byte, process_buffer);
        connect(serial_port, &QSerialPort::readyRead, this, &comhdlc::comport_data_available);
        connect(serial_port, &QSerialPort::errorOccurred, this, &comhdlc::comport_error_handler);
        connect(serial_port, &QSerialPort::bytesWritten, this, &comhdlc::comport_bytes_written);

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
        serial_port->clear(QSerialPort::AllDirections);
        serial_port->close();
    }

    delete serial_port;
    serial_port = nullptr;
}

bool comhdlc::is_comport_connected(void)
{
    if (serial_port != nullptr)
    {
        return serial_port->isOpen();
    }

    return false;
}

void comhdlc::transfer_file(const QByteArray &file)
{
    file_send.clear();
    file_send = file;

    QDataStream to_send(&file_send, QIODevice::ReadOnly);
    to_send.setByteOrder(QDataStream::LittleEndian);
    QByteArray buffer(MINIHDLC_MAX_FRAME_LENGTH, '\0');

    quint16 i = 0;
    for (; (i < MINIHDLC_MAX_FRAME_LENGTH) && !to_send.atEnd(); ++i)
    {
        char byte = 0;
        to_send.readRawData(&byte, 1);
        buffer.insert(i, byte);
    }

    QVector<QByteArray> file_chunks;
    //file_chunks.push_back()

    const char answer[] = { eComhdlcFrameType_ACK, eCmdWriteFile };
    QByteArray buff(answer, 2);
    set_expected_answer(buff);

    send_command(eCmdWriteFile);
    //send_data(buffer);
    // wait for response
    // send file name and file size
    // send chunk
    // wait for response
    // send chunk
    // wait for response
    // send finish
}

void comhdlc::comport_data_available()
{
    char byte = 0;
    while(serial_port->read(&byte, 1) > 0)
    {
        minihdlc_char_receiver((quint8)byte);
    }
}

void comhdlc::comport_bytes_written(quint64 bytes)
{
    if (send_buffer.count() == bytes)
    {
        qDebug() << "Bytes written" << bytes;

    }
}

void comhdlc::comport_error_handler(QSerialPort::SerialPortError serialPortError)
{
    qDebug() << "Serial port " << com_port_name << " error occured " << serialPortError;
}

void comhdlc::send_byte(uint8_t data)
{
    serial_port->write((const char*)&data, 1);
}

void comhdlc::process_buffer(const uint8_t *buff, uint16_t buff_len)
{
    Q_ASSERT(buff != nullptr);

    QByteArray answer((const char*)buff, buff_len);

    qDebug() << "Expected answer: " << expected_answer;
    qDebug() << "Answer received: " << answer;

    if (expected_answer == answer)
    {
        answer_received = true;
        expected_answer.clear();
    }
    else
    {
        serial_port->clear(QSerialPort::AllDirections);
    }
}

void comhdlc::send_data(const QByteArray &data)
{
    const quint16 data_size = data.size();

    if (data_size > MINIHDLC_MAX_FRAME_LENGTH)
    {
        Q_ASSERT(0);
        return;
    }

    send_buffer = data;

    minihdlc_send_frame(reinterpret_cast<const uint8_t*>(data.constData()), data_size);
}

void comhdlc::send_handshake()
{
    Q_ASSERT(timer != nullptr);

    if (answer_received == false)
    {
        const char ans[] = { eComhdlcFrameType_ACK, eComHdlcAnswer_HandShake };
        QByteArray answer(ans, 2);
        set_expected_answer(answer);
    }
    else
    {
        timer->stop();
        answer_received = false;
    }

    const quint8 raw[] = { 0xBE, 0xEF };
    QByteArray data = QByteArray::fromRawData((const char*)raw, sizeof(raw));
    send_data(data);
}

void comhdlc::send_command(uint8_t command)
{
    const QByteArray data((const char*)&command, 1);
    send_data(data);
}

void comhdlc::set_expected_answer(const QByteArray &answer)
{
    expected_answer.clear();
    expected_answer = answer;
}
