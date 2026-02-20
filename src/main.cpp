#include <QApplication>
#include <QMessageBox>
#include "ui/MainWindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    
    // Set application metadata
    QApplication::setApplicationName("VBDecompiler");
    QApplication::setApplicationVersion("1.0.0");
    QApplication::setOrganizationName("VBDecompiler Project");
    
    // Create and show main window
    MainWindow window;
    window.show();
    
    return app.exec();
}
