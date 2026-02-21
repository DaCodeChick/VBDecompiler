// VBDecompiler - Visual Basic Decompiler
// Copyright (c) 2026 VBDecompiler Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "MainWindow.h"
#include "ui_MainWindow.h"
#include "../../include/vbdecompiler_ffi.h"
#include <QFileDialog>
#include <QMessageBox>
#include <sstream>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , decompiler(nullptr)
{
    ui->setupUi(this);
    setupConnections();
    
    // Initialize Rust decompiler
    decompiler = vbdecompiler_new();
    
    setWindowTitle("VBDecompiler - Visual Basic 5/6 Decompiler");
    resize(1280, 800);
}

MainWindow::~MainWindow()
{
    if (decompiler) {
        vbdecompiler_free(decompiler);
    }
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
           "<p>Built with Rust core and Qt 6 GUI.</p>"
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
    ui->codeEditor->clear();
    statusBar()->showMessage(tr("Loading: %1").arg(filePath));
    
    if (!decompiler) {
        QMessageBox::critical(
            this,
            tr("Error"),
            tr("Decompiler not initialized")
        );
        return;
    }
    
    // Call Rust decompiler via FFI
    VBDecompilationResult* result = nullptr;
    std::string path = filePath.toStdString();
    int status = vbdecompiler_decompile_file(decompiler, path.c_str(), &result);
    
    if (status != 0) {
        // Error occurred
        QString errorMsg;
        switch (status) {
            case -1:
                errorMsg = tr("Invalid argument");
                break;
            case -2:
                errorMsg = tr("Invalid UTF-8 in path");
                break;
            case -3:
                errorMsg = tr("Decompilation failed. This might be due to:\n"
                             "  - Unsupported VB version\n"
                             "  - Corrupted or packed executable\n"
                             "  - Native code (not P-Code)\n"
                             "  - Invalid VB5/6 executable");
                break;
            default:
                errorMsg = tr("Unknown error (code: %1)").arg(status);
                break;
        }
        
        QMessageBox::critical(
            this,
            tr("Decompilation Error"),
            errorMsg
        );
        statusBar()->showMessage(tr("Decompilation failed"), 5000);
        return;
    }
    
    // Success - display results
    if (result) {
        std::ostringstream output;
        
        // Header
        output << "' VBDecompiler - Decompiled from: " << path << "\n";
        output << "' Project: " << result->project_name << "\n";
        output << "' P-Code: " << (result->is_pcode ? "Yes" : "No") << "\n";
        output << "' Objects: " << result->object_count << "\n";
        output << "' Methods: " << result->method_count << "\n";
        output << "'\n\n";
        
        // Generated VB6 code
        output << result->vb6_code;
        
        // Display in editor
        ui->codeEditor->setPlainText(QString::fromStdString(output.str()));
        
        statusBar()->showMessage(
            tr("Successfully decompiled %1 (%2 objects, %3 methods)")
                .arg(filePath)
                .arg(result->object_count)
                .arg(result->method_count),
            10000
        );
        
        // Free the result
        vbdecompiler_free_result(result);
    }
}
