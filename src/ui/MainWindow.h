#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onOpenFile();
    void onAbout();

private:
    Ui::MainWindow *ui;
    
    void setupConnections();
    void loadFile(const QString& filePath);
};

#endif // MAINWINDOW_H
