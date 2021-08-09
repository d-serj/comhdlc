/**
 * @file mainwindow.cpp
 */

#include <QtSerialPort/QSerialPort>
#include <QtSerialPort/QSerialPortInfo>
#include <QByteArray>
#include <QList>
#include <QDebug>
#include <QFile>
#include <QFileDialog>
#include <QMessageBox>

#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include "comhdlc.h"
#include "ledindicator.h"


MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    ui->selected_file_name->setReadOnly(true);

    ui->buttonDisconnect->setEnabled(false);

    const auto serialPorts = QSerialPortInfo::availablePorts();

    if (serialPorts.empty())
    {
        QMessageBox::critical(this, "Error", "There are no available comports in your system", QMessageBox::Ok);
    }

    for (const QSerialPortInfo &info : serialPorts)
    {
        ui->comboBox->addItem(info.portName());
    }
    ui->comboBox->setInsertPolicy(QComboBox::NoInsert);

    ui->text_log->setReadOnly(true);

    led_indicator = new LedIndicator(this);
    ui->gridLayout->addWidget(led_indicator, 1, 0);

    ui->file_send_progress->reset();
    ui->file_send_progress->hide();
    ui->file_send_progress->setMinimum(0);
}

MainWindow::~MainWindow()
{
    delete ui;

    if (hdlc != nullptr)
    {
        delete hdlc;
        hdlc = nullptr;
    }

    if (led_indicator)
    {
        delete led_indicator;
        led_indicator = nullptr;
    }
}

void MainWindow::on_buttonConnect_clicked()
{
    if (hdlc == nullptr)
    {
        hdlc = new comhdlc(ui->comboBox->currentText());

        if (hdlc->is_comport_connected())
        {
            ui->buttonConnect->setEnabled(false);
            ui->buttonDisconnect->setEnabled(true);
            ui->comboBox->setEnabled(false);
            connect(hdlc, &comhdlc::device_connected,     this, &MainWindow::comhdlc_device_connected);
            connect(hdlc, &comhdlc::file_was_transferred, this, &MainWindow::comhdlc_file_transferred);
            connect(hdlc, &comhdlc::file_chunk_transferred, this, &MainWindow::comhdlc_chunk_transferred);
        }
        else
        {
            ui->buttonConnect->setEnabled(true);
            ui->buttonDisconnect->setEnabled(false);
            ui->comboBox->setEnabled(true);
            delete hdlc;
            hdlc = nullptr;
        }
    }
}

void MainWindow::on_buttonDisconnect_clicked()
{
    if (hdlc != nullptr)
    {
        delete hdlc;
        hdlc = nullptr;
        ui->buttonDisconnect->setEnabled(false);
        ui->buttonConnect->setEnabled(true);
        ui->comboBox->setEnabled(true);
        ui->file_send_progress->hide();
        log_message("[INFO] Device disconnected");
    }

    if (led_indicator)
    {
        led_indicator->setState(false);
    }
}

void MainWindow::comhdlc_device_connected(bool connected)
{
    QString res = connected ? "connected" : "disconnected";
    QString str = "[INFO] Device " + res;
    qDebug() << str;
    log_message(str);

    if (led_indicator)
    {
        led_indicator->setState(connected);
    }
}

void MainWindow::comhdlc_file_transferred(bool transferred)
{
    QString res = transferred ? "transferred" : "not transferred";
    QString str = "[INFO] File was " + res;
    qDebug() << str;
    log_message(str);

    ui->file_send_progress->setValue(100);
}

void MainWindow::comhdlc_chunk_transferred(quint16 chunk_size)
{
    file_size += chunk_size;
    ui->file_send_progress->setValue(file_size);
}

void MainWindow::log_message(const QString &string)
{
    Q_ASSERT(!string.isEmpty());

    ui->text_log->append(string);
}

void MainWindow::on_button_send_file_clicked()
{
    if (file_opened.isEmpty())
    {
        log_message("[ERROR] File is not opened");
    }
    else if (hdlc)
    {
        hdlc->transfer_file(file_opened, file_name);
        file_opened.clear();
        ui->file_send_progress->show();
        ui->selected_file_name->clear();
        file_size = 0;
    }
    else
    {
        log_message("[ERROR] Device is not connected yet");
    }
}


void MainWindow::on_button_file_dialog_clicked()
{
    file_name = QFileDialog::getOpenFileName(this,
        "Open firmware file", "", "Firmware file (*.bin)");
    if (file_name.isEmpty())
    {
        return;
    }

    QFile file(file_name);

    if (file.open(QIODevice::ReadOnly | QIODevice::ExistingOnly))
    {
        file_opened.clear();
        file_opened = file.readAll();
        qDebug() << "File " << file_name << " opened. Size is " << file_opened.size() << " " << file.size();
        ui->selected_file_name->setText(file_name);
        ui->file_send_progress->setMaximum(file.size());
        file.close();
    }

    ui->file_send_progress->setValue(0);
}

