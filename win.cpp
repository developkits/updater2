#define WINVER 0x0600
#define _WIN32_WINNT 0x0600
#include "shlobj.h"
#include "windows.h"
#include "winnls.h"
#include "shobjidl.h"
#include "objbase.h"
#include "objidl.h"
#include "shlguid.h"

#include "system.h"
#include "settings.h"
#include "quazip/quazip/JlCompress.h"
#include <QSettings>
#include <QDir>
#include <QDebug>
#include <QCoreApplication>
#include <QProcess>


namespace {
void setRegistryKey(const QString& key,
                    const QString& name,
                    const QString& value)
{
    QSettings registry(key, QSettings::NativeFormat);
    registry.setValue(name, value);
}

// Create a Shortcut. Code from https://msdn.microsoft.com/en-us/library/bb776891(VS.85).aspx
bool CreateLink(const QString& sourcePath, const QString& workingDir,
                const QString& linkPath, QString& linkName)
{
    HRESULT hres;
    IShellLink* psl;


    // Get a pointer to the IShellLink interface. It is assumed that CoInitialize
    // has already been called.
    hres = CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_IShellLink, (LPVOID*)&psl);
    if (SUCCEEDED(hres))
    {
        IPersistFile* ppf;

        // Set the path to the shortcut target and add the description.
        psl->SetPath(sourcePath.toStdWString().c_str());
        psl->SetWorkingDirectory(workingDir.toStdWString().c_str());
        psl->SetDescription(linkName.toStdWString().c_str());

        // Query IShellLink for the IPersistFile interface, used for saving the
        // shortcut in persistent storage.
        hres = psl->QueryInterface(IID_IPersistFile, (LPVOID*)&ppf);

        if (SUCCEEDED(hres))
        {
            // Save the link by calling IPersistFile::Save.
            hres = ppf->Save(linkPath.toStdWString().c_str(), TRUE);
            ppf->Release();
        }
        psl->Release();
    }
    return SUCCEEDED(hres);
}
}  // namespace
namespace Sys {
bool IsWow64()
{
    BOOL bIsWow64 = FALSE;

    typedef BOOL (APIENTRY *LPFN_ISWOW64PROCESS)
    (HANDLE, PBOOL);

    LPFN_ISWOW64PROCESS fnIsWow64Process;

    HMODULE module = GetModuleHandle(L"kernel32");
    const char funcName[] = "IsWow64Process";
    fnIsWow64Process = (LPFN_ISWOW64PROCESS)
    GetProcAddress(module, funcName);

    if(NULL != fnIsWow64Process)
    {
        if (!fnIsWow64Process(GetCurrentProcess(),
            &bIsWow64))
            return false;
    }
    return bIsWow64 != FALSE;
}

QString archiveName(void)
{
    if (IsWow64()) {
        return "win64.zip";
    } else {
        return "win32.zip";
    }
}

QString defaultInstallPath(void)
{
    static const char* PROGRAM_FILES_VAR = "programfiles";
    static const char* PROGRAM_FILES_WOW64_VAR = "PROGRAMW6432";
    QString installPath = qgetenv(IsWow64() ? PROGRAM_FILES_WOW64_VAR
                                            : PROGRAM_FILES_VAR);
    return installPath + "\\Unvanquished";
}

QString executableName(void)
{
    return "daemon.exe";
}

bool install(void)
{
    Settings settings;
    QString installPath = settings.installPath();

    // Create unv:// protocol handler
    setRegistryKey("HKEY_CLASSES_ROOT\\unv", "Default", "URL: Unvanquished Protocol");
    setRegistryKey("HKEY_CLASSES_ROOT\\unv\\DefaultIcon", "Default",
                   installPath + "\\daemon.exe,1");
    setRegistryKey("HKEY_CLASSES_ROOT\\unv", "URL Protocol", "");
    setRegistryKey("HKEY_CLASSES_ROOT\\unv\\shell\\open\\command", "Default",
                   installPath + "\\daemon.exe -pakpath " + installPath +
                        " +connect \"%1\"");

    // Create a start menu shortcut
    // By default, install it to the users's start menu, unless they are instaling
    // the game globally.
    auto startMenuTye = FOLDERID_Programs;
    if (installPath.contains("Program Files", Qt::CaseInsensitive)) {
        startMenuTye = FOLDERID_CommonPrograms;
    }
    PWSTR path = nullptr;
    auto ret = SHGetKnownFolderPath(startMenuTye, 0, nullptr, &path);
    // TODO: Bubble up error if this call fails.
    if (!SUCCEEDED(ret)) {
        CoTaskMemFree(path);
        return true;
    }
    QDir dir(QString::fromStdWString(std::wstring(path)));
    dir.mkdir("Unvanquished");
    dir.setPath(dir.path() + "\\Unvanquished");
    QString linkName = "Unvanquished";
    CreateLink(installPath + "\\daemon.exe", installPath, dir.path() + "\\Unvanquished.lnk", linkName);
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
    if (out.size() != 1 || !out[0].endsWith(".exe", Qt::CaseInsensitive)) {
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
    return "UnvUpdaterWin.zip";
}

std::string getCertStore()
{
    return "";  // Not used on windows.
}


}  // namespace Sys
