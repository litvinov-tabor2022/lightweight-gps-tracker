#include "StateManager.h"

GPS_TRACKER::StateManager::StateManager(GPS_TRACKER::Configuration *configuration) :
        configuration(configuration) {
    bufferSizeLimit = (configuration->GSM_CONFIG.reconnectTime / configuration->CONFIG.sleepTime) * 2;
}

void GPS_TRACKER::StateManager::begin() {
    loadPersistState();
    wakeup_reason = esp_sleep_get_wakeup_cause();
}

void GPS_TRACKER::StateManager::loadPersistState() {
    File file = SPIFFS.open(stateFile);
    DynamicJsonDocument doc(1024);
    DeserializationError error = deserializeJson(doc, file);
    if (error) {
        Serial.println(F("Failed to read persist state, using default configuration"));
        file.close();
        return;
    }
    deserialize(doc);
    Serial.printf("Visited waypoints %d\n", visitedWaypoints);
    file.close();
}

void GPS_TRACKER::StateManager::clearMemory() {
    SPIFFS.remove(stateFile);
}

void GPS_TRACKER::StateManager::serialize(JsonDocument *doc) const {
    (*doc)["visited-waypoints"] = visitedWaypoints;
    (*doc)["last-fast-fix-file-update"] = lastFastFixFileUpdate;
    (*doc)["time-of-last-reset"] = timeOfLastReset;
}

void GPS_TRACKER::StateManager::deserialize(JsonDocument &doc) {
    visitedWaypoints = doc["visited-waypoints"];
    lastFastFixFileUpdate = doc["last-fast-fix-file-update"];
    timeOfLastReset = doc["time-of-last-reset"];
}

void GPS_TRACKER::StateManager::persistState() {
    clearMemory();
    File file = SPIFFS.open(stateFile, FILE_WRITE);
    DynamicJsonDocument doc(1024);
    this->serialize(&doc);
    if (serializeJson(doc, file) == 0) {
        Serial.println(F("Failed to write to file"));
    }
    file.close();
}

MQTT::STATE GPS_TRACKER::StateManager::getMqttState() const {
    return mqttState;
}

GSM::STATE GPS_TRACKER::StateManager::getGsmState() const {
    return gsmState;
}

void GPS_TRACKER::StateManager::setMqttState(MQTT::STATE state) {
    StateManager::mqttState = state;
}

void GPS_TRACKER::StateManager::setGsmState(GSM::STATE state) {
    StateManager::gsmState = state;
}

size_t GPS_TRACKER::StateManager::getVisitedWaypoints() const {
    return visitedWaypoints;
}

void GPS_TRACKER::StateManager::checkCollision() {
    if (visitedWaypoints < configuration->WAYPOINTS.size()) {
        // waypoint reached
        if (distanceToNextWaypoint() <= configuration->CONFIG.accuracy) {
            newWaypointReachedCallback(configuration->WAYPOINTS[visitedWaypoints]);
            visitedWaypoints++;
            persistState();
        }
    }
}

double GPS_TRACKER::StateManager::distanceToNextWaypoint() {
    if (visitedWaypoints == configuration->WAYPOINTS.size()) {
        return std::numeric_limits<double>::max();
    }
    auto distanceFromNextWaypointInKm =
            6378.388 *
            acos(sin(deg2rad(actPosition.lat)) * sin(deg2rad(configuration->WAYPOINTS[visitedWaypoints].lat)) +
                 cos(deg2rad(actPosition.lat)) * cos(
                         deg2rad(configuration->WAYPOINTS[visitedWaypoints].lat)) *
                 cos(deg2rad(configuration->WAYPOINTS[visitedWaypoints].lon - actPosition.lon)));
    double distanceFromNextWaypoint = distanceFromNextWaypointInKm * 1000;
    Serial.printf("Distance from next waypoint %f, %f is %f\n", configuration->WAYPOINTS[visitedWaypoints].lat,
                  configuration->WAYPOINTS[visitedWaypoints].lon, distanceFromNextWaypoint);
    return distanceFromNextWaypoint;
}

double GPS_TRACKER::StateManager::deg2rad(float deg) {
    return (deg * M_PI / 180);
}

void GPS_TRACKER::StateManager::onReachedWaypoint(std::function<void(const waypoint &)> callback) {
    newWaypointReachedCallback = std::move(callback);
}

void GPS_TRACKER::StateManager::test() {
    newWaypointReachedCallback(configuration->WAYPOINTS[visitedWaypoints]);
}

void GPS_TRACKER::StateManager::removePersistedState() {
    clearMemory();
}

void GPS_TRACKER::StateManager::updatePosition(GPS_TRACKER::GPSCoordinates newPosition, bool shouldBeEnqueued) {
    if (shouldBeEnqueued) {
        enqueuePosition(newPosition);
    }
    actPosition = std::move(newPosition);
    Serial.printf("%d positions in queue\n", coordinatesBuffer.size());
    checkCollision();
}

GPS_TRACKER::Timestamp GPS_TRACKER::StateManager::getLastFastFixFileUpdate() const {
    return lastFastFixFileUpdate;
}

void GPS_TRACKER::StateManager::setLastFastFixFileUpdate(GPS_TRACKER::Timestamp lastUpdate) {
    StateManager::lastFastFixFileUpdate = lastUpdate;
    persistState();
}

esp_sleep_wakeup_cause_t GPS_TRACKER::StateManager::getWakeupReason() const {
    return wakeup_reason;
}

void GPS_TRACKER::StateManager::setNumberOfConnectedDevices(uint8_t no) {
    connectedDevices = no;
}

bool GPS_TRACKER::StateManager::couldSleep() {
    return distanceToNextWaypoint() > 150;
}

bool GPS_TRACKER::StateManager::isFastFixFileValid(GPS_TRACKER::Timestamp timestamp) const {
    return lastFastFixFileUpdate - timestamp < 216000;
}

void GPS_TRACKER::StateManager::setTimeOfLastReset(GPS_TRACKER::Timestamp timestamp) {
    this->timeOfLastReset = timestamp;
}

bool GPS_TRACKER::StateManager::shouldBeRestarted() const {
    Timestamp timeFromLastReset = (actTime - timeOfLastReset);
    Serial.printf("seconds since last reset: %lu\n", timeFromLastReset);
    return (timeFromLastReset > 900 && coordinatesBuffer.empty()) || isRestartNeeded;
}

void GPS_TRACKER::StateManager::updateActTime(GPS_TRACKER::Timestamp timestamp) {
    this->actTime = timestamp;
}

void GPS_TRACKER::StateManager::needsRestart() {
    Serial.println("tracker needs reset");
    this->isRestartNeeded = true;
}

bool GPS_TRACKER::StateManager::getPositionFromBuffer(GPS_TRACKER::GPSCoordinates &coordinates) {
    if (coordinatesBuffer.empty())
        return false;
    coordinates = coordinatesBuffer.front();
    coordinatesBuffer.pop_front();
    return true;
}

void GPS_TRACKER::StateManager::enqueuePosition(const GPS_TRACKER::GPSCoordinates& newPosition) {
    this->coordinatesBuffer.push_front(newPosition);
    if (this->coordinatesBuffer.size() > bufferSizeLimit)
        this->coordinatesBuffer.pop_back();
}
