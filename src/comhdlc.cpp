/**
 * @file comhdlc.cpp
 */

#include "comhdlc.h"

#include <QSerialPort>
#include <QDebug>
#include <QDataStream>
#include <QByteArray>
#include <QByteArrayList>
#include <tinyframe/TinyFrame.h>

comhdlc* comhdlc::comhdlc_ptr = nullptr;

/** Callbacks for TinyFrame */
static TF_Result tf_write_file_clbk(TinyFrame *tf, TF_Msg *msg);
static TF_Result tf_write_file_size_clbk(TinyFrame *tf, TF_Msg *msg);
static TF_Result tf_handshake_clbk(TinyFrame *tf, TF_Msg *msg);

comhdlc::comhdlc(QString comName)
    : com_port_name{comName},
      serial_port{new QSerialPort(this)}
{
    if (com_port_name.isEmpty())
    {
        Q_ASSERT(0);
        return;
    }

    if (serial_port->isOpen())
    {
        qDebug() << "[ERROR] Serial port " << serial_port->portName() << " is already opened";
        delete serial_port;
        serial_port = nullptr;
        return;
    }

    serial_port->setPortName(com_port_name);
    serial_port->setBaudRate(QSerialPort::Baud38400);
    serial_port->setDataBits(QSerialPort::Data8);
    serial_port->setParity(QSerialPort::NoParity);
    serial_port->setStopBits(QSerialPort::OneStop);

    if (serial_port->open(QIODevice::ReadWrite))
    {
        qDebug() << "[INFO] " << com_port_name << " is opened";
        serial_port->clear(QSerialPort::AllDirections);
        connect(serial_port, &QSerialPort::readyRead, this, &comhdlc::comport_data_available);
        connect(serial_port, &QSerialPort::errorOccurred, this, &comhdlc::comport_error_handler);
        connect(serial_port, &QSerialPort::bytesWritten, this, &comhdlc::comport_bytes_written);

        tiny_frame = TF_Init(TF_MASTER);

        timer_handshake = new QTimer(this);
        timer_tf        = new QTimer(this);
        connect(timer_tf,        &QTimer::timeout, this, &comhdlc::tf_handle_tick);
        connect(timer_handshake, &QTimer::timeout, this, &comhdlc::send_handshake);

        timer_tf->start(1);

        const quint16 timeout_ms = 100;
        timer_handshake->start(timeout_ms);
        qDebug() << "[INFO] Handshake is started with " << timeout_ms << " ms timeout";

        // Register Tiny Frame callbacks
        TF_AddTypeListener(tiny_frame, eComHdlcAnswer_HandShake, tf_handshake_clbk);
        TF_AddTypeListener(tiny_frame, eCmdWriteFile           , tf_write_file_clbk);
        TF_AddTypeListener(tiny_frame, eCmdWriteFileSize       , tf_write_file_size_clbk);

        comhdlc_ptr = this;
    }
    else
    {
        qDebug() << "[ERROR] " << com_port_name << " cannot be opened. Error: " << serial_port->error();
        delete serial_port;
        serial_port = nullptr;
    }
}

comhdlc::~comhdlc()
{
    if (timer_handshake)
    {
        timer_handshake->stop();
        delete timer_handshake;
        timer_handshake = nullptr;
    }

    if (timer_tf)
    {
        timer_tf->stop();
        delete timer_tf;
        timer_tf = nullptr;
    }

    if (tiny_frame)
    {
        TF_DeInit(tiny_frame);
        tiny_frame = nullptr;
    }

    if (serial_port)
    {
        if (serial_port->isOpen())
        {
            qDebug() << "[INFO] Serial port " << serial_port->portName() << " closed";
            serial_port->clear(QSerialPort::AllDirections);
            serial_port->close();
        }

        delete serial_port;
        serial_port = nullptr;
    }

    comhdlc_ptr = nullptr;
}

bool comhdlc::is_comport_connected(void) const
{
    if (serial_port != nullptr)
    {
        return serial_port->isOpen();
    }

    return false;
}

void comhdlc::handshake_routine_stop()
{
    if (timer_handshake)
    {
        timer_handshake->stop();
    }
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
        QByteArray buffer;

        for (quint16 i = 0; i < TF_SENDBUF_LEN; ++i)
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

    const uint32_t file_size = static_cast<uint32_t>(file_send.size());

    // Wait for 10 seconds for the response
    TF_QuerySimple(tiny_frame,
                   eCmdWriteFileSize,
                   (const uint8_t*)&file_size,
                   sizeof(uint32_t),
                   tf_write_file_size_clbk,
                   10000);
}

void comhdlc::transfer_file_chunk()
{
    // The file was transferred
    if (file_chunk_current >= file_chunks.size())
    {
        emit file_was_transferred(true);
        return;
    }

    QByteArray chunk        = file_chunks.at(file_chunk_current);
    const quint16 data_size = static_cast<quint16>(chunk.size());

    emit file_chunk_transferred(data_size);

    send_buffer = chunk;

    TF_QuerySimple(tiny_frame,
                   eCmdWriteFile,
                   reinterpret_cast<const uint8_t*>(chunk.constData()),
                   data_size,
                   tf_write_file_clbk,
                   2000);

    ++file_chunk_current;
}

void comhdlc::comport_data_available()
{
    quint32 bytes_received = 0;
    char byte              = 0;

    while(serial_port->read(&byte, 1) > 0)
    {
        TF_AcceptChar(tiny_frame, (quint8)byte);

        ++bytes_received;
    }

    qDebug() << "[INFO] " << bytes_received << " bytes were received";
}

void comhdlc::comport_bytes_written(quint64 bytes)
{
    qDebug() << "[INFO] " << bytes << " Bytes were written";
}

void comhdlc::comport_error_handler(QSerialPort::SerialPortError serialPortError)
{
    qDebug() << "[ERROR] Serial port " << com_port_name << " error occured " << serialPortError;
}

void comhdlc::comport_send_buff(const quint8 *data, quint16 data_len)
{
    Q_ASSERT(data);
    Q_ASSERT(data_len > 0);

    if (serial_port)
    {
        serial_port->write((const char*)data, data_len);
    }
}

void comhdlc::send_handshake()
{
    Q_ASSERT(timer_handshake != nullptr);

    const quint8 raw[] = { 0xBE, 0xEF };
    TF_QuerySimple(tiny_frame,
                   eComHdlcAnswer_HandShake,
                   raw,
                   sizeof (raw),
                   tf_handshake_clbk,
                   100
                   );
}

void comhdlc::tf_handle_tick()
{
    Q_ASSERT(tiny_frame);
    TF_Tick(tiny_frame);
}

comhdlc *comhdlc_get_instance()
{
    return comhdlc::comhdlc_ptr;
}

//
// TF callbacks
//

static TF_Result tf_write_file_size_clbk(TinyFrame *tf, TF_Msg *msg)
{   
    if (msg->type == eCmdWriteFileSize)
    {
        if (comhdlc_get_instance())
        {
            comhdlc_get_instance()->transfer_file_chunk();
        }

        return TF_CLOSE;
    }

    return TF_NEXT;
}

static TF_Result tf_write_file_clbk(TinyFrame *tf, TF_Msg *msg)
{
    Q_UNUSED(tf);
    Q_ASSERT(msg != nullptr);

    if (msg->type == eCmdWriteFile)
    {
        if (comhdlc_get_instance())
        {
            comhdlc_get_instance()->transfer_file_chunk();
        }

        qDebug() << "[INFO] File write callback TinyFrame";

        return TF_STAY;
    }

    return TF_NEXT;
}

static TF_Result tf_handshake_clbk(TinyFrame *tf, TF_Msg *msg)
{
    Q_UNUSED(tf);
    Q_ASSERT(msg != nullptr);

    if (msg->type == eComHdlcAnswer_HandShake)
    {
        if (comhdlc_get_instance())
        {
            comhdlc_get_instance()->handshake_routine_stop();
            emit comhdlc_get_instance()->device_connected(true);
        }

       return TF_CLOSE;
    }

    return TF_NEXT;
}

extern "C" void TF_WriteImpl(TinyFrame *tf, const uint8_t *buff, uint32_t len);

void TF_WriteImpl(TinyFrame *tf, const uint8_t *buff, uint32_t len)
{
    Q_UNUSED(tf);
    Q_ASSERT(buff != nullptr);

    if (comhdlc_get_instance())
    {
        comhdlc_get_instance()->comport_send_buff(buff, len);
    }
}
