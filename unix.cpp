#include "system.h"
#include "settings.h"
#include "quazip/quazip/JlCompress.h"
#include <QDir>
#include <QDebug>
#include <QCoreApplication>
#include <QProcess>

namespace Sys {
QString archiveName(void)
{
    return "linux64.zip";
}

QString defaultInstallPath(void)
{
    return QDir::homePath() + "/.local/share/Unvanquished";
}

QString executableName(void)
{
    return "daemon";
}

bool install(void)
{
    // Set up menu and protocol handler
    Settings settings;
    QFile desktopFile(":resources/unvanquished.desktop");
    if (!desktopFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }
    QString desktopStr = QString(desktopFile.readAll().data())
        .arg(settings.installPath());
    QFile outputFile(QDir::homePath() + "/.local/share/applications/unvanquished.desktop");
    if (!outputFile.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        desktopFile.close();
        return false;
    }
    outputFile.write(desktopStr.toUtf8().constData(), desktopStr.size());
    outputFile.close();

    // install icon
    QString iconDir = QDir::homePath() + "/.local/share/icons/hicolor/128x128/apps/";
    QDir dir(iconDir);
    if (!dir.exists()) {
        if (!dir.mkpath(dir.path())) {
            return false;
        }
    }
    QFile::copy(":resources/unvanquished.png",
                iconDir + "unvanquished.png");
    return true;
}

bool updateUpdater(const QString& updaterArchive)
{
    QString current = QCoreApplication::applicationFilePath();
    QString backup = current + ".bak";
    QFile backupUpdater(backup);
    if (backupUpdater.exists()) {
        if (!backupUpdater.remove()) {
            qDebug() << "Could not remove backup updater. Aboring autoupdate.";
            return false;
        }
    }
    if (!QFile::rename(current, backup)) {
        qDebug() << "Could not move " << current << " to " << backup;
        return false;
    }
    QDir destination(current);
    if (!destination.cdUp()) {
        qDebug() << "Unexpected destination";
        return false;
    }
    // Only expect a single executable.
    auto out = JlCompress::extractDir(updaterArchive, destination.absolutePath());
    if (out.size() < 1) {
        qDebug() << "Error extracting update.";
        return false;
    }
    if (out.size() != 1) {
        qDebug() << "Invalid update archive.";
        return false;
    }

    if (out[0] != current) {
        if (!QFile::rename(out[0], current)) {
            qDebug() << "Error renaming new updater to previous file name.";
            return false;
        }
    }

    if (!QProcess::startDetached(current)) {
        qDebug() << "Error starting " << current;
        return false;
    }
    QCoreApplication::quit();
    return true;
}

QString updaterArchiveName(void)
{
    return "UnvUpdaterLinux.zip";
}

std::string getCertStore()
{
    // From Go: https://golang.org/src/crypto/x509/root_linux.go
    static QStringList CERT_LOCATIONS = {
        "/etc/ssl/certs/ca-certificates.crt",                // Debian/Ubuntu/Gentoo etc.
        "/etc/pki/tls/certs/ca-bundle.crt",                  // Fedora/RHEL 6
        "/etc/ssl/ca-bundle.pem",                            // OpenSUSE
        "/etc/pki/tls/cacert.pem",                           // OpenELEC
        "/etc/pki/ca-trust/extracted/pem/tls-ca-bundle.pem", // CentOS/RHEL 7
    };

    for (const QString& path : CERT_LOCATIONS) {
        QFile file(path);
        if (file.exists()) return path.toStdString();
    }
    return "";
}

}  // namespace Sys
