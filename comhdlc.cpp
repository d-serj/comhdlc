#include "comhdlc.h"
#include "minihdlc.h"

#include <QSerialPort>
#include <QDebug>
#include <QDataStream>
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
        delete serial_port;
        serial_port = nullptr;
        return;
    }

    serial_port->setPortName(com_port_name);
    serial_port->setBaudRate(QSerialPort::Baud9600);
    if (serial_port->open(QIODevice::ReadWrite) == false)
    {
        qDebug() << com_port_name << " cannot be opened";
        qDebug() << "Error: " << serial_port->error();
        delete serial_port;
        serial_port = nullptr;
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

void comhdlc::transfer_file(const QByteArray &file, QString file_name)
{
    file_chunks.clear();
    file_send.clear();
    file_send = file;

    QDataStream to_send(&file_send, QIODevice::ReadOnly);
    to_send.setByteOrder(QDataStream::LittleEndian);

    // Split file to chunks
    while (!to_send.atEnd())
    {
        QByteArray buffer(MINIHDLC_MAX_FRAME_LENGTH, '\0');

        for (quint16 i = 0; i < MINIHDLC_MAX_FRAME_LENGTH; ++i)
        {
            char byte = 0;
            const qint16 bytes_read = to_send.readRawData(&byte, 1);
            if (bytes_read <= 0)
            {
                break;
            }

            buffer.insert(i, byte);
        }

        file_chunks.append(buffer);
    }

    const char answer[] = { eComhdlcFrameType_ACK, eCmdWriteFile };
    set_expected_answer(answer, sizeof(answer));

    send_command(eCmdWriteFile);

    Q_ASSERT(timer != nullptr);
    Q_ASSERT(!timer->isActive());

    const bool disconnected = disconnect(timer, &QTimer::timeout, this, &comhdlc::send_handshake);
    Q_ASSERT(disconnected);

    connect(timer, &QTimer::timeout, this, &comhdlc::file_send_routine);
    timer->start(5);
    file_chunk_current = 0;
}

void comhdlc::file_send_routine()
{
    if (answer_received)
    {
        ++file_chunk_current;
    }

    send_data(file_chunks.at(file_chunk_current));

    // send_data(buffer);
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
    if (static_cast<quint64>(send_buffer.size()) == bytes)
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
    Q_ASSERT(buff_len > 0);

    QByteArray answer((const char*)buff, buff_len);

    qDebug() << "Expected answer: " << expected_answer;
    qDebug() << "Answer received: " << answer;

    if (expected_answer == answer)
    {
        answer_received = true;
        expected_answer.clear();
    }

    serial_port->clear(QSerialPort::AllDirections);
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
        set_expected_answer(ans, sizeof(ans));
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
    QByteArray data(2, '\0');
    data[0] = eComhdlcFrameType_REQ;
    data[1] = command;

    send_data(data);
}

void comhdlc::set_expected_answer(const char *answer, qsizetype answer_size)
{
    expected_answer.clear();
    expected_answer.append(answer, answer_size);
}
