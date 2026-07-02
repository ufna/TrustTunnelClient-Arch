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
    void onNewProfile();

private:
    enum class State { Connected, Disconnected, Transitioning, NoConfig };

    QIcon makeIcon(const QColor &dotColor);
    bool isServiceActive();
    bool isTunUp();
    void setTransitioning();
    void updateTray();

    // --- Profiles ---
    void ensureProfilesMigrated();
    QStringList listProfiles() const;
    QString activeProfileName() const;
    QString profilePath(const QString &name) const;
    void rebuildProfileMenu();
    void switchProfile(const QString &name);
    void openEditor(const QString &path);

#ifdef Q_OS_LINUX
    void runDBus(const QString &method);
#endif
#ifdef Q_OS_MACOS
    void runLaunchctl(const QString &action);
#endif

    QSystemTrayIcon *m_trayIcon;
    QMenu *m_menu;
    QMenu *m_profileMenu;
    QActionGroup *m_profileGroup;
    QAction *m_statusAction;
    QAction *m_toggleAction;
    QAction *m_editAction;
    QTimer *m_pollTimer;
    QTimer *m_transitionTimer;

    QIcon m_greenIcon;
    QIcon m_redIcon;
    QIcon m_yellowIcon;
    QIcon m_greyIcon;

    State m_state = State::Disconnected;
    bool m_connected = false;
    QString m_activeProfile;

    static constexpr const char *CONFIG_PATH = "/opt/trusttunnel_client/trusttunnel_client.toml";
    static constexpr const char *PROFILES_DIR = "/opt/trusttunnel_client/profiles";
    static constexpr int POLL_INTERVAL_MS = 3000;

#ifdef Q_OS_LINUX
    static constexpr const char *SERVICE_NAME = "trusttunnel";
#endif
#ifdef Q_OS_MACOS
    static constexpr const char *DAEMON_LABEL = "com.trusttunnel.client";
    static constexpr const char *DAEMON_PLIST = "/Library/LaunchDaemons/com.trusttunnel.client.plist";
    static constexpr const char *LOG_PATH = "/opt/trusttunnel_client/trusttunnel.log";
#endif
};
