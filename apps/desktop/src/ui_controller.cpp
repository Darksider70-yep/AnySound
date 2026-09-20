#ifdef QT_CORE_LIB
#include "ui_controller.hpp"

namespace chorus::ui {

UiController::UiController(QObject* parent)
    : QObject(parent) {
    connect(&timer_, &QTimer::timeout, this, &UiController::onTick);
    timer_.start(33); // ~30 Hz tick
}

UiController::~UiController() {
    timer_.stop();
}

QString UiController::sessionPin() const {
    const auto snap = app_.snapshot();
    return QString::fromStdString(snap.session_pin);
}

QVariantList UiController::connectedClients() const {
    QVariantList list;
    const auto snap = app_.snapshot();
    for (const auto& c : snap.connected_clients) {
        QVariantMap map;
        map["id"] = c.id;
        map["name"] = QString::fromStdString(c.name);
        map["platform"] = QString::fromStdString(c.platform);
        map["address"] = QString::fromStdString(c.address);
        map["authenticated"] = c.authenticated;
        map["volume"] = c.volume;
        map["is_muted"] = c.is_muted;
        map["offset_ms"] = c.offset_ms;

        QVariantMap statsMap;
        statsMap["sync_error_us"] = static_cast<qint64>(c.last_stats.sync_error_us);
        statsMap["skew_ppm"] = c.last_stats.skew_ppm;
        statsMap["underruns"] = c.last_stats.underruns;
        statsMap["late"] = c.last_stats.late;
        statsMap["loss_pct"] = c.last_stats.loss_pct;
        statsMap["buffer_ms"] = c.last_stats.buffer_ms;
        map["last_stats"] = statsMap;

        list.append(map);
    }
    return list;
}

QVariantList UiController::discoveredHosts() const {
    QVariantList list;
    const auto snap = app_.snapshot();
    for (const auto& h : snap.discovered_hosts) {
        QVariantMap map;
        map["name"] = QString::fromStdString(h.name);
        map["address"] = QString::fromStdString(h.address);
        map["tcp_port"] = h.tcp_port;
        map["udp_port"] = h.udp_port;
        list.append(map);
    }
    return list;
}

qreal UiController::masterVolume() const {
    return master_volume_;
}

int UiController::targetLatencyMs() const {
    return target_latency_ms_;
}

QString UiController::connectedHostName() const {
    return connected_host_name_;
}

qreal UiController::syncErrorMs() const {
    const auto snap = app_.snapshot();
    return static_cast<qreal>(snap.client_stats.sync_error_us) / 1000.0;
}

QString UiController::syncState() const {
    const auto snap = app_.snapshot();
    if (!snap.is_active) {
        return "connecting";
    }
    const qreal err = std::abs(syncErrorMs());
    if (err <= 5.0) return "tight";
    if (err <= 20.0) return "drifting";
    return "out";
}

qreal UiController::clientVolume() const {
    const auto snap = app_.snapshot();
    return snap.volume;
}

bool UiController::clientMuted() const {
    const auto snap = app_.snapshot();
    return snap.is_muted;
}

int UiController::clientOffsetMs() const {
    const auto snap = app_.snapshot();
    return snap.offset_ms;
}

void UiController::startHost() {
    (void)app_.start_host("", static_cast<uint64_t>(target_latency_ms_), false);
    emit stateChanged();
}

void UiController::stopHost() {
    app_.stop_host();
    emit stateChanged();
}

void UiController::startScan() {
    // Discovery scanner is automatically running in AppController
    emit stateChanged();
}

void UiController::joinHost(const QString& hostIp, int tcpPort, const QString& pin) {
    connected_host_name_ = hostIp;
    (void)app_.join_host(hostIp.toStdString(), static_cast<uint16_t>(tcpPort), pin.toStdString());
    emit stateChanged();
}

void UiController::leaveHost() {
    app_.leave_host();
    emit stateChanged();
}

void UiController::setClientVolume(int clientId, qreal volume) {
    (void)app_.set_client_volume(static_cast<uint32_t>(clientId), static_cast<float>(volume));
    emit stateChanged();
}

void UiController::setClientMute(int clientId, bool mute) {
    (void)app_.set_client_mute(static_cast<uint32_t>(clientId), mute);
    emit stateChanged();
}

void UiController::setClientOffset(int clientId, int offsetMs) {
    (void)app_.set_client_offset_ms(static_cast<uint32_t>(clientId), offsetMs);
    emit stateChanged();
}

void UiController::setMasterVolume(qreal volume) {
    master_volume_ = volume;
    emit masterVolumeChanged();
}

void UiController::setTargetLatency(int latencyMs) {
    target_latency_ms_ = latencyMs;
    emit targetLatencyChanged();
}

void UiController::setLocalVolume(qreal volume) {
    app_.set_volume(static_cast<float>(volume));
    emit clientVolumeChanged();
}

void UiController::setLocalMute(bool mute) {
    app_.set_mute(mute);
    emit clientMutedChanged();
}

void UiController::setLocalOffset(int offsetMs) {
    app_.set_offset_ms(offsetMs);
    emit clientOffsetChanged();
}

void UiController::onTick() {
    app_.update();
    emit stateChanged();
}

}  // namespace chorus::ui
#endif
