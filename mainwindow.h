#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QLayout>
#include <QSerialPort>

#include "comhdlc.h"

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

    void on_file_dialog_clicked();

    void comhdlc_device_connected(bool connected);

    void on_buttonSendFile_clicked();

private:
    void log_message(const QString &string);

    Ui::MainWindow *ui = nullptr;
    comhdlc *hdlc      = nullptr;
    QString file_name  = "";
    QByteArray file_opened;
};
#endif // MAINWINDOW_H
