#include "TrayAgent.h"

#include <QApplication>
#include <QProcess>
#include <QPainter>
#include <QPixmap>
#include <QDir>
#include <QFileInfo>

TrayAgent::TrayAgent(QObject *parent)
    : QObject(parent)
{
    m_greenIcon = makeIcon(QColor(0x4C, 0xAF, 0x50));
    m_redIcon = makeIcon(QColor(0xF4, 0x43, 0x36));
    m_yellowIcon = makeIcon(QColor(0xFF, 0xC1, 0x07));

    m_statusAction = new QAction("TrustTunnel: Disconnected", this);
    m_statusAction->setEnabled(false);

    m_toggleAction = new QAction("Enable", this);
    connect(m_toggleAction, &QAction::triggered, this, &TrayAgent::onToggle);

    auto *restartAction = new QAction("Restart", this);
    connect(restartAction, &QAction::triggered, this, &TrayAgent::onRestart);

    auto *editConfigAction = new QAction("Edit Config", this);
    connect(editConfigAction, &QAction::triggered, this, &TrayAgent::onEditConfig);

    auto *viewLogsAction = new QAction("View Logs", this);
    connect(viewLogsAction, &QAction::triggered, this, &TrayAgent::onViewLogs);

    auto *quitAction = new QAction("Quit", this);
    connect(quitAction, &QAction::triggered, qApp, &QApplication::quit);

    m_menu = new QMenu();
    m_menu->addAction(m_statusAction);
    m_menu->addSeparator();
    m_menu->addAction(m_toggleAction);
    m_menu->addAction(restartAction);
    m_menu->addSeparator();
    m_menu->addAction(editConfigAction);
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

QIcon TrayAgent::makeIcon(const QColor &color)
{
    QPixmap pixmap(64, 64);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setBrush(color);
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(4, 4, 56, 56);
    return QIcon(pixmap);
}

bool TrayAgent::isServiceActive()
{
    QProcess proc;
    proc.start("systemctl", {"is-active", SERVICE_NAME});
    proc.waitForFinished(3000);
    return proc.readAllStandardOutput().trimmed() == "active";
}

bool TrayAgent::isTunUp()
{
    return QFileInfo::exists("/sys/class/net/tun0");
}

void TrayAgent::runPrivileged(const QStringList &args)
{
    QProcess::startDetached("pkexec", args);
}

void TrayAgent::setTransitioning()
{
    m_state = State::Transitioning;
    updateTray();
    m_transitionTimer->start(POLL_INTERVAL_MS + 1000);
}

void TrayAgent::pollStatus()
{
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
    switch (m_state) {
    case State::Connected:
        m_trayIcon->setIcon(m_greenIcon);
        m_trayIcon->setToolTip("TrustTunnel: Connected");
        m_statusAction->setText("TrustTunnel: Connected");
        m_toggleAction->setText("Disable");
        break;
    case State::Disconnected:
        m_trayIcon->setIcon(m_redIcon);
        m_trayIcon->setToolTip("TrustTunnel: Disconnected");
        m_statusAction->setText("TrustTunnel: Disconnected");
        m_toggleAction->setText("Enable");
        break;
    case State::Transitioning:
        m_trayIcon->setIcon(m_yellowIcon);
        m_trayIcon->setToolTip("TrustTunnel: Transitioning...");
        m_statusAction->setText("TrustTunnel: Transitioning...");
        break;
    }
}

void TrayAgent::onToggle()
{
    setTransitioning();
    QString action = m_connected ? "stop" : "start";
    runPrivileged({"systemctl", action, SERVICE_NAME});
}

void TrayAgent::onRestart()
{
    setTransitioning();
    runPrivileged({"systemctl", "restart", SERVICE_NAME});
}

void TrayAgent::onEditConfig()
{
    // Open as text/plain so xdg-open always finds a text editor,
    // even if application/toml has no registered handler.
    QProcess::startDetached("xdg-open", {CONFIG_PATH});
    // If xdg-open fails for toml, fall back: set the MIME default once
    // with: xdg-mime default sublime_text.desktop application/toml
}

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
