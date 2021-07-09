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
}

MainWindow::~MainWindow()
{
    delete ui;

    if (hdlc != nullptr)
    {
        delete hdlc;
        hdlc = nullptr;
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
            connect(hdlc, &comhdlc::device_connected, this, &MainWindow::comhdlc_device_connected);
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
    }
}

void MainWindow::on_file_dialog_clicked()
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
        file.close();
    }
}


void MainWindow::comhdlc_device_connected(bool connected)
{
    QString str = connected ? "connected" : "disconnected";
    qDebug() << "Device " << str;
}


void MainWindow::on_buttonSendFile_clicked()
{
    if (file_opened.isEmpty())
    {
        log_message("[ERROR] File is not opened");
        return;
    }

    if (hdlc)
    {
        hdlc->transfer_file(file_opened, file_name);
        file_opened.clear();
    }
}

void MainWindow::log_message(const QString &string)
{
    Q_ASSERT(!string.isEmpty());

    ui->text_log->append(string);
}