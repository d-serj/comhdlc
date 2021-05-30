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
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::on_buttonConnect_clicked()
{
    if (hdlc == nullptr)
    {
        hdlc = new comhdlc(ui->comboBox->currentText());
    }
}

void MainWindow::on_buttonDisconnect_clicked()
{
    if (hdlc != nullptr)
    {
        delete hdlc;
        hdlc = nullptr;
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
