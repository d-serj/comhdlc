/**
 * @file mainwindow.cpp
 */

#include <QtSerialPort/QSerialPort>
#include <QtSerialPort/QSerialPortInfo>
#include <QByteArray>
#include <QList>
#include <QDebug>

#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include "comhdlc.h"


MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    ui->comboBox->setInsertPolicy(QComboBox::NoInsert);

    const auto serialPorts = QSerialPortInfo::availablePorts();
    for (const QSerialPortInfo &info : serialPorts)
    {
        ui->comboBox->addItem(info.portName());
    }

    ui->buttonDisconnect->setEnabled(false);
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

        if (hdlc->is_connected())
        {
            ui->buttonConnect->setEnabled(false);
            ui->buttonDisconnect->setEnabled(true);
            ui->comboBox->setEnabled(false);
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

void MainWindow::on_lineEdit_returnPressed()
{
    QString inputText = ui->lineEdit->text();
    qDebug() << "Send " << inputText;
    ui->lineEdit->clear();
    QByteArray arr(inputText.toStdString().c_str());

    if (hdlc)
    {
        hdlc->send_data(arr);
    }
}
