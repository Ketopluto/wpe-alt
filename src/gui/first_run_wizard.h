#pragma once

#include <QDialog>
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QProcess>
#include <QCoreApplication>
#include <QStandardPaths>
#include <QMessageBox>
#include <QDir>
#include <QFile>
#include <QTextStream>

class FirstRunWizard : public QDialog {
    Q_OBJECT
public:
    explicit FirstRunWizard(QWidget* parent = nullptr) : QDialog(parent) {
        setWindowTitle("Welcome to Wallpaper Studio");
        setMinimumSize(500, 300);
        setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

        auto* layout = new QVBoxLayout(this);
        layout->setSpacing(20);
        layout->setContentsMargins(30, 30, 30, 30);

        auto* title = new QLabel("Welcome to Wallpaper Studio!", this);
        QFont titleFont = title->font();
        titleFont.setPointSize(16);
        titleFont.setBold(true);
        title->setFont(titleFont);
        title->setAlignment(Qt::AlignCenter);

        auto* description = new QLabel(
            "This is a lightweight, open-source alternative to Wallpaper Engine.\n\n"
            "It is designed to run silently in your system tray and provide beautiful dynamic "
            "wallpapers without ever requiring Administrator privileges, making it perfect for "
            "restricted work environments.\n\n"
            "To get started, you can create a shortcut on your Desktop.", this);
        description->setWordWrap(true);
        description->setAlignment(Qt::AlignLeft | Qt::AlignTop);

        auto* createShortcutBtn = new QPushButton("Create Desktop Shortcut", this);
        createShortcutBtn->setMinimumHeight(40);
        connect(createShortcutBtn, &QPushButton::clicked, this, &FirstRunWizard::createShortcut);

        auto* continueBtn = new QPushButton("Continue to Application", this);
        continueBtn->setMinimumHeight(40);
        connect(continueBtn, &QPushButton::clicked, this, &QDialog::accept);

        layout->addWidget(title);
        layout->addWidget(description);
        layout->addStretch();
        layout->addWidget(createShortcutBtn);
        layout->addWidget(continueBtn);
    }

private:
    void createShortcut() {
        QString exePath = QCoreApplication::applicationFilePath();
        QString desktopPath = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
        QString linkPath = QDir(desktopPath).filePath("Wallpaper Studio.lnk");

        // Write a temporary PowerShell script to create the shortcut securely without admin rights
        QString psScriptPath = QDir::temp().filePath("create_wps_shortcut.ps1");
        QFile file(psScriptPath);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&file);
            out << "$WshShell = New-Object -comObject WScript.Shell\n";
            out << "$Shortcut = $WshShell.CreateShortcut(\"" << linkPath.replace("/", "\\") << "\")\n";
            out << "$Shortcut.TargetPath = \"" << exePath.replace("/", "\\") << "\"\n";
            out << "$Shortcut.WorkingDirectory = \"" << QCoreApplication::applicationDirPath().replace("/", "\\") << "\"\n";
            out << "$Shortcut.Save()\n";
            file.close();

            QProcess::execute("powershell", {"-ExecutionPolicy", "Bypass", "-File", psScriptPath});
            file.remove();

            QMessageBox::information(this, "Success", "Shortcut created on your Desktop!");
        } else {
            QMessageBox::warning(this, "Error", "Failed to create the shortcut script.");
        }
    }
};
