#include "clientData.hpp"
#include <Windows.h>
#include "Logger.hpp"
#include "task_force_radio.hpp"

void clientData::updatePosition(const unitPositionPacket & packet) {
	LockGuard_exclusive lock(&m_lock);

	clientPosition = packet.position;
	viewDirection = packet.viewDirection;
	canSpeak = packet.canSpeak;
	canUseSWRadio = packet.canUseSWRadio;
	canUseLRRadio = packet.canUseLRRadio;
	canUseDDRadio = packet.canUseDDRadio;
	vehicleId = packet.vehicleID;
	terrainInterception = packet.terrainInterception;
	voiceVolumeMultiplifier = packet.voiceVolume;
	objectInterception = packet.objectInterception;


    lastPositionUpdateTime = std::chrono::system_clock::now();
	dataFrame = TFAR::getInstance().m_gameData.currentDataFrame;
}

float clientData::effectiveDistanceTo(std::shared_ptr<clientData>& other) {
	float d = getClientPosition().distanceTo(other->getClientPosition());
	// (bob distance player) + (bob call TFAR_fnc_calcTerrainInterception) * 7 + (bob call TFAR_fnc_calcTerrainInterception) * 7 * ((bob distance player) / 2000.0)
	float result = d +
		+(other->terrainInterception * TFAR::getInstance().m_gameData.terrainIntersectionCoefficient)
		+ (other->terrainInterception * TFAR::getInstance().m_gameData.terrainIntersectionCoefficient * d / 2000.0f);
	result *= TFAR::getInstance().m_gameData.receivingDistanceMultiplicator;
	return result;
}
