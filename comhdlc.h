#ifndef COMHDLC_H
#define COMHDLC_H

#include <cstdint>
#include <QSerialPort>
#include <QObject>

class comhdlc : public QObject
{
    Q_OBJECT
public:
    comhdlc(QString comName);
    ~comhdlc();
    void send_data(const QByteArray &data);

private:
    QString com_port_name;
    static QSerialPort *serial_port;
    static void send_byte(uint8_t data);
    static void process_buffer(const uint8_t* buff, uint16_t buff_len);

private slots:
    void comport_data_available();
};

#endif // COMHDLC_H
