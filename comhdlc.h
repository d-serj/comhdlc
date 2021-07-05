#ifndef COMHDLC_H
#define COMHDLC_H

#include <cstdint>
#include <QSerialPort>
#include <QObject>
#include <QTimer>

class comhdlc : public QObject
{
    Q_OBJECT
public:
    comhdlc(QString comName);
    ~comhdlc();
    void send_data(const QByteArray &data);
    void transfer_file(const QByteArray &file);
    bool is_comport_connected(void);

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
        eComHdlcAnswer_HandShake = 2,
    };


    QString com_port_name;
    QTimer *timer = nullptr;
    QByteArray file_send;
    QByteArray send_buffer;

    static bool answer_received;
    static QByteArray expected_answer;
    static quint64 bytes_written;
    static QSerialPort *serial_port;

    static void send_byte(uint8_t data);
    static void process_buffer(const uint8_t* buff, uint16_t buff_len);
    void send_handshake(void);
    void send_command(uint8_t command);
    void set_expected_answer(const QByteArray &answer);

private slots:
    void comport_data_available();
    void comport_error_handler(QSerialPort::SerialPortError serialPortError);
    void comport_bytes_written(quint64 bytes);

signals:
    void device_connected(bool connected);
};

#endif // COMHDLC_H
