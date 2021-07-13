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

static quint32 file_chunk_current = 0;
static bool bdevice_connected   = false;

/** Callbacks for TinyFrame */
static TF_Result tf_write_file_clbk(TinyFrame *tf, TF_Msg *msg);
static TF_Result tf_handshake_clbk(TinyFrame *tf, TF_Msg *msg);

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
    serial_port->setBaudRate(QSerialPort::Baud19200);
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

        connect(serial_port, &QSerialPort::readyRead, this, &comhdlc::comport_data_available);
        connect(serial_port, &QSerialPort::errorOccurred, this, &comhdlc::comport_error_handler);
        connect(serial_port, &QSerialPort::bytesWritten, this, &comhdlc::comport_bytes_written);

        minihdlc_init(send_byte, process_buffer);
        tf = TF_Init(TF_MASTER);

        timer    = new QTimer(this);
        timer_tf = new QTimer(this);
        connect(timer, &QTimer::timeout, this, &comhdlc::send_handshake);
        connect(timer, &QTimer::timeout, this, &comhdlc::tf_handle_tick);
        const quint16 timeout_ms = 100;
        timer->start(timeout_ms);
        timer_tf->start(1);
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

    if (timer_tf != nullptr)
    {
        timer_tf->stop();
        delete timer_tf;
        timer_tf = nullptr;
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
    file_send          = file;
    file_chunk_current = 0;

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
}

void comhdlc::file_send_routine()
{
    QByteArray chunk        = file_chunks.at(file_chunk_current);
    const quint16 data_size = chunk.size();

    if (data_size > MINIHDLC_MAX_FRAME_LENGTH)
    {
        Q_ASSERT(0);
        return;
    }

    send_buffer = chunk;

    TF_SendSimple(tf, 1, reinterpret_cast<const uint8_t*>(chunk.constData()), data_size);

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

void comhdlc::send_command(uint8_t command)
{
    QByteArray data(1, '\0');
    data[0] = command;

    // TODO
}

static TF_Result tf_write_file_clbk(TinyFrame *tf, TF_Msg *msg)
{
    Q_UNUSED(tf);
    Q_ASSERT(msg != nullptr);

    if (msg->type == eCmdWriteFile)
    {
        ++file_chunk_current;
        return TF_CLOSE;
    }

    return TF_NEXT;
}

static TF_Result tf_handshake_clbk(TinyFrame *tf, TF_Msg *msg)
{
    Q_UNUSED(tf);
    Q_ASSERT(msg != nullptr);

    if (msg->type == eComHdlcAnswer_HandShake)
    {
       bdevice_connected = true;
       return TF_CLOSE;
    }

    return TF_NEXT;
}

void comhdlc::send_handshake()
{
    Q_ASSERT(timer != nullptr);

    const quint8 raw[] = { 0xBE, 0xEF };
    TF_QuerySimple(tf,
                   eComHdlcAnswer_HandShake,
                   raw,
                   sizeof (raw),
                   tf_handshake_clbk,
                   100
                   );
}

void comhdlc::tf_handle_tick()
{
    Q_ASSERT(tf);
    TF_Tick(tf);
}

extern "C" void TF_WriteImpl(TinyFrame *tf, const uint8_t *buff, uint32_t len);

void TF_WriteImpl(TinyFrame *tf, const uint8_t *buff, uint32_t len)
{
    Q_UNUSED(tf);
    Q_ASSERT(buff != nullptr);

    minihdlc_send_frame(buff, len);
}
