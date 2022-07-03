/*
 * Copyright 2022 arthomnix
 *
 * This source is subject to the Microsoft Public License (MS-PL).
 * Please see the COPYING.md file for more information.
 */

#include <QFileDialog>
#include <BuildConfig.h>
#include "CreateShortcutDialog.h"
#include "ui_CreateShortcutDialog.h"
#include "Application.h"
#include "minecraft/auth/AccountList.h"
#include "icons/IconList.h"

#ifdef Q_OS_WIN
#include <shobjidl.h>
#include <shlguid.h>
#endif

CreateShortcutDialog::CreateShortcutDialog(QWidget *parent, InstancePtr instance)
    :QDialog(parent), ui(new Ui::CreateShortcutDialog), m_instance(instance)
{
    ui->setupUi(this);

    QStringList accountNameList;
    auto accounts = APPLICATION->accounts();

    for (int i = 0; i < accounts->count(); i++)
    {
        accountNameList.append(accounts->at(i)->profileName());
    }

    ui->profileComboBox->addItems(accountNameList);

    if (accounts->defaultAccount())
    {
        ui->profileComboBox->setCurrentText(accounts->defaultAccount()->profileName());
    }

#if defined(Q_OS_UNIX) && !defined(Q_OS_LINUX)
    ui->createScriptCheckBox->setEnabled(false);
    ui->createScriptCheckBox->setChecked(true);
#endif

    updateDialogState();
}

CreateShortcutDialog::~CreateShortcutDialog()
{
    delete ui;
}

void CreateShortcutDialog::on_shortcutPathBrowse_clicked()
{
    QString linkExtension;
#ifdef Q_OS_UNIX
    linkExtension = ui->createScriptCheckBox->isChecked() ? "sh" : "desktop";
#endif
#ifdef Q_OS_WIN
    linkExtension = ui->createScriptCheckBox->isChecked() ? "bat" : "lnk";
#endif
    QFileDialog fileDialog(this, tr("Select shortcut path"), QStandardPaths::writableLocation(QStandardPaths::DesktopLocation));
    fileDialog.setDefaultSuffix(linkExtension);
    fileDialog.setAcceptMode(QFileDialog::AcceptOpen);
    fileDialog.setFileMode(QFileDialog::AnyFile);
    fileDialog.selectFile(m_instance->name() + " - " + BuildConfig.LAUNCHER_DISPLAYNAME + "." + linkExtension);
    if (fileDialog.exec())
    {
        ui->shortcutPath->setText(fileDialog.selectedFiles().at(0));
    }
    updateDialogState();
}

void CreateShortcutDialog::accept()
{
    createShortcut();
    QDialog::accept();
}


void CreateShortcutDialog::updateDialogState()
{

    ui->buttonBox->button(QDialogButtonBox::StandardButton::Ok)->setEnabled(
            !ui->shortcutPath->text().isEmpty()
            && (!ui->joinServerCheckBox->isChecked() || !ui->joinServer->text().isEmpty())
            && (!ui->offlineUsernameCheckBox->isChecked() || !ui->offlineUsername->text().isEmpty())
            && (!ui->useProfileCheckBox->isChecked() || !ui->profileComboBox->currentText().isEmpty())
    );
    ui->joinServer->setEnabled(ui->joinServerCheckBox->isChecked());
    ui->profileComboBox->setEnabled(ui->useProfileCheckBox->isChecked());
    ui->offlineUsernameCheckBox->setEnabled(ui->launchOfflineCheckBox->isChecked());
    ui->offlineUsername->setEnabled(ui->launchOfflineCheckBox->isChecked() && ui->offlineUsernameCheckBox->isChecked());
    if (!ui->launchOfflineCheckBox->isChecked())
    {
        ui->offlineUsernameCheckBox->setChecked(false);
    }
}

QString CreateShortcutDialog::getLaunchCommand()
{
    return QDir::toNativeSeparators(QCoreApplication::applicationFilePath())
        + getLaunchArgs();
}

QString CreateShortcutDialog::getLaunchArgs()
{
    return " -l " + m_instance->id()
           + (ui->joinServerCheckBox->isChecked() ? " -s " + ui->joinServer->text() : "")
           + (ui->useProfileCheckBox->isChecked() ? " -a " + ui->profileComboBox->currentText() : "")
           + (ui->launchOfflineCheckBox->isChecked() ? " -o" : "")
           + (ui->offlineUsernameCheckBox->isChecked() ? " -n " + ui->offlineUsername->text() : "");
}

void CreateShortcutDialog::createShortcut()
{
#ifdef Q_OS_WIN
    if (ui->createScriptCheckBox->isChecked()) // on windows, creating .lnk shortcuts requires specific win32 api stuff
                                               // rather than just writing a text file
    {
#endif
        QString shortcutText;
#ifdef Q_OS_UNIX
        // Unix shell script
        if (ui->createScriptCheckBox->isChecked())
        {
            shortcutText = "#!/bin/sh\n"
                    + "cd " + QCoreApplication::applicationDirPath() + "\n"
                    + getLaunchCommand() + " &\n";
        } else
            // freedesktop.org desktop entry
        {
            // save the launcher icon to a file so we can use it in the shortcut
            if (!QFileInfo::exists(QCoreApplication::applicationDirPath() + "/icons/shortcut-icon.png"))
            {
                QPixmap iconPixmap = QIcon(":/logo.svg").pixmap(64, 64);
                iconPixmap.save(QCoreApplication::applicationDirPath() + "/icons/shortcut-icon.png");
            }

            shortcutText = "[Desktop Entry]\n"
                           "Type=Application\n"
                           "Name=" + m_instance->name() + " - " + BuildConfig.LAUNCHER_DISPLAYNAME + "\n"
                           + "Exec=" + getLaunchCommand() + "\n"
                           + "Path=" + QCoreApplication::applicationDirPath() + "\n"
                           + "Icon=" + QCoreApplication::applicationDirPath() + "/icons/shortcut-icon.png\n";

        }
#endif
#ifdef Q_OS_WIN
        // Windows batch script implementation
        shortcutText = "@ECHO OFF\r\n"
                       "CD " + QDir::toNativeSeparators(QCoreApplication::applicationDirPath()) + "\r\n"
                       "START /B " + getLaunchCommand() + "\r\n";
#endif
        QFile shortcutFile(ui->shortcutPath->text());
        if (shortcutFile.open(QIODevice::WriteOnly))
        {
            QTextStream stream(&shortcutFile);
            stream << shortcutText;
            shortcutFile.setPermissions(QFile::ReadOwner | QFile::ReadGroup | QFile::ReadOther
                                        | QFile::WriteOwner | QFile::ExeOwner | QFile::ExeGroup);
            shortcutFile.close();
        }
#ifdef Q_OS_WIN
    }
    else
    {
        if (!QFileInfo::exists(QCoreApplication::applicationDirPath() + "/icons/shortcut-icon.ico"))
        {
            QPixmap iconPixmap = QIcon(":/logo.svg").pixmap(64, 64);
            iconPixmap.save(QCoreApplication::applicationDirPath() + "/icons/shortcut-icon.ico");
        }
        
        createWindowsLink(QDir::toNativeSeparators(QCoreApplication::applicationFilePath()).toStdString().c_str(),
                          QDir::toNativeSeparators(QCoreApplication::applicationDirPath()).toStdString().c_str(),
                          getLaunchArgs().toStdString().c_str(),
                          ui->shortcutPath->text().toStdString().c_str(),
                          (m_instance->name() + " - " + BuildConfig.LAUNCHER_DISPLAYNAME).toStdString().c_str(),
                          QDir::toNativeSeparators(QCoreApplication::applicationDirPath() + "/icons/shortcut-icon.ico").toStdString().c_str()
                          );
    }
#endif
}

#ifdef Q_OS_WIN
void CreateShortcutDialog::createWindowsLink(LPCSTR target, LPCSTR workingDir, LPCSTR args, LPCSTR filename,
                                             LPCSTR desc, LPCSTR iconPath)
{
    HRESULT result;
    IShellLink *link;

    CoInitialize(nullptr);
    result = CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_IShellLink, (LPVOID *) &link);
    if (SUCCEEDED(result))
    {
        IPersistFile *file;

        link->SetPath(target);
        link->SetWorkingDirectory(workingDir);
        link->SetArguments(args);
        link->SetDescription(desc);
        link->SetIconLocation(iconPath, 0);

        result = link->QueryInterface(IID_IPersistFile, (LPVOID *) &file);

        if (SUCCEEDED(result))
        {
            WCHAR path[MAX_PATH];
            MultiByteToWideChar(CP_ACP, 0, filename, -1, path, MAX_PATH);

            file->Save(path, TRUE);
            file->Release();
        }
        link->Release();
    }
    CoUninitialize();
}
#endif