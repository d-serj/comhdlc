#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QLayout>
#include <QSerialPort>

#include "comhdlc.h"
#include "ledindicator.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:

    void on_buttonDisconnect_clicked();

    void on_buttonConnect_clicked();

    void comhdlc_device_connected(bool connected);

    void comhdlc_file_transferred(bool transferred);

    void comhdlc_chunk_transferred(quint16 chunk_size);

    void on_button_send_file_clicked();

    void on_button_file_dialog_clicked();

private:
    void log_message(const QString &string);

    Ui::MainWindow *ui = nullptr;
    comhdlc *hdlc      = nullptr;
    QString file_name  = "";
    QByteArray file_opened;
    LedIndicator *led_indicator = nullptr;
    quint32 file_size = 0;
};
#endif // MAINWINDOW_H
