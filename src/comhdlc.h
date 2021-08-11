#ifndef COMHDLC_H
#define COMHDLC_H

#include <cstdint>
#include <QSerialPort>
#include <QObject>
#include <QTimer>

#include <tinyframe/TinyFrame.h>

enum eComHdlcFrameTypes
{
    eComhdlcFrameType_ACK  = 1,
    eComhdlcFrameType_REQ  = 2,
    eComhdlcFrameType_DATA = 3,
    eComhdlcFrameType_ERR  = 4,
};

enum eComHdlcCommands
{
    eCmdWriteFile            = 1,
    eCmdWriteFileSize        = 2,
    eCmdWriteFileFinish      = 3,
    eComHdlcAnswer_HandShake = 4,
};

class comhdlc : public QObject
{
    Q_OBJECT
public:
    comhdlc(QString comName);
    ~comhdlc();
    void transfer_file(const QByteArray &file, QString file_name);
    void transfer_file_chunk();
    bool is_comport_connected(void) const;
    void handshake_routine_stop(void);
    static void comport_send_buff(const uint8_t *data, quint16 data_len);

private:
    QString com_port_name;
    QTimer *timer_handshake    = nullptr;
    QTimer *timer_tf = nullptr;
    QByteArray file_send;
    QByteArray send_buffer;
    QByteArrayList file_chunks;

    static TinyFrame *tf;
    static quint64 bytes_written;
    static QSerialPort *serial_port;

    void send_handshake(void);
    void tf_handle_tick(void);

private slots:
    void comport_data_available();
    void comport_error_handler(QSerialPort::SerialPortError serialPortError);
    void comport_bytes_written(quint64 bytes);

signals:
    void device_connected(bool connected);
    void file_was_transferred(bool transferred);
    void file_chunk_transferred(quint16 chunk_size);
};

#endif // COMHDLC_H
