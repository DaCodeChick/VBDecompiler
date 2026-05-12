#include "MainWindow.h"
#include "ui_MainWindow.h"
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QInputDialog>
#include <QTextCursor>
#include <QScrollBar>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    setupConnections();
    updateStatusBar("Ready");
}

MainWindow::~MainWindow() {
    if (ctx) {
        vbdecomp_close(ctx);
    }
    delete ui;
}

void MainWindow::setupConnections() {
    // File menu
    connect(ui->actionOpen, &QAction::triggered, this, &MainWindow::onActionOpen);
    connect(ui->actionClose, &QAction::triggered, this, &MainWindow::onActionClose);
    connect(ui->actionSaveProject, &QAction::triggered, this, &MainWindow::onActionSaveProject);
    connect(ui->actionLoadProject, &QAction::triggered, this, &MainWindow::onActionLoadProject);
    connect(ui->actionExit, &QAction::triggered, this, &MainWindow::onActionExit);

    // Analysis menu
    connect(ui->actionAnalyze, &QAction::triggered, this, &MainWindow::onActionAnalyze);
    connect(ui->actionDecompileFunction, &QAction::triggered, this, &MainWindow::onActionDecompileFunction);
    connect(ui->actionFindFunction, &QAction::triggered, this, &MainWindow::onActionFindFunction);
    connect(ui->actionGotoAddress, &QAction::triggered, this, &MainWindow::onActionGotoAddress);

    // View menu
    connect(ui->actionShowFunctions, &QAction::toggled, this, &MainWindow::onActionShowFunctions);
    connect(ui->actionShowStrings, &QAction::toggled, this, &MainWindow::onActionShowStrings);
    connect(ui->actionShowHex, &QAction::toggled, this, &MainWindow::onActionShowHex);
    connect(ui->actionShowGraph, &QAction::toggled, this, &MainWindow::onActionShowGraph);

    // Help menu
    connect(ui->actionAbout, &QAction::triggered, this, &MainWindow::onActionAbout);
    connect(ui->actionDocumentation, &QAction::triggered, this, &MainWindow::onActionDocumentation);

    // Widget interactions
    connect(ui->functionList, &QTreeWidget::itemClicked, this, &MainWindow::onFunctionListItemClicked);
    connect(ui->functionFilter, &QLineEdit::textChanged, this, &MainWindow::onFunctionFilterTextChanged);
    connect(ui->stringFilter, &QLineEdit::textChanged, this, &MainWindow::onStringFilterTextChanged);
    connect(ui->stringsList, &QListWidget::itemClicked, this, &MainWindow::onStringsListItemClicked);
    connect(ui->xrefsList, &QTreeWidget::itemClicked, this, &MainWindow::onXrefsListItemClicked);
}

// File menu slots
void MainWindow::onActionOpen() {
    QString filePath = QFileDialog::getOpenFileName(
        this,
        "Open Binary File",
        QString(),
        "Executable Files (*.exe *.dll *.ocx);;All Files (*)"
    );

    if (!filePath.isEmpty()) {
        loadBinaryFile(filePath);
    }
}

void MainWindow::onActionClose() {
    closeBinaryFile();
}

void MainWindow::onActionSaveProject() {
    if (!fileLoaded) {
        QMessageBox::warning(this, "No File Loaded", "Please load a binary file first.");
        return;
    }

    QString projectPath = QFileDialog::getSaveFileName(
        this,
        "Save Project",
        QString(),
        "VBDecompiler Project (*.vbdp);;All Files (*)"
    );

    if (!projectPath.isEmpty()) {
        // TODO: Implement project saving using SQLite DB
        updateStatusBar("Project saved: " + projectPath);
    }
}

void MainWindow::onActionLoadProject() {
    QString projectPath = QFileDialog::getOpenFileName(
        this,
        "Load Project",
        QString(),
        "VBDecompiler Project (*.vbdp);;All Files (*)"
    );

    if (!projectPath.isEmpty()) {
        // TODO: Implement project loading from SQLite DB
        updateStatusBar("Project loaded: " + projectPath);
    }
}

void MainWindow::onActionExit() {
    close();
}

// Analysis menu slots
void MainWindow::onActionAnalyze() {
    if (!fileLoaded || !ctx) {
        QMessageBox::warning(this, "No File Loaded", "Please load a binary file first.");
        return;
    }

    updateStatusBar("Analyzing binary...");

    // Perform analysis using C API
    vbdecomp_info_t info;
    if (vbdecomp_get_info(ctx, &info)) {
        updateBinaryInfo();
        updateSectionsList();
        updateFunctionList();
        updateStringsList();
        
        updateStatusBar("Analysis complete");
        
        QString msg = QString("Analysis complete!\n\n");
        msg += QString("VB Binary: %1\n").arg(info.is_vb ? "Yes" : "No");
        msg += QString("Sections loaded: %1\n").arg(vbdecomp_get_section_count(ctx));
        msg += QString("Functions: %1\n").arg(vbdecomp_get_function_count(ctx));
        msg += QString("Strings: %1\n").arg(vbdecomp_get_string_count(ctx));
        
        QMessageBox::information(this, "Analysis Complete", msg);
        
        // Switch to Binary Info tab to show results
        ui->rightTabs->setCurrentWidget(ui->infoTab);
    } else {
        updateStatusBar("Analysis failed");
        QMessageBox::critical(this, "Analysis Failed", "Failed to analyze the binary file.");
    }
}

void MainWindow::onActionDecompileFunction() {
    if (!fileLoaded || !ctx) {
        QMessageBox::warning(this, "No File Loaded", "Please load a binary file first.");
        return;
    }

    // Get current function from selection or ask user
    QList<QTreeWidgetItem*> selected = ui->functionList->selectedItems();
    if (selected.isEmpty()) {
        QMessageBox::information(this, "No Function Selected", "Please select a function from the function list.");
        return;
    }

    bool ok;
    uint32_t address = selected.first()->text(0).toUInt(&ok, 16);
    if (ok) {
        showDecompilerOutput(address);
    }
}

void MainWindow::onActionFindFunction() {
    if (!fileLoaded) {
        QMessageBox::warning(this, "No File Loaded", "Please load a binary file first.");
        return;
    }

    bool ok;
    QString funcName = QInputDialog::getText(
        this,
        "Find Function",
        "Function name or address:",
        QLineEdit::Normal,
        QString(),
        &ok
    );

    if (ok && !funcName.isEmpty()) {
        // TODO: Implement function search
        updateStatusBar("Searching for function: " + funcName);
    }
}

void MainWindow::onActionGotoAddress() {
    if (!fileLoaded) {
        QMessageBox::warning(this, "No File Loaded", "Please load a binary file first.");
        return;
    }

    bool ok;
    QString addrStr = QInputDialog::getText(
        this,
        "Go to Address",
        "Address (hex):",
        QLineEdit::Normal,
        "0x",
        &ok
    );

    if (ok && !addrStr.isEmpty()) {
        uint32_t address = addrStr.toUInt(&ok, 16);
        if (ok) {
            showDisassemblyAt(address);
            ui->centerTabs->setCurrentWidget(ui->disassemblyTab);
        } else {
            QMessageBox::warning(this, "Invalid Address", "Please enter a valid hexadecimal address.");
        }
    }
}

// View menu slots
void MainWindow::onActionShowFunctions(bool checked) {
    ui->leftPanel->setVisible(checked);
}

void MainWindow::onActionShowStrings(bool checked) {
    ui->leftTabs->setTabVisible(ui->leftTabs->indexOf(ui->stringsTab), checked);
}

void MainWindow::onActionShowHex(bool checked) {
    ui->centerTabs->setTabVisible(ui->centerTabs->indexOf(ui->hexTab), checked);
}

void MainWindow::onActionShowGraph(bool checked) {
    ui->centerTabs->setTabVisible(ui->centerTabs->indexOf(ui->graphTab), checked);
}

// Help menu slots
void MainWindow::onActionAbout() {
    QMessageBox::about(this, "About VBDecompiler",
        "<h2>VBDecompiler</h2>"
        "<p>Version 0.1.0</p>"
        "<p>A cross-platform VB6 decompiler with native-code and P-code support.</p>"
        "<p>Licensed under LGPL v3</p>"
    );
}

void MainWindow::onActionDocumentation() {
    QMessageBox::information(this, "Documentation", 
        "Documentation coming soon!\n\n"
        "For now, check the README.md in the project repository."
    );
}

// Widget interaction slots
void MainWindow::onFunctionListItemClicked(QTreeWidgetItem *item, int column) {
    bool ok;
    uint32_t address = item->text(0).toUInt(&ok, 16);
    if (ok) {
        showDisassemblyAt(address);
        showXRefsFor(address);
        ui->centerTabs->setCurrentWidget(ui->disassemblyTab);
    }
}

void MainWindow::onFunctionFilterTextChanged(const QString &text) {
    for (int i = 0; i < ui->functionList->topLevelItemCount(); ++i) {
        QTreeWidgetItem *item = ui->functionList->topLevelItem(i);
        bool matches = text.isEmpty() || 
                      item->text(0).contains(text, Qt::CaseInsensitive) ||
                      item->text(1).contains(text, Qt::CaseInsensitive);
        item->setHidden(!matches);
    }
}

void MainWindow::onStringFilterTextChanged(const QString &text) {
    for (int i = 0; i < ui->stringsList->count(); ++i) {
        QListWidgetItem *item = ui->stringsList->item(i);
        bool matches = text.isEmpty() || 
                      item->text().contains(text, Qt::CaseInsensitive);
        item->setHidden(!matches);
    }
}

void MainWindow::onStringsListItemClicked(QListWidgetItem *item) {
    // Parse address from string item (format: "0x12345678: string content")
    QString text = item->text();
    int colonPos = text.indexOf(':');
    if (colonPos > 0) {
        bool ok;
        uint32_t address = text.left(colonPos).toUInt(&ok, 16);
        if (ok) {
            showHexViewAt(address);
            ui->centerTabs->setCurrentWidget(ui->hexTab);
        }
    }
}

void MainWindow::onXrefsListItemClicked(QTreeWidgetItem *item, int column) {
    bool ok;
    uint32_t address = item->text(0).toUInt(&ok, 16);
    if (ok) {
        showDisassemblyAt(address);
        ui->centerTabs->setCurrentWidget(ui->disassemblyTab);
    }
}

// Helper methods
void MainWindow::loadBinaryFile(const QString &filePath) {
    closeBinaryFile();

    ctx = vbdecomp_open(filePath.toUtf8().constData());
    if (!ctx) {
        QMessageBox::critical(this, "Error", "Failed to load binary file.");
        return;
    }

    currentFilePath = filePath;
    fileLoaded = true;
    
    setWindowTitle("VBDecompiler - " + QFileInfo(filePath).fileName());
    updateStatusBar("Loaded: " + filePath);

    // Enable analysis actions
    ui->actionAnalyze->setEnabled(true);
    ui->actionClose->setEnabled(true);
    ui->actionSaveProject->setEnabled(true);
    
    // Automatically populate basic info (binary info and sections)
    updateBinaryInfo();
    updateSectionsList();
    
    // Switch to the info tab to show results
    ui->rightTabs->setCurrentWidget(ui->infoTab);
    ui->leftTabs->setCurrentWidget(ui->sectionsTab);
}

void MainWindow::closeBinaryFile() {
    if (ctx) {
        vbdecomp_close(ctx);
        ctx = nullptr;
    }

    fileLoaded = false;
    currentFilePath.clear();
    
    // Clear all views
    ui->functionList->clear();
    ui->sectionsList->clear();
    ui->stringsList->clear();
    ui->disassemblyView->clear();
    ui->hexView->clear();
    ui->decompilerView->clear();
    ui->pcodeView->clear();
    ui->xrefsList->clear();
    ui->binaryInfoView->clear();

    setWindowTitle("VBDecompiler");
    updateStatusBar("Ready");

    // Disable analysis actions
    ui->actionAnalyze->setEnabled(false);
    ui->actionClose->setEnabled(false);
    ui->actionDecompileFunction->setEnabled(false);
}

void MainWindow::updateBinaryInfo() {
    if (!ctx) return;

    vbdecomp_info_t info;
    QString infoText = "Binary Information\n";
    infoText += "==================\n\n";
    infoText += "File: " + currentFilePath + "\n\n";

    if (vbdecomp_get_info(ctx, &info)) {
        infoText += QString("VB Binary: %1\n").arg(info.is_vb ? "Yes" : "No");
        
        if (info.is_vb) {
            infoText += QString("VB Version: %1\n").arg(info.vb_version);
            
            QString compType = "Unknown";
            if (info.compilation_type == VBDECOMP_COMPILE_NATIVE) {
                compType = "Native Code";
            } else if (info.compilation_type == VBDECOMP_COMPILE_PCODE) {
                compType = "P-Code";
            }
            infoText += QString("Compilation Type: %1\n").arg(compType);
            
            QString binType = "Unknown";
            if (info.binary_type == VBDECOMP_BINARY_EXE) {
                binType = "EXE";
            } else if (info.binary_type == VBDECOMP_BINARY_DLL) {
                binType = "DLL";
            } else if (info.binary_type == VBDECOMP_BINARY_OCX) {
                binType = "OCX";
            }
            infoText += QString("Binary Type: %1\n").arg(binType);
            
            infoText += QString("Has Forms: %1\n").arg(info.has_forms ? "Yes" : "No");
            
            if (info.runtime_dll) {
                infoText += QString("Runtime DLL: %1\n").arg(info.runtime_dll);
            }
        }
        
        infoText += QString("\nEntry Point: 0x%1\n").arg(info.entry_point, 8, 16, QChar('0'));
        infoText += QString("Image Base: 0x%1\n").arg(info.image_base, 8, 16, QChar('0'));
        infoText += QString("Image Size: %1 bytes\n").arg(info.image_size);
    } else {
        infoText += "\nFailed to retrieve binary information.\n";
    }

    ui->binaryInfoView->setPlainText(infoText);
}

void MainWindow::updateFunctionList() {
    if (!ctx) return;

    ui->functionList->clear();

    size_t function_count = vbdecomp_get_function_count(ctx);
    
    for (size_t i = 0; i < function_count; i++) {
        vbdecomp_function_t func;
        if (vbdecomp_get_function(ctx, i, &func)) {
            QTreeWidgetItem *item = new QTreeWidgetItem();
            item->setText(0, QString("0x%1").arg(func.address, 8, 16, QChar('0')));
            
            QString name = func.name ? QString::fromUtf8(func.name) : QString("sub_%1").arg(func.address, 8, 16, QChar('0'));
            if (func.is_export) {
                name += " [export]";
            }
            if (func.is_thunk) {
                name += " [thunk]";
            }
            item->setText(1, name);
            item->setText(2, QString::number(func.size));
            
            ui->functionList->addTopLevelItem(item);
        }
    }
    
    ui->functionList->resizeColumnToContents(0);
    ui->functionList->resizeColumnToContents(1);
    ui->functionList->resizeColumnToContents(2);
    
    updateStatusBar(QString("Loaded %1 functions").arg(function_count));
}

void MainWindow::updateSectionsList() {
    if (!ctx) return;

    ui->sectionsList->clear();

    size_t section_count = vbdecomp_get_section_count(ctx);
    
    for (size_t i = 0; i < section_count; i++) {
        vbdecomp_section_t section;
        if (vbdecomp_get_section(ctx, i, &section)) {
            QTreeWidgetItem *item = new QTreeWidgetItem();
            item->setText(0, QString::fromUtf8(section.name));
            item->setText(1, QString("0x%1").arg(section.virtual_address, 8, 16, QChar('0')));
            item->setText(2, QString::number(section.virtual_size));
            ui->sectionsList->addTopLevelItem(item);
        }
    }
    
    ui->sectionsList->resizeColumnToContents(0);
    ui->sectionsList->resizeColumnToContents(1);
    ui->sectionsList->resizeColumnToContents(2);
    
    updateStatusBar(QString("Loaded %1 sections").arg(section_count));
}

void MainWindow::updateStringsList() {
    if (!ctx) return;

    ui->stringsList->clear();

    size_t string_count = vbdecomp_get_string_count(ctx);
    
    for (size_t i = 0; i < string_count; i++) {
        vbdecomp_string_t str;
        if (vbdecomp_get_string(ctx, i, &str)) {
            QString text = QString("0x%1: %2")
                .arg(str.address, 8, 16, QChar('0'))
                .arg(QString::fromUtf8(str.value));
            
            QListWidgetItem *item = new QListWidgetItem(text);
            ui->stringsList->addItem(item);
        }
    }
    
    updateStatusBar(QString("Loaded %1 strings").arg(string_count));
}

void MainWindow::showDisassemblyAt(uint32_t address) {
    if (!ctx) return;

    // Try to disassemble at the given address
    char* disasm_result = vbdecomp_disassemble(ctx, address, 50); // 50 instructions
    
    if (disasm_result) {
        ui->disassemblyView->setPlainText(QString::fromUtf8(disasm_result));
        vbdecomp_free_string(disasm_result);
        updateStatusBar(QString("Showing disassembly at 0x%1").arg(address, 8, 16, QChar('0')));
    } else {
        QString disasm = QString("Disassembly at 0x%1\n").arg(address, 8, 16, QChar('0'));
        disasm += "====================\n\n";
        disasm += "Unable to disassemble at this address.\n";
        disasm += "The address may be invalid or not executable code.\n";
        ui->disassemblyView->setPlainText(disasm);
        updateStatusBar("Failed to disassemble");
    }
}

void MainWindow::showDecompilerOutput(uint32_t address) {
    if (!ctx) return;

    // Try to decompile the function at the given address
    char* decompile_result = vbdecomp_decompile(ctx, address);
    
    if (decompile_result) {
        ui->decompilerView->setPlainText(QString::fromUtf8(decompile_result));
        vbdecomp_free_string(decompile_result);
        updateStatusBar(QString("Decompiled function at 0x%1").arg(address, 8, 16, QChar('0')));
    } else {
        QString output = QString("Decompiled function at 0x%1\n").arg(address, 8, 16, QChar('0'));
        output += "============================\n\n";
        output += "' Unable to decompile function.\n";
        output += "' The function may not have been analyzed yet.\n";
        output += "' Try clicking Analysis -> Analyze Binary first.\n";
        ui->decompilerView->setPlainText(output);
        updateStatusBar("Decompilation not available");
    }
    
    ui->rightTabs->setCurrentWidget(ui->decompilerTab);
}

void MainWindow::showXRefsFor(uint32_t address) {
    if (!ctx) return;

    ui->xrefsList->clear();

    // Get cross-references TO this address
    uint32_t xrefs_to[256];
    size_t xrefs_to_count = vbdecomp_get_xrefs_to(ctx, address, xrefs_to, 256);
    
    for (size_t i = 0; i < xrefs_to_count; i++) {
        QTreeWidgetItem *item = new QTreeWidgetItem();
        item->setText(0, QString("0x%1").arg(xrefs_to[i], 8, 16, QChar('0')));
        item->setText(1, QString("0x%1").arg(address, 8, 16, QChar('0')));
        item->setText(2, "Call/Jump To");
        ui->xrefsList->addTopLevelItem(item);
    }
    
    // Get cross-references FROM this address
    uint32_t xrefs_from[256];
    size_t xrefs_from_count = vbdecomp_get_xrefs_from(ctx, address, xrefs_from, 256);
    
    for (size_t i = 0; i < xrefs_from_count; i++) {
        QTreeWidgetItem *item = new QTreeWidgetItem();
        item->setText(0, QString("0x%1").arg(address, 8, 16, QChar('0')));
        item->setText(1, QString("0x%1").arg(xrefs_from[i], 8, 16, QChar('0')));
        item->setText(2, "Call/Jump From");
        ui->xrefsList->addTopLevelItem(item);
    }
    
    ui->xrefsList->resizeColumnToContents(0);
    ui->xrefsList->resizeColumnToContents(1);
    ui->xrefsList->resizeColumnToContents(2);
    
    size_t total = xrefs_to_count + xrefs_from_count;
    updateStatusBar(QString("Found %1 cross-references for 0x%2").arg(total).arg(address, 8, 16, QChar('0')));
}

void MainWindow::showHexViewAt(uint32_t address) {
    if (!ctx) return;

    // Read 512 bytes from the address
    uint8_t buffer[512];
    size_t bytes_read = vbdecomp_read_bytes(ctx, address, buffer, sizeof(buffer));
    
    if (bytes_read == 0) {
        QString hexDump = QString("Hex view at 0x%1\n").arg(address, 8, 16, QChar('0'));
        hexDump += "===================\n\n";
        hexDump += "Unable to read bytes at this address.\n";
        ui->hexView->setPlainText(hexDump);
        updateStatusBar("Failed to read memory");
        return;
    }
    
    // Format as hex dump (16 bytes per line)
    QString hexDump;
    for (size_t i = 0; i < bytes_read; i += 16) {
        // Address
        hexDump += QString("%1  ").arg(address + i, 8, 16, QChar('0'));
        
        // Hex bytes
        QString ascii;
        for (size_t j = 0; j < 16 && (i + j) < bytes_read; j++) {
            uint8_t byte = buffer[i + j];
            hexDump += QString("%1 ").arg(byte, 2, 16, QChar('0'));
            
            // ASCII representation
            if (byte >= 32 && byte <= 126) {
                ascii += QChar(byte);
            } else {
                ascii += '.';
            }
        }
        
        // Padding if less than 16 bytes
        size_t remaining = 16 - qMin(size_t(16), bytes_read - i);
        for (size_t j = 0; j < remaining; j++) {
            hexDump += "   ";
        }
        
        hexDump += " |" + ascii + "|\n";
    }
    
    ui->hexView->setPlainText(hexDump);
    updateStatusBar(QString("Showing %1 bytes at 0x%2").arg(bytes_read).arg(address, 8, 16, QChar('0')));
}

void MainWindow::updateStatusBar(const QString &message) {
    statusBar()->showMessage(message);
}
