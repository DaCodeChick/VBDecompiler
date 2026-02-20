// VBDecompiler - Visual Basic Decompiler
// Copyright (c) 2026 VBDecompiler Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "MainWindow.h"
#include "ui_MainWindow.h"
#include <QFileDialog>
#include <QMessageBox>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    setupConnections();
    
    setWindowTitle("VBDecompiler - Visual Basic 5/6 Decompiler");
    resize(1280, 800);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::setupConnections()
{
    connect(ui->actionOpen, &QAction::triggered, this, &MainWindow::onOpenFile);
    connect(ui->actionAbout, &QAction::triggered, this, &MainWindow::onAbout);
}

void MainWindow::onOpenFile()
{
    QString fileName = QFileDialog::getOpenFileName(
        this,
        tr("Open VB Executable"),
        QString(),
        tr("VB Files (*.exe *.dll *.ocx);;All Files (*)")
    );
    
    if (!fileName.isEmpty()) {
        loadFile(fileName);
    }
}

void MainWindow::onAbout()
{
    QMessageBox::about(
        this,
        tr("About VBDecompiler"),
        tr("<h3>VBDecompiler 1.0.0</h3>"
           "<p>A Ghidra-style decompiler for Visual Basic 5/6 executables.</p>"
           "<p>Built with C++23 and Qt 6.</p>"
           "<p><b>Features:</b></p>"
           "<ul>"
           "<li>P-Code and x86 disassembly</li>"
           "<li>VB6 source code reconstruction</li>"
           "<li>Symbol and type browsers</li>"
           "<li>Function call trees</li>"
           "</ul>"
           "<p>Licensed under GPL-3.0</p>")
    );
}

void MainWindow::loadFile(const QString& filePath)
{
    // TODO: Implement file loading
    QMessageBox::information(
        this,
        tr("Load File"),
        tr("Loading: %1\n\nFile loading not yet implemented.").arg(filePath)
    );
}
