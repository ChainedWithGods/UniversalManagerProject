#include "ModManagerInternal.h"

using namespace mmdetail;

QString ModManager::getSystemDriveLetter() const
{
    if (qEnvironmentVariableIsSet("SystemDrive")) {
        const QByteArray value = qgetenv("SystemDrive");
        if (!value.isEmpty()) {
            return QString::fromUtf8(value).left(1).toUpper();
        }
    }
    return QStringLiteral("C");
}

bool ModManager::isPathOnSystemDrive(const QString& path) const
{
    if (path.isEmpty()) {
        return false;
    }

    const QString cleanPath = QDir::fromNativeSeparators(QDir::cleanPath(path));
    if (cleanPath.length() < 2) {
        return false;
    }

    if (cleanPath.at(1) != QLatin1Char(':')) {
        return false;
    }
    const QChar drive = cleanPath.at(0).toUpper();
    if (drive < QLatin1Char('A') || drive > QLatin1Char('Z')) {
        return false;
    }

    return drive == getSystemDriveLetter().at(0);
}

bool ModManager::launchDetachedElevated(const QString& exePath,
                                        const QStringList& args,
                                        const QString& workingDir)
{
#ifdef Q_OS_WIN

    auto quoteArg = [](const QString& arg) -> QString {
        if (arg.isEmpty()) {
            return QStringLiteral("\"\"");
        }

        bool needsQuoting = false;
        for (const QChar c : arg) {
            if (c.isSpace() || c == QLatin1Char('\t') || c == QLatin1Char('"')) {
                needsQuoting = true;
                break;
            }
        }
        if (!needsQuoting) {
            return arg;
        }

        QString escaped;
        escaped.reserve(arg.size() + 2);
        escaped.append(QLatin1Char('"'));

        int backslashes = 0;
        for (const QChar c : arg) {
            if (c == QLatin1Char('\\')) {
                ++backslashes;
                escaped.append(c);
            } else if (c == QLatin1Char('"')) {

                for (int i = 0; i < backslashes; ++i) {
                    escaped.append(QLatin1Char('\\'));
                }
                escaped.append(QLatin1Char('\\'));
                escaped.append(QLatin1Char('"'));
                backslashes = 0;
            } else {
                backslashes = 0;
                escaped.append(c);
            }
        }

        for (int i = 0; i < backslashes; ++i) {
            escaped.append(QLatin1Char('\\'));
        }
        escaped.append(QLatin1Char('"'));
        return escaped;
    };

    QStringList quotedArgs;
    quotedArgs.reserve(args.size());
    for (const QString& a : args) {
        quotedArgs << quoteArg(a);
    }
    const QString paramStr = quotedArgs.join(QChar(L' '));

    SHELLEXECUTEINFOW execInfo{};
    execInfo.cbSize = sizeof(execInfo);
    execInfo.fMask = SEE_MASK_NOCLOSEPROCESS;
    execInfo.lpVerb = L"runas";
    execInfo.nShow = SW_SHOWNORMAL;
    execInfo.lpFile = reinterpret_cast<LPCWSTR>(QDir::toNativeSeparators(exePath).utf16());
    if (!paramStr.isEmpty()) {
        execInfo.lpParameters = reinterpret_cast<LPCWSTR>(paramStr.utf16());
    }
    if (!workingDir.isEmpty()) {
        execInfo.lpDirectory = reinterpret_cast<LPCWSTR>(QDir::toNativeSeparators(workingDir).utf16());
    }

    if (ShellExecuteExW(&execInfo)) {
        if (execInfo.hProcess) {
            CloseHandle(execInfo.hProcess);
        }
        return true;
    }

    const DWORD err = GetLastError();
    qWarning() << "launchDetachedElevated: ShellExecuteExW failed for"
               << exePath << "with error" << err;
    return false;
#else
    Q_UNUSED(exePath);
    Q_UNUSED(args);
    Q_UNUSED(workingDir);
    return false;
#endif
}

QString ModManager::launchGame() {
    QString executablePath = QUrl(m_gamePath).toLocalFile();
    if (executablePath.isEmpty()) {
        executablePath = m_gamePath;
    }
    executablePath = QDir::cleanPath(executablePath);

    if (executablePath.isEmpty()) {
        return "Game executable is not set";
    }

    QFileInfo exeInfo(executablePath);
    if (!exeInfo.exists() || !exeInfo.isFile()) {
        return "Game executable not found";
    }

    QString driveLetter = exeInfo.absolutePath().left(2).toUpper();
    if (driveLetter == "C:" && !isRunningAsAdmin()) {
        emit cDriveAdminRequired();
        return "Administrator privileges required for C: drive";
    }

    const QString workingDir = exeInfo.absolutePath();

    qint64 pid = 0;
    const bool success = QProcess::startDetached(executablePath, {}, workingDir, &pid);
    if (!success) {
        return "Failed to launch game";
    }

    m_lastLaunchedGamePid = pid;
    m_gameWindowTitle.clear();

    auto* titleTimer = new QTimer(this);
    titleTimer->setInterval(500);
    int* attempts = new int(0);
    connect(titleTimer, &QTimer::timeout, this, [this, titleTimer, attempts]() {
        m_gameWindowTitle = getGameWindowTitleFromPid(m_lastLaunchedGamePid);
        (*attempts)++;
        if (!m_gameWindowTitle.isEmpty() || *attempts >= 20) {
            titleTimer->stop();
            titleTimer->deleteLater();
            delete attempts;
        }
    });
    titleTimer->start();

    if (!m_avaloniumStatus.isEmpty() || !m_universalRuntimeLog.isEmpty()) {
        m_universalRuntimeLog.clear();
        emit universalRuntimeLogChanged();
        m_avaloniumStatus.clear();
        emit avaloniumStatusChanged();
    }

    m_runtimeLogReadPos = 0;
    m_iocpStatusRequested = false;

    return "Launching game";
}

QString ModManager::resolveToolPath(const QString& relativePath) const {
    QString base = QCoreApplication::applicationDirPath();
    QDir dir(base);

    for (int i = 0; i < 5; ++i) {
        const QString candidate = QDir::cleanPath(dir.absoluteFilePath(relativePath));
        if (QFileInfo::exists(candidate)) {
            return candidate;
        }
        dir.cdUp();
    }

    return QString();
}

QString ModManager::getGameWindowTitleFromPid(qint64 pid) {
#ifdef Q_OS_WIN
    struct EnumData {
        DWORD pid;
        QString title;
    };
    EnumData data;
    data.pid = static_cast<DWORD>(pid);

    EnumWindows([](HWND hwnd, LPARAM lParam) -> BOOL {
        auto* d = reinterpret_cast<EnumData*>(lParam);
        DWORD winPid = 0;
        GetWindowThreadProcessId(hwnd, &winPid);
        if (winPid == d->pid && IsWindowVisible(hwnd) && GetWindow(hwnd, GW_OWNER) == nullptr) {
            wchar_t buf[512] = {};
            GetWindowTextW(hwnd, buf, 512);
            if (buf[0] != L'\0') {
                d->title = QString::fromWCharArray(buf);
                return FALSE;
            }
        }
        return TRUE;
    }, reinterpret_cast<LPARAM>(&data));

    return data.title;
#else
    return QString();
#endif
}

QString ModManager::sanitizeFileName(const QString& name) {
    QString s = name;

    s.remove(QRegularExpression("[<>\"/\\|?*]"));

    s.replace(':', '-');

    s = s.simplified();

    if (s.length() > 120) {
        s = s.left(120);
    }
    return s;
}

QString ModManager::resolveRuntimeGameTitle() {
    QString gameTitle = sanitizeFileName(m_gameWindowTitle);

#ifdef Q_OS_WIN
    if (gameTitle.isEmpty() && m_lastLaunchedGamePid > 0) {
        const QString liveWindowTitle = getGameWindowTitleFromPid(m_lastLaunchedGamePid).trimmed();
        if (!liveWindowTitle.isEmpty()) {
            m_gameWindowTitle = liveWindowTitle;
            gameTitle = sanitizeFileName(liveWindowTitle);
        }
    }
#endif

    if (gameTitle.isEmpty() && !m_avaloniumStatus.isEmpty()) {
        const QRegularExpression gameLineRegex(
            "(?:^|\\r?\\n)Game\\s*:\\s*([^\\r\\n]+)",
            QRegularExpression::CaseInsensitiveOption);
        const QRegularExpressionMatch match = gameLineRegex.match(m_avaloniumStatus);
        if (match.hasMatch()) {
            gameTitle = sanitizeFileName(match.captured(1).trimmed());
        }
    }

    if (gameTitle.isEmpty()) {
        gameTitle = QFileInfo(m_gamePath).completeBaseName();
        gameTitle.remove(QRegularExpression("-Win64-Shipping$", QRegularExpression::CaseInsensitiveOption));
        gameTitle = sanitizeFileName(gameTitle);
    }

    if (gameTitle.isEmpty()) {
        gameTitle = "mapping";
    }

    return gameTitle;
}

void ModManager::setAdvancedProcessMode(bool enabled) {
    if (m_isAdvancedProcessMode != enabled) {
        m_isAdvancedProcessMode = enabled;
        emit advancedProcessModeChanged();
    }
}

void ModManager::setSelectedProcess(qint64 pid, const QString& name, const QString& path) {
    m_selectedProcessPid = pid;
    m_selectedProcessName = name;
    m_selectedProcessPath = path;
    emit selectedProcessChanged();
}

void ModManager::clearSelectedProcess() {
    m_selectedProcessPid = 0;
    m_selectedProcessName.clear();
    m_selectedProcessPath.clear();
    emit selectedProcessChanged();
}

QVariantList ModManager::getRunningProcesses() const {
    QVariantList list;
#ifdef Q_OS_WIN
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) {
        return list;
    }

    PROCESSENTRY32W pe;
    pe.dwSize = sizeof(PROCESSENTRY32W);

    if (Process32FirstW(hSnapshot, &pe)) {
        do {
            if (pe.th32ProcessID == 0 || pe.th32ProcessID == 4) {
                continue;
            }

            const QString exeName = QString::fromWCharArray(pe.szExeFile);
            if (exeName.isEmpty()) {
                continue;
            }

            QString fullPath;
            QString windowTitle;
            HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pe.th32ProcessID);
            if (hProc) {
                WCHAR buffer[MAX_PATH * 2];
                DWORD size = sizeof(buffer) / sizeof(WCHAR);
                if (QueryFullProcessImageNameW(hProc, 0, buffer, &size)) {
                    fullPath = QDir::cleanPath(QString::fromWCharArray(buffer));
                }
                CloseHandle(hProc);
            }

            windowTitle = getGameWindowTitleFromPid(pe.th32ProcessID);

            QVariantMap map;
            map[QStringLiteral("pid")] = static_cast<qint64>(pe.th32ProcessID);
            map[QStringLiteral("name")] = exeName;
            map[QStringLiteral("path")] = fullPath;
            map[QStringLiteral("title")] = windowTitle;
            list.append(map);
        } while (Process32NextW(hSnapshot, &pe));
    }

    CloseHandle(hSnapshot);

    std::sort(list.begin(), list.end(), [](const QVariant &a, const QVariant &b) {
        return a.toMap()[QStringLiteral("name")].toString().compare(b.toMap()[QStringLiteral("name")].toString(), Qt::CaseInsensitive) < 0;
    });
#endif
    return list;
}

bool ModManager::restartAsAdministrator() {
#ifdef Q_OS_WIN
    const QString appPath = QDir::toNativeSeparators(QCoreApplication::applicationFilePath());

    emit aboutToRestart();

    QCoreApplication::processEvents();
    QThread::msleep(100);

    SHELLEXECUTEINFOW execInfo{};
    execInfo.cbSize = sizeof(execInfo);
    execInfo.fMask = SEE_MASK_NOCLOSEPROCESS;
    execInfo.lpVerb = L"runas";
    execInfo.lpFile = reinterpret_cast<LPCWSTR>(appPath.utf16());
    execInfo.nShow = SW_SHOWNORMAL;

    if (ShellExecuteExW(&execInfo)) {
        if (execInfo.hProcess) {
            CloseHandle(execInfo.hProcess);
        }
        QCoreApplication::quit();
        return true;
    }

    return false;
#else
    return false;
#endif
}

bool ModManager::restartApplication() {
    const QString executablePath = QCoreApplication::applicationFilePath();
    const QStringList arguments = QCoreApplication::arguments().mid(1);
    const QString workingDir = QFileInfo(executablePath).absolutePath();

    emit aboutToRestart();

    QCoreApplication::processEvents();
    QThread::msleep(100);

#ifdef Q_OS_WIN
    if (m_managerHideConsole) {
        auto quoteWindowsArg = [](QString arg) {
            arg.replace("\"", "\\\"");
            if (arg.contains(' ') || arg.contains('\t')) {
                arg.prepend('"');
                arg.append('"');
            }
            return arg;
        };

        QStringList commandParts;
        commandParts << quoteWindowsArg(QDir::toNativeSeparators(executablePath));
        for (const QString& argument : arguments) {
            commandParts << quoteWindowsArg(argument);
        }

        QString commandLine = commandParts.join(' ');
        std::wstring nativeCommandLine = commandLine.toStdWString();
        std::wstring nativeWorkingDir = QDir::toNativeSeparators(workingDir).toStdWString();

        STARTUPINFOW startupInfo{};
        startupInfo.cb = sizeof(startupInfo);

        PROCESS_INFORMATION processInfo{};
        const BOOL created = CreateProcessW(
            nullptr,
            nativeCommandLine.data(),
            nullptr,
            nullptr,
            FALSE,
            CREATE_NO_WINDOW,
            nullptr,
            nativeWorkingDir.c_str(),
            &startupInfo,
            &processInfo);

        if (created) {
            CloseHandle(processInfo.hThread);
            CloseHandle(processInfo.hProcess);
            QCoreApplication::quit();
            return true;
        }

        return false;
    }
#endif

    qint64 pid = 0;
    const bool success = QProcess::startDetached(executablePath, arguments, workingDir, &pid);
    if (success) {
        QCoreApplication::quit();
    }

    return success;
}
