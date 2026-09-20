#ifdef QT_CORE_LIB
#include <chorus/app/app_controller.hpp>

#include <QObject>
#include <QString>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>

namespace chorus::ui {

#ifdef QT_CORE_LIB
class UiController : public QObject {
    Q_OBJECT

    Q_PROPERTY(QString sessionPin READ sessionPin NOTIFY stateChanged)
    Q_PROPERTY(QVariantList connectedClients READ connectedClients NOTIFY stateChanged)
    Q_PROPERTY(QVariantList discoveredHosts READ discoveredHosts NOTIFY stateChanged)
    Q_PROPERTY(qreal masterVolume READ masterVolume WRITE setMasterVolume NOTIFY masterVolumeChanged)
    Q_PROPERTY(int targetLatencyMs READ targetLatencyMs WRITE setTargetLatency NOTIFY targetLatencyChanged)
    Q_PROPERTY(QString connectedHostName READ connectedHostName NOTIFY stateChanged)
    Q_PROPERTY(qreal syncErrorMs READ syncErrorMs NOTIFY stateChanged)
    Q_PROPERTY(QString syncState READ syncState NOTIFY stateChanged)
    Q_PROPERTY(qreal clientVolume READ clientVolume WRITE setLocalVolume NOTIFY clientVolumeChanged)
    Q_PROPERTY(bool clientMuted READ clientMuted WRITE setLocalMute NOTIFY clientMutedChanged)
    Q_PROPERTY(int clientOffsetMs READ clientOffsetMs WRITE setLocalOffset NOTIFY clientOffsetChanged)

public:
    explicit UiController(QObject* parent = nullptr);
    ~UiController() override;

    [[nodiscard]] QString sessionPin() const;
    [[nodiscard]] QVariantList connectedClients() const;
    [[nodiscard]] QVariantList discoveredHosts() const;
    [[nodiscard]] qreal masterVolume() const;
    [[nodiscard]] int targetLatencyMs() const;
    [[nodiscard]] QString connectedHostName() const;
    [[nodiscard]] qreal syncErrorMs() const;
    [[nodiscard]] QString syncState() const;
    [[nodiscard]] qreal clientVolume() const;
    [[nodiscard]] bool clientMuted() const;
    [[nodiscard]] int clientOffsetMs() const;

public slots:
    void startHost();
    void stopHost();
    void startScan();
    void joinHost(const QString& hostIp, int tcpPort, const QString& pin);
    void leaveHost();

    void setClientVolume(int clientId, qreal volume);
    void setClientMute(int clientId, bool mute);
    void setClientOffset(int clientId, int offsetMs);

    void setMasterVolume(qreal volume);
    void setTargetLatency(int latencyMs);
    void setLocalVolume(qreal volume);
    void setLocalMute(bool mute);
    void setLocalOffset(int offsetMs);

signals:
    void stateChanged();
    void masterVolumeChanged();
    void targetLatencyChanged();
    void clientVolumeChanged();
    void clientMutedChanged();
    void clientOffsetChanged();

private slots:
    void onTick();

private:
    chorus::AppController app_;
    QTimer timer_;
    qreal master_volume_{1.0};
    int target_latency_ms_{300};
    QString connected_host_name_{"Host"};
};
#endif

}  // namespace chorus::ui
