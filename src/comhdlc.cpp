#include "comhdlc.h"
#include "minihdlc.h"

#include <QSerialPort>
#include <QDebug>
#include <QDataStream>
#include <QByteArray>
#include <QByteArrayList>
#include <tinyframe/TinyFrame.h>

QSerialPort *comhdlc::serial_port = nullptr;
quint64 comhdlc::bytes_written    = 0;
TinyFrame *comhdlc::tf            = nullptr;

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
        tf = TF_Init(TF_MASTER);
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

    if (tf != nullptr)
    {
        TF_DeInit(tf);
        tf = nullptr;
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

bool comhdlc::is_comport_connected(void) const
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
    send_data(file_chunks.at(file_chunk_current));

    const char answer[] = { eComhdlcFrameType_ACK, eCmdWriteFile };

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
    qDebug() << "Bytes need to be written: " << send_buffer.size()
             << ". Originally written bytes" << bytes;
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

    qDebug() << "Answer received. Answer len: " << buff_len;

    TF_Accept(tf, buff, buff_len);

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

    TF_SendSimple(tf, 1, reinterpret_cast<const uint8_t*>(data.constData()), data_size);
}

void comhdlc::send_handshake()
{
    Q_ASSERT(timer != nullptr);

    const quint8 raw[] = { 0xBE, 0xEF };
    QByteArray data = QByteArray::fromRawData((const char*)raw, sizeof(raw));
    TF_QuerySimple(tf,
                   eComHdlcAnswer_HandShake,
                   &raw,
                   sizeof (raw),
                   [](TinyFrame *tf, TF_Msg *msg)
                   {
                      Q_UNUSED(tf);
                      Q_UNUSED(msg);
                      return TF_CLOSE;
                   },
                   0
    );
}

void comhdlc::send_command(uint8_t command)
{
    QByteArray data(1, '\0');
    data[0] = command;

    send_data(data);
}

extern "C" void TF_WriteImpl(TinyFrame *tf, const uint8_t *buff, uint32_t len);

void TF_WriteImpl(TinyFrame *tf, const uint8_t *buff, uint32_t len)
{
    Q_UNUSED(tf);
    Q_ASSERT(buff != nullptr);

    minihdlc_send_frame(buff, len);
}
