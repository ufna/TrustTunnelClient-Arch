#include "TrayAgent.h"

#include <QApplication>
#include <QProcess>
#include <QPainter>
#include <QPixmap>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QInputDialog>
#include <QActionGroup>
#include <QSvgRenderer>
#include <QDebug>

#ifdef Q_OS_LINUX
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#endif

TrayAgent::TrayAgent(QObject *parent)
    : QObject(parent)
{
    m_greenIcon = makeIcon(QColor(0x4C, 0xAF, 0x50));
    m_redIcon = makeIcon(QColor(0xF4, 0x43, 0x36));
    m_yellowIcon = makeIcon(QColor(0xFF, 0xC1, 0x07));
    m_greyIcon = makeIcon(QColor(0x9E, 0x9E, 0x9E));

    ensureProfilesMigrated();
    m_activeProfile = activeProfileName();

    m_statusAction = new QAction("TrustTunnel: Disconnected", this);
    m_statusAction->setEnabled(false);

    m_toggleAction = new QAction("Enable", this);
    connect(m_toggleAction, &QAction::triggered, this, &TrayAgent::onToggle);

    auto *restartAction = new QAction("Restart", this);
    connect(restartAction, &QAction::triggered, this, &TrayAgent::onRestart);

    m_editAction = new QAction("Edit current profile", this);
    connect(m_editAction, &QAction::triggered, this, &TrayAgent::onEditConfig);

    auto *viewLogsAction = new QAction("View Logs", this);
    connect(viewLogsAction, &QAction::triggered, this, &TrayAgent::onViewLogs);

    auto *quitAction = new QAction("Quit", this);
    connect(quitAction, &QAction::triggered, qApp, &QApplication::quit);

    // --- Profile submenu ---
    m_profileGroup = new QActionGroup(this);
    m_profileGroup->setExclusive(true);
    m_profileMenu = new QMenu();
    connect(m_profileMenu, &QMenu::aboutToShow, this, &TrayAgent::rebuildProfileMenu);
    rebuildProfileMenu();

    auto *profileMenuAction = new QAction("Profile", this);
    profileMenuAction->setMenu(m_profileMenu);

    m_menu = new QMenu();
    m_menu->addAction(m_statusAction);
    m_menu->addSeparator();
    m_menu->addAction(m_toggleAction);
    m_menu->addAction(restartAction);
    m_menu->addSeparator();
    m_menu->addAction(profileMenuAction);
    m_menu->addSeparator();
    m_menu->addAction(m_editAction);
    m_menu->addAction(viewLogsAction);
    m_menu->addSeparator();
    m_menu->addAction(quitAction);

    m_trayIcon = new QSystemTrayIcon(m_redIcon, this);
    m_trayIcon->setContextMenu(m_menu);
    m_trayIcon->setToolTip("TrustTunnel: Disconnected");
    m_trayIcon->show();

    m_transitionTimer = new QTimer(this);
    m_transitionTimer->setSingleShot(true);
    connect(m_transitionTimer, &QTimer::timeout, this, [this]() {
        m_state = State::Disconnected; // will be corrected by next poll
    });

    m_pollTimer = new QTimer(this);
    connect(m_pollTimer, &QTimer::timeout, this, &TrayAgent::pollStatus);
    m_pollTimer->start(POLL_INTERVAL_MS);

    pollStatus();
}

QIcon TrayAgent::makeIcon(const QColor &dotColor)
{
    static const char *svgData = R"SVG(
<svg viewBox="0 0 24 24" xmlns="http://www.w3.org/2000/svg">
<path d="M8.64504 6.29034C5.99845 7.84572 4.83059 10.9545 5.61888 13.7779C5.63299 13.8284 5.64781 13.879 5.66318 13.9294C5.86305 14.8145 5.10125 15.845 4.08838 16.6505C5.28501 16.1576 6.55605 15.9936 7.23207 16.599C7.26855 16.6369 7.30546 16.6743 7.34273 16.7112C9.42582 18.7737 12.7101 19.2662 15.3567 17.7108C18.0033 16.1554 19.1711 13.0465 18.3828 10.2231C18.3687 10.1725 18.3539 10.1221 18.3386 10.0718C18.1387 9.18663 18.9005 8.15591 19.9135 7.35033C18.7168 7.84329 17.4457 8.0075 16.7697 7.40219C16.7332 7.36427 16.6962 7.32671 16.6589 7.2898C14.5758 5.22732 11.2916 4.73497 8.64504 6.29034Z" fill="#F6F6F6"/>
<path d="M6.91113 3.34009C10.9253 0.981024 15.907 1.72748 19.0664 4.85571C19.1229 4.91165 19.1791 4.96818 19.2344 5.02563C20.2597 5.94374 22.187 5.69519 24.002 4.94751C22.4656 6.16934 21.31 7.73291 21.6133 9.07544C21.6366 9.15175 21.6593 9.22833 21.6807 9.30493C22.8762 13.5872 21.1049 18.3023 17.0908 20.6614C13.0767 23.0204 8.09499 22.274 4.93555 19.1458L4.76758 18.9749C3.74217 18.0572 1.81467 18.3054 0 19.053C1.53618 17.8312 2.69182 16.2685 2.38867 14.926C2.36537 14.8497 2.34268 14.7732 2.32129 14.6965C1.12564 10.4142 2.89708 5.69918 6.91113 3.34009ZM16.6562 7.29028C14.5732 5.22782 11.2892 4.73495 8.64258 6.29028C5.99613 7.84558 4.82817 10.9543 5.61621 13.7776C5.63032 13.8281 5.64577 13.8796 5.66113 13.9299C5.86058 14.8149 5.09861 15.8452 4.08594 16.6506C5.28247 16.1577 6.55343 15.9937 7.22949 16.5989C7.26589 16.6367 7.30267 16.6744 7.33984 16.7112C9.42294 18.7737 12.7079 19.2666 15.3545 17.7112C18.0011 16.1558 19.1682 13.0463 18.3799 10.2229C18.3658 10.1725 18.3513 10.1217 18.3359 10.0715C18.1363 9.1865 18.8984 8.15628 19.9111 7.35083C18.7147 7.8437 17.4437 8.00754 16.7676 7.40259C16.7311 7.3647 16.6935 7.32717 16.6562 7.29028Z" fill="#3972AA"/>
<path fill-rule="evenodd" clip-rule="evenodd" d="M14.9406 9.8741C15.2426 10.2113 15.2418 10.757 14.9388 11.0931L12.6258 13.6581C11.8335 14.6206 11.3335 14.6206 10.5094 13.6581L9.06106 12.0519C8.75803 11.7158 8.7572 11.1701 9.05921 10.8329C9.36122 10.4957 9.8517 10.4948 10.1547 10.8308L11.5676 12.3977L13.8451 9.87204C14.1481 9.53599 14.6386 9.53691 14.9406 9.8741Z" fill="#3972AA"/>
</svg>)SVG";

    QSvgRenderer renderer{QByteArray{svgData}};
    QPixmap pixmap(64, 64);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    renderer.render(&painter);

    // Status dot in bottom-right corner
    constexpr int dotSize = 24;
    constexpr int margin = 2;
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setBrush(dotColor);
    painter.setPen(QPen(Qt::white, 2));
    painter.drawEllipse(64 - dotSize - margin, 64 - dotSize - margin, dotSize, dotSize);

    return QIcon(pixmap);
}

// --- Profiles ---

void TrayAgent::ensureProfilesMigrated()
{
    QDir().mkpath(PROFILES_DIR);

    QFileInfo cfg(CONFIG_PATH);
    if (cfg.isSymLink())
        return; // already migrated

    // Regular file (or missing) -> fold into profiles/default.toml and point a
    // relative symlink at it. The daemon keeps reading CONFIG_PATH; the symlink
    // transparently resolves to the active profile.
    const QString defaultPath = profilePath("default");
    if (!QFileInfo::exists(defaultPath) && cfg.exists())
        QFile::copy(CONFIG_PATH, defaultPath);

    if (QFileInfo::exists(CONFIG_PATH))
        QFile::remove(CONFIG_PATH);
    if (!QFile::link(QStringLiteral("profiles/default.toml"), CONFIG_PATH))
        qWarning() << "Failed to create profile symlink at" << CONFIG_PATH;
}

QStringList TrayAgent::listProfiles() const
{
    QDir dir(PROFILES_DIR);
    if (!dir.exists())
        return {};

    QStringList names;
    const QStringList files = dir.entryList({"*.toml"}, QDir::Files, QDir::Name);
    for (const auto &f : files)
        names << QFileInfo(f).completeBaseName();
    return names;
}

QString TrayAgent::activeProfileName() const
{
    QFileInfo cfg(CONFIG_PATH);
    if (cfg.isSymLink())
        return QFileInfo(cfg.symLinkTarget()).completeBaseName();
    return {};
}

QString TrayAgent::profilePath(const QString &name) const
{
    return QString(PROFILES_DIR) + '/' + name + ".toml";
}

void TrayAgent::rebuildProfileMenu()
{
    m_profileMenu->clear();
    m_activeProfile = activeProfileName();

    const QStringList profiles = listProfiles();
    for (const auto &name : profiles) {
        QAction *a = m_profileMenu->addAction(name);
        a->setCheckable(true);
        a->setActionGroup(m_profileGroup);
        a->setChecked(name == m_activeProfile);
        connect(a, &QAction::triggered, this, [this, name]() { switchProfile(name); });
    }

    if (profiles.isEmpty()) {
        QAction *empty = m_profileMenu->addAction("(no profiles)");
        empty->setEnabled(false);
    }

    m_profileMenu->addSeparator();
    QAction *newAction = m_profileMenu->addAction("New profile from current…");
    connect(newAction, &QAction::triggered, this, &TrayAgent::onNewProfile);

    // Reflect the active name in the Edit action.
    m_editAction->setText(m_activeProfile.isEmpty()
                              ? QStringLiteral("Edit current profile")
                              : QStringLiteral("Edit \"%1\"").arg(m_activeProfile));
}

void TrayAgent::switchProfile(const QString &name)
{
    if (name.isEmpty() || name == m_activeProfile)
        return;

    const QString target = profilePath(name);
    if (!QFileInfo::exists(target)) {
        m_trayIcon->showMessage("TrustTunnel", "Profile not found: " + name,
                                QSystemTrayIcon::Warning, 5000);
        return;
    }

    // Repoint the active config symlink atomically: create a temp symlink, then
    // rename it over the existing one (same filesystem => atomic swap).
    const QString rel = QStringLiteral("profiles/") + name + ".toml";
    const QString tmp = QString(CONFIG_PATH) + ".tmp";
    if (QFileInfo::exists(tmp))
        QFile::remove(tmp);
    if (!QFile::link(rel, tmp)) {
        m_trayIcon->showMessage("TrustTunnel", "Failed to switch profile",
                                QSystemTrayIcon::Critical, 5000);
        return;
    }
    if (!QFile::rename(tmp, CONFIG_PATH)) {
        QFile::remove(tmp);
        m_trayIcon->showMessage("TrustTunnel", "Failed to switch profile",
                                QSystemTrayIcon::Critical, 5000);
        return;
    }

    m_activeProfile = name;
    m_editAction->setText(QStringLiteral("Edit \"%1\"").arg(name));
    rebuildProfileMenu();

    // Apply by restarting the daemon so it re-reads the new profile.
    setTransitioning();
    if (isServiceActive()) {
#ifdef Q_OS_LINUX
        runDBus("RestartUnit");
#endif
#ifdef Q_OS_MACOS
        runLaunchctl("restart");
#endif
    } else {
        m_trayIcon->showMessage("TrustTunnel",
            QStringLiteral("Profile set to \"%1\". Start the service to connect.").arg(name),
            QSystemTrayIcon::Information, 5000);
    }
}

void TrayAgent::onNewProfile()
{
    bool ok = false;
    const QString name = QInputDialog::getText(nullptr,
        QStringLiteral("New TrustTunnel profile"),
        QStringLiteral("Profile name:"), QLineEdit::Normal, QString(), &ok).trimmed();
    if (!ok || name.isEmpty())
        return;

    if (name.contains('/') || name.contains('\\') ||
        name.contains("..") || name.contains(':')) {
        m_trayIcon->showMessage("TrustTunnel", "Invalid profile name",
                                QSystemTrayIcon::Warning, 5000);
        return;
    }

    const QString path = profilePath(name);
    if (QFileInfo::exists(path)) {
        m_trayIcon->showMessage("TrustTunnel", "Profile already exists: " + name,
                                QSystemTrayIcon::Warning, 5000);
        return;
    }

    // Seed from the current active config (canonical target = current profile).
    const QString src = QFileInfo(CONFIG_PATH).canonicalFilePath();
    if (!src.isEmpty() && QFileInfo::exists(src)) {
        QFile::copy(src, path);
    } else {
        QFile f(path);
        if (f.open(QIODevice::WriteOnly)) // empty placeholder
            f.close();
    }

    rebuildProfileMenu();
    openEditor(path);
    m_trayIcon->showMessage("TrustTunnel",
        QStringLiteral("Created profile \"%1\". Edit it, then switch to apply.").arg(name),
        QSystemTrayIcon::Information, 6000);
}

// --- Platform-specific: editor ---

#ifdef Q_OS_LINUX
void TrayAgent::openEditor(const QString &path)
{
    QProcess::startDetached("xdg-open", {path});
}
#endif

#ifdef Q_OS_MACOS
void TrayAgent::openEditor(const QString &path)
{
    QProcess::startDetached("open", {"-t", path});
}
#endif

// --- Platform-specific: service status ---

#ifdef Q_OS_LINUX
bool TrayAgent::isServiceActive()
{
    QProcess proc;
    proc.start("systemctl", {"is-active", SERVICE_NAME});
    proc.waitForFinished(3000);
    return proc.readAllStandardOutput().trimmed() == "active";
}
#endif

#ifdef Q_OS_MACOS
bool TrayAgent::isServiceActive()
{
    QProcess proc;
    proc.start("pgrep", {"-x", "trusttunnel_client"});
    proc.waitForFinished(3000);
    return proc.exitCode() == 0;
}
#endif

// --- Platform-specific: tunnel interface check ---

#ifdef Q_OS_LINUX
bool TrayAgent::isTunUp()
{
    return QFileInfo::exists("/sys/class/net/tun0");
}
#endif

#ifdef Q_OS_MACOS
bool TrayAgent::isTunUp()
{
    // System utun interfaces (iCloud, etc.) only have link-local IPv6.
    // TrustTunnel's utun has an IPv4 address — look for that.
    QProcess proc;
    proc.start("bash", {"-c", "ifconfig | grep -B2 'inet ' | grep -q '^utun'"});
    proc.waitForFinished(3000);
    return proc.exitCode() == 0;
}
#endif

// --- Platform-specific: service control ---

#ifdef Q_OS_LINUX
void TrayAgent::runDBus(const QString &method)
{
    QDBusMessage msg = QDBusMessage::createMethodCall(
        "org.freedesktop.systemd1",
        "/org/freedesktop/systemd1",
        "org.freedesktop.systemd1.Manager",
        method);
    msg << QString(SERVICE_NAME) + ".service" << QString("replace");
    msg.setInteractiveAuthorizationAllowed(true);

    auto pending = QDBusConnection::systemBus().asyncCall(msg);
    auto *watcher = new QDBusPendingCallWatcher(pending, this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *w) {
        w->deleteLater();
        QDBusPendingReply<QDBusObjectPath> reply = *w;
        if (reply.isError()) {
            qWarning() << "D-Bus call failed:" << reply.error().name() << reply.error().message();
            m_trayIcon->showMessage("TrustTunnel", "Service error: " + reply.error().message(),
                                    QSystemTrayIcon::Critical, 5000);
        }
    });
}
#endif

#ifdef Q_OS_MACOS
void TrayAgent::runLaunchctl(const QString &action)
{
    QString cmd;
    if (action == "start") {
        cmd = QString("launchctl load -w %1").arg(DAEMON_PLIST);
    } else if (action == "stop") {
        cmd = QString("launchctl unload %1").arg(DAEMON_PLIST);
    } else if (action == "restart") {
        cmd = QString("killall trusttunnel_client 2>/dev/null; sleep 1; launchctl unload %1 2>/dev/null; launchctl load -w %1").arg(DAEMON_PLIST);
    }

    QString script = QString("do shell script \"%1\" with administrator privileges").arg(cmd);

    auto *proc = new QProcess(this);
    connect(proc, qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
            this, [this, proc](int exitCode, QProcess::ExitStatus) {
        proc->deleteLater();
        if (exitCode != 0) {
            QString err = proc->readAllStandardError().trimmed();
            if (!err.isEmpty()) {
                qWarning() << "launchctl failed:" << err;
                m_trayIcon->showMessage("TrustTunnel", "Error: " + err,
                                        QSystemTrayIcon::Critical, 5000);
            }
        }
    });

    proc->start("osascript", {"-e", script});
}
#endif

// --- Common ---

void TrayAgent::setTransitioning()
{
    m_state = State::Transitioning;
    updateTray();
    m_transitionTimer->start(POLL_INTERVAL_MS + 1000);
}

void TrayAgent::pollStatus()
{
    if (!QFileInfo(CONFIG_PATH).exists()) {
        m_state = State::NoConfig;
        m_connected = false;
        updateTray();
        return;
    }

    bool active = isServiceActive();
    bool tun = isTunUp();
    m_connected = active && tun;

    if (m_state != State::Transitioning) {
        m_state = m_connected ? State::Connected : State::Disconnected;
    }

    updateTray();
}

void TrayAgent::updateTray()
{
    const QString label = m_activeProfile.isEmpty()
                              ? QString()
                              : QStringLiteral(" — %1").arg(m_activeProfile);

    switch (m_state) {
    case State::Connected:
        m_trayIcon->setIcon(m_greenIcon);
        m_trayIcon->setToolTip("TrustTunnel: Connected" + label);
        m_statusAction->setText("TrustTunnel: Connected" + label);
        m_toggleAction->setText("Disable");
        m_toggleAction->setEnabled(true);
        break;
    case State::Disconnected:
        m_trayIcon->setIcon(m_redIcon);
        m_trayIcon->setToolTip("TrustTunnel: Disconnected" + label);
        m_statusAction->setText("TrustTunnel: Disconnected" + label);
        m_toggleAction->setText("Enable");
        m_toggleAction->setEnabled(true);
        break;
    case State::Transitioning:
        m_trayIcon->setIcon(m_yellowIcon);
        m_trayIcon->setToolTip("TrustTunnel: Switching..." + label);
        m_statusAction->setText("TrustTunnel: Switching..." + label);
        break;
    case State::NoConfig:
        m_trayIcon->setIcon(m_greyIcon);
        m_trayIcon->setToolTip("TrustTunnel: No profile configured");
        m_statusAction->setText("TrustTunnel: No profile configured");
        m_toggleAction->setText("Enable");
        m_toggleAction->setEnabled(false);
        break;
    }
}

// --- Platform-specific: toggle/restart ---

#ifdef Q_OS_LINUX
void TrayAgent::onToggle()
{
    setTransitioning();
    runDBus(m_connected ? "StopUnit" : "StartUnit");
}

void TrayAgent::onRestart()
{
    setTransitioning();
    runDBus("RestartUnit");
}
#endif

#ifdef Q_OS_MACOS
void TrayAgent::onToggle()
{
    setTransitioning();
    runLaunchctl(m_connected ? "stop" : "start");
}

void TrayAgent::onRestart()
{
    setTransitioning();
    runLaunchctl("restart");
}
#endif

// --- Platform-specific: edit config ---

void TrayAgent::onEditConfig()
{
    // CONFIG_PATH is a symlink to the active profile, so editing it edits the
    // profile source directly. Restart the service to apply changes.
    openEditor(CONFIG_PATH);
}

// --- Platform-specific: view logs ---

#ifdef Q_OS_LINUX
void TrayAgent::onViewLogs()
{
    struct TermCmd {
        QString program;
        QStringList args;
    };

    const TermCmd terminals[] = {
        {"konsole",        {"-e", "journalctl", "-u", SERVICE_NAME, "-f"}},
        {"gnome-terminal", {"--", "journalctl", "-u", SERVICE_NAME, "-f"}},
        {"xterm",          {"-e", "journalctl", "-u", SERVICE_NAME, "-f"}},
    };

    for (const auto &term : terminals) {
        if (QProcess::startDetached(term.program, term.args)) {
            return;
        }
    }
}
#endif

#ifdef Q_OS_MACOS
void TrayAgent::onViewLogs()
{
    QString script = QString(
        "tell application \"Terminal\"\n"
        "    do script \"tail -f %1\"\n"
        "    activate\n"
        "end tell").arg(LOG_PATH);
    QProcess::startDetached("osascript", {"-e", script});
}
#endif
