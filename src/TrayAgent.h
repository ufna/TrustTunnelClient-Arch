#pragma once

#include <QObject>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QTimer>
#include <QIcon>

class TrayAgent : public QObject {
    Q_OBJECT

public:
    explicit TrayAgent(QObject *parent = nullptr);

private slots:
    void pollStatus();
    void onToggle();
    void onRestart();
    void onEditConfig();
    void onViewLogs();

private:
    enum class State { Connected, Disconnected, Transitioning };

    QIcon makeIcon(const QColor &color);
    bool isServiceActive();
    bool isTunUp();
    void runPrivileged(const QStringList &args);
    void setTransitioning();
    void updateTray();

    QSystemTrayIcon *m_trayIcon;
    QMenu *m_menu;
    QAction *m_statusAction;
    QAction *m_toggleAction;
    QTimer *m_pollTimer;
    QTimer *m_transitionTimer;

    QIcon m_greenIcon;
    QIcon m_redIcon;
    QIcon m_yellowIcon;

    State m_state = State::Disconnected;
    bool m_connected = false;

    static constexpr const char *SERVICE_NAME = "trusttunnel";
    static constexpr const char *CONFIG_PATH = "/opt/trusttunnel_client/trusttunnel_client.toml";
    static constexpr int POLL_INTERVAL_MS = 3000;
};
