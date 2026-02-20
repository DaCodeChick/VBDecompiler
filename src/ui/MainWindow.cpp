// VBDecompiler - Visual Basic Decompiler
// Copyright (c) 2026 VBDecompiler Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "MainWindow.h"
#include "ui_MainWindow.h"
#include "../core/pe/PEFile.h"
#include "../core/vb/VBFile.h"
#include "../core/disasm/pcode/PCodeDisassembler.h"
#include "../core/ir/PCodeLifter.h"
#include "../core/decompiler/Decompiler.h"
#include <QFileDialog>
#include <QMessageBox>
#include <filesystem>
#include <sstream>

using namespace VBDecompiler;

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
    ui->codeEditor->clear();
    statusBar()->showMessage(tr("Loading: %1").arg(filePath));
    
    try {
        // Step 1: Parse PE file
        auto peFile = std::make_unique<PEFile>(std::filesystem::path(filePath.toStdString()));
        if (!peFile->parse()) {
            QMessageBox::critical(
                this,
                tr("PE Parse Error"),
                tr("Failed to parse PE file:\n%1").arg(QString::fromStdString(peFile->getLastError()))
            );
            statusBar()->showMessage(tr("Failed to load file"), 5000);
            return;
        }
        
        // Step 2: Parse VB structures
        auto vbFile = std::make_unique<VBFile>(std::move(peFile));
        if (!vbFile->parse()) {
            QMessageBox::critical(
                this,
                tr("VB Parse Error"),
                tr("Failed to parse VB structures:\n%1").arg(QString::fromStdString(vbFile->getLastError()))
            );
            statusBar()->showMessage(tr("Not a valid VB file"), 5000);
            return;
        }
        
        // Check if P-Code or native
        if (!vbFile->isPCode()) {
            QMessageBox::warning(
                this,
                tr("Native Code"),
                tr("This executable contains native x86 code.\n"
                   "Native code decompilation is partially implemented (70%).\n"
                   "Only P-Code executables are fully supported.")
            );
            statusBar()->showMessage(tr("Native code not fully supported"), 5000);
            return;
        }
        
        // Display project info
        std::ostringstream output;
        output << "' VBDecompiler - Decompiled from: " << filePath.toStdString() << "\n";
        output << "' Project: " << vbFile->getProjectName() << "\n";
        output << "' P-Code: Yes\n";
        output << "' Objects: " << vbFile->getObjectCount() << "\n";
        output << "'\n\n";
        
        // Step 3: Decompile each object
        PCodeDisassembler disassembler;
        PCodeLifter lifter;
        Decompiler decompiler;
        
        bool anyMethodsDecompiled = false;
        
        for (size_t objIdx = 0; objIdx < vbFile->getObjects().size(); ++objIdx) {
            const auto& obj = vbFile->getObjects()[objIdx];
            
            output << "' ========================================\n";
            output << "' Object: " << obj.name << "\n";
            if (obj.isForm()) {
                output << "' Type: Form\n";
            } else if (obj.isModule()) {
                output << "' Type: Module\n";
            } else if (obj.isClass()) {
                output << "' Type: Class\n";
            }
            
            if (!obj.info) {
                output << "' No method info available\n\n";
                continue;
            }
            
            output << "' Methods: " << obj.info->wMethodCount << "\n";
            output << "' ========================================\n\n";
            
            // Decompile each method in the object
            for (size_t methodIdx = 0; methodIdx < obj.methodNames.size(); ++methodIdx) {
                const auto& methodName = obj.methodNames[methodIdx];
                
                // Extract P-Code bytes
                auto pcodeBytes = vbFile->getPCodeForMethod(static_cast<uint32_t>(objIdx), 
                                                            static_cast<uint32_t>(methodIdx));
                
                if (pcodeBytes.empty()) {
                    output << "' Method: " << methodName << " (no P-Code)\n\n";
                    continue;
                }
                
                // Disassemble P-Code
                std::span<const uint8_t> pcodeSpan(pcodeBytes.data(), pcodeBytes.size());
                auto instructions = disassembler.disassembleProcedure(pcodeSpan, 0, 0);
                
                if (instructions.empty()) {
                    output << "' Method: " << methodName << " (disassembly failed)\n\n";
                    continue;
                }
                
                // Lift to IR
                auto irFunction = lifter.lift(instructions, methodName, 0);
                if (!irFunction) {
                    output << "' Method: " << methodName << " (IR lift failed)\n\n";
                    continue;
                }
                
                // Decompile to VB6
                std::string vbCode = decompiler.decompile(*irFunction);
                output << vbCode << "\n\n";
                
                anyMethodsDecompiled = true;
            }
        }
        
        if (!anyMethodsDecompiled) {
            output << "' No methods could be decompiled.\n";
            output << "' This might be due to:\n";
            output << "'   - Unsupported VB version\n";
            output << "'   - Corrupted or packed executable\n";
            output << "'   - Native code (not P-Code)\n";
        }
        
        // Display results
        ui->codeEditor->setPlainText(QString::fromStdString(output.str()));
        statusBar()->showMessage(
            tr("Successfully decompiled %1 (%2 objects)")
                .arg(filePath)
                .arg(vbFile->getObjectCount()), 
            10000
        );
        
    } catch (const std::exception& e) {
        QMessageBox::critical(
            this,
            tr("Decompilation Error"),
            tr("An error occurred during decompilation:\n%1").arg(e.what())
        );
        statusBar()->showMessage(tr("Decompilation failed"), 5000);
    }
}
