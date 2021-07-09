#ifndef COMHDLC_H
#define COMHDLC_H

#include <cstdint>
#include <QSerialPort>
#include <QObject>
#include <QTimer>

#include <tinyframe/TinyFrame.h>

class comhdlc : public QObject
{
    Q_OBJECT
public:
    comhdlc(QString comName);
    ~comhdlc();
    void send_data(const QByteArray &data);
    void transfer_file(const QByteArray &file, QString file_name);
    bool is_comport_connected(void) const;

private:

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
        eCmdWriteFileFinish      = 2,
        eComHdlcAnswer_HandShake = 3,
    };


    QString com_port_name;
    QTimer *timer = nullptr;
    QByteArray file_send;
    QByteArray send_buffer;
    QByteArrayList file_chunks;
    quint32 file_chunk_current;


    static TinyFrame *tf;
    static quint64 bytes_written;
    static QSerialPort *serial_port;

    static void send_byte(uint8_t data);
    static void process_buffer(const uint8_t* buff, uint16_t buff_len);
    void send_handshake(void);
    void file_send_routine(void);
    void send_command(uint8_t command);

    /** Callbacks for TinyFrame */

private slots:
    void comport_data_available();
    void comport_error_handler(QSerialPort::SerialPortError serialPortError);
    void comport_bytes_written(quint64 bytes);

signals:
    void device_connected(bool connected);
};

#endif // COMHDLC_H
