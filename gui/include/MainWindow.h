#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QString>
#include <QTreeWidgetItem>
#include <QListWidgetItem>
#include <memory>

extern "C" {
#include "vbdecomp.h"
}

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    // File menu
    void onActionOpen();
    void onActionClose();
    void onActionSaveProject();
    void onActionLoadProject();
    void onActionExit();

    // Analysis menu
    void onActionAnalyze();
    void onActionDecompileFunction();
    void onActionFindFunction();
    void onActionGotoAddress();

    // View menu
    void onActionShowFunctions(bool checked);
    void onActionShowStrings(bool checked);
    void onActionShowHex(bool checked);
    void onActionShowGraph(bool checked);

    // Help menu
    void onActionAbout();
    void onActionDocumentation();

    // Widget interactions
    void onFunctionListItemClicked(QTreeWidgetItem *item, int column);
    void onFunctionFilterTextChanged(const QString &text);
    void onStringFilterTextChanged(const QString &text);
    void onStringsListItemClicked(QListWidgetItem *item);
    void onXrefsListItemClicked(QTreeWidgetItem *item, int column);

private:
    Ui::MainWindow *ui;
    vbdecomp_context_t *ctx = nullptr;
    QString currentFilePath;
    bool fileLoaded = false;

    // Helper methods
    void setupConnections();
    void loadBinaryFile(const QString &filePath);
    void closeBinaryFile();
    void updateBinaryInfo();
    void updateFunctionList();
    void updateSectionsList();
    void updateStringsList();
    void showDisassemblyAt(uint32_t address);
    void showDecompilerOutput(uint32_t address);
    void showXRefsFor(uint32_t address);
    void showHexViewAt(uint32_t address);
    void updateStatusBar(const QString &message);
};

#endif // MAINWINDOW_H
