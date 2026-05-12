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
        updateFunctionList();
        updateSectionsList();
        updateStringsList();
        updateBinaryInfo();
        updateStatusBar("Analysis complete");
        QMessageBox::information(this, "Analysis Complete", "Binary analysis completed successfully.");
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

    // TODO: Use C API to get binary info
    QString info = "Binary Information\n";
    info += "==================\n\n";
    info += "File: " + currentFilePath + "\n";
    info += "Type: PE Executable\n";
    // Add more info as available from C API

    ui->binaryInfoView->setPlainText(info);
}

void MainWindow::updateFunctionList() {
    if (!ctx) return;

    ui->functionList->clear();

    // TODO: Use C API to get function list
    // For now, placeholder
    updateStatusBar("Updated function list");
}

void MainWindow::updateSectionsList() {
    if (!ctx) return;

    ui->sectionsList->clear();

    // TODO: Use C API to get sections
    updateStatusBar("Updated sections list");
}

void MainWindow::updateStringsList() {
    if (!ctx) return;

    ui->stringsList->clear();

    // TODO: Use C API to get strings
    updateStatusBar("Updated strings list");
}

void MainWindow::showDisassemblyAt(uint32_t address) {
    if (!ctx) return;

    // TODO: Use C API to get disassembly
    QString disasm = QString("Disassembly at 0x%1\n").arg(address, 8, 16, QChar('0'));
    disasm += "====================\n\n";
    disasm += "; Function code here\n";

    ui->disassemblyView->setPlainText(disasm);
    updateStatusBar(QString("Showing disassembly at 0x%1").arg(address, 8, 16, QChar('0')));
}

void MainWindow::showDecompilerOutput(uint32_t address) {
    if (!ctx) return;

    // TODO: Use C API to get decompiled output
    QString output = QString("Decompiled function at 0x%1\n").arg(address, 8, 16, QChar('0'));
    output += "============================\n\n";
    output += "' VB6 pseudo-code here\n";

    ui->decompilerView->setPlainText(output);
    ui->rightTabs->setCurrentWidget(ui->decompilerTab);
    updateStatusBar(QString("Decompiled function at 0x%1").arg(address, 8, 16, QChar('0')));
}

void MainWindow::showXRefsFor(uint32_t address) {
    if (!ctx) return;

    ui->xrefsList->clear();

    // TODO: Use C API to get cross-references
    updateStatusBar(QString("Showing xrefs for 0x%1").arg(address, 8, 16, QChar('0')));
}

void MainWindow::showHexViewAt(uint32_t address) {
    if (!ctx) return;

    // TODO: Use C API to get raw bytes and format as hex dump
    QString hexDump = QString("Hex view at 0x%1\n").arg(address, 8, 16, QChar('0'));
    hexDump += "===================\n\n";

    ui->hexView->setPlainText(hexDump);
    updateStatusBar(QString("Showing hex at 0x%1").arg(address, 8, 16, QChar('0')));
}

void MainWindow::updateStatusBar(const QString &message) {
    statusBar()->showMessage(message);
}
