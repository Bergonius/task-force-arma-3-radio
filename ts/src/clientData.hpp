#pragma once
#include "common.hpp"
#include <map>
#include <ts3_functions.h>
#include "RadioEffect.hpp"
#include "Clunk.h"
#include <clunk/wav_file.h>
#include "simpleSource/SimpleComp.h"
#include <memory> //shared_ptr
#include <unordered_map>
#include "Locks.hpp"
enum class OVER_RADIO_TYPE {   //Receiving FROM senders Radio.
	LISTEN_TO_SW,
	LISTEN_TO_LR,
	LISTEN_TO_DD, //#diverRadio
	LISTEN_TO_NONE
};

struct unitPositionPacket {
	std::string nickname;
	Position3D position;
	Direction3D viewDirection;
	bool canSpeak;
	bool canUseSWRadio;
	bool canUseLRRadio;
	bool canUseDDRadio;
	std::string vehicleID;
	int terrainInterception;
	float voiceVolume;
	int objectInterception;
};

class clientDataEffects {
public:
	clientDataEffects() {

		compressor.setSampleRate(48000);
		compressor.setThresh(65);
		compressor.setRelease(300);
		compressor.setAttack(1);
		compressor.setRatio(0.1);
		compressor.initRuntime();

		resetRadioEffect();
	}

	Dsp::SimpleFilter<Dsp::Butterworth::BandPass<2>, MAX_CHANNELS>* getSpeakerPhone(std::string key) {
		LockGuard_shared lock_shared(&m_lock);
		if (!filtersPhone.count(key)) {
			lock_shared.unlock();
			LockGuard_exclusive lock_exclusive(&m_lock);
			filtersPhone[key] = std::make_unique<Dsp::SimpleFilter<Dsp::Butterworth::BandPass<2>, MAX_CHANNELS>>();
			filtersPhone[key]->setup(2, 48000, 1850, 1550);
		}
		return filtersPhone[key].get();
	}

	Dsp::SimpleFilter<Dsp::Butterworth::BandPass<1>, MAX_CHANNELS>* getSpeakerFilter(std::string key) {
		LockGuard_shared lock_shared(&m_lock);
		if (!filtersSpeakers.count(key)) {
			lock_shared.unlock();
			LockGuard_exclusive lock_exclusive(&m_lock);
			filtersSpeakers[key] = std::make_unique<Dsp::SimpleFilter<Dsp::Butterworth::BandPass<1>, MAX_CHANNELS>>();
			filtersSpeakers[key]->setup(1, 48000, 2000, 1000);
		}
		return filtersSpeakers[key].get();
	}

	PersonalRadioEffect* getSwRadioEffect(std::string key) {
		LockGuard_shared lock_shared(&m_lock);
		if (!swEffects.count(key)) {
			lock_shared.unlock();
			LockGuard_exclusive lock_exclusive(&m_lock);
			swEffects[key] = std::make_unique<PersonalRadioEffect>();
		}
		return swEffects[key].get();
	}

	LongRangeRadioffect* getLrRadioEffect(std::string key) {
		LockGuard_shared lock_shared(&m_lock);
		if (!lrEffects.count(key)) {
			lock_shared.unlock();
			LockGuard_exclusive lock_exclusive(&m_lock);
			lrEffects[key] = std::make_unique<LongRangeRadioffect>();
		}
		return lrEffects[key].get();
	}

	UnderWaterRadioEffect* getUnderwaterRadioEffect(std::string key) {
		LockGuard_shared lock_shared(&m_lock);
		if (!ddEffects.count(key)) {
			lock_shared.unlock();
			LockGuard_exclusive lock_exclusive(&m_lock);
			ddEffects[key] = std::make_unique<UnderWaterRadioEffect>();
		}
		return ddEffects[key].get();
	}

	Clunk* getClunk(std::string key) {
		LockGuard_shared lock_shared(&m_lock);
		if (!clunks.count(key)) {
			lock_shared.unlock();
			LockGuard_exclusive lock_exclusive(&m_lock);
			clunks[key] = std::make_unique<Clunk>();
		}
		return clunks[key].get();
	}

	void removeClunk(std::string key) {
		LockGuard_exclusive lock(&m_lock);
		clunks.erase(key);
	}

	Dsp::SimpleFilter<Dsp::Butterworth::LowPass<4>, MAX_CHANNELS>* getFilterCantSpeak(std::string key) {
		LockGuard_shared lock_shared(&m_lock);
		if (!filtersCantSpeak.count(key)) {
			lock_shared.unlock();
			LockGuard_exclusive lock_exclusive(&m_lock);
			filtersCantSpeak[key] = std::make_unique<Dsp::SimpleFilter<Dsp::Butterworth::LowPass<4>, MAX_CHANNELS>>();
			filtersCantSpeak[key]->setup(4, 48000, 100);
		}
		return filtersCantSpeak[key].get();
	}

	Dsp::SimpleFilter<Dsp::Butterworth::LowPass<2>, MAX_CHANNELS>* getFilterVehicle(std::string key, float vehicleVolumeLoss) {
		std::string byKey = key + std::to_string(vehicleVolumeLoss);
		LockGuard_shared lock_shared(&m_lock);
		if (!filtersVehicle.count(byKey)) {
			lock_shared.unlock();
			LockGuard_exclusive lock_exclusive(&m_lock);
			filtersVehicle[byKey] = std::make_unique<Dsp::SimpleFilter<Dsp::Butterworth::LowPass<2>, MAX_CHANNELS>>();
			filtersVehicle[byKey]->setup(2, 48000, 20000 * (1.0 - vehicleVolumeLoss) / 4.0);
		}
		return filtersVehicle[byKey].get();
	}

	std::map<uint8_t, std::unique_ptr<Dsp::SimpleFilter<Dsp::Butterworth::LowPass<2>, MAX_CHANNELS>>> filtersObjectInterception;

	Dsp::SimpleFilter<Dsp::Butterworth::LowPass<2>, MAX_CHANNELS>* getFilterObjectInterception(uint8_t objectCount) {
		objectCount = std::min(objectCount, static_cast<uint8_t>(5));
		LockGuard_shared lock_shared(&m_lock);
		if (!filtersObjectInterception.count(objectCount)) {
			lock_shared.unlock();
			LockGuard_exclusive lock_exclusive(&m_lock);
			filtersObjectInterception[objectCount] = std::make_unique<Dsp::SimpleFilter<Dsp::Butterworth::LowPass<2>, MAX_CHANNELS>>();
			filtersObjectInterception[objectCount]->setup(2, 48000, 1800 - ((objectCount - 1) * 400)); //#TODO not happy with that..
		}
		return filtersObjectInterception[objectCount].get();
	}


	chunkware_simple::SimpleComp compressor;
	void resetRadioEffect() {
		LockGuard_exclusive lock(&m_lock);
		swEffects.clear();
		lrEffects.clear();
		ddEffects.clear();
		filtersSpeakers.clear();
		filtersPhone.clear();
	}

	void resetVoices() {
		LockGuard_exclusive lock(&m_lock);
		clunks.clear();
		filtersCantSpeak.clear();
		filtersVehicle.clear();
	}
	  private:
		  using LockGuard_shared = LockGuard_shared<ReadWriteLock>;
		  using LockGuard_exclusive = LockGuard_exclusive<ReadWriteLock>;
		  ReadWriteLock m_lock;
		  
		  //Filters and Effects
		  template<typename T>
		  using effectMap = std::map<std::string, std::unique_ptr<T>>;

		  effectMap<PersonalRadioEffect> swEffects;
		  effectMap<LongRangeRadioffect> lrEffects;
		  effectMap<UnderWaterRadioEffect> ddEffects;
		  effectMap<Clunk> clunks;
		  
		  effectMap<Dsp::SimpleFilter<Dsp::Butterworth::LowPass<4>, MAX_CHANNELS>> filtersCantSpeak;
		  effectMap<Dsp::SimpleFilter<Dsp::Butterworth::LowPass<2>, MAX_CHANNELS>> filtersVehicle;
		  effectMap<Dsp::SimpleFilter<Dsp::Butterworth::BandPass<1>, MAX_CHANNELS>> filtersSpeakers;
		  effectMap<Dsp::SimpleFilter<Dsp::Butterworth::BandPass<2>, MAX_CHANNELS>> filtersPhone;
};






class clientData {
public:


	explicit clientData(TSClientID _clientID): clientId(_clientID) {

	}


	void updatePosition(const unitPositionPacket & packet);



	std::string getNickname() { LockGuard_shared lock(&m_lock); return nickname; }
	void setNickname(const std::string& val) { LockGuard_exclusive lock(&m_lock); nickname = val; }
	dataType::Position3D getClientPosition() { LockGuard_shared lock(&m_lock); return clientPosition; }
	void setClientPosition(const Position3D& val) { LockGuard_exclusive lock(&m_lock); clientPosition = val; }
	dataType::Direction3D getViewDirection() { LockGuard_shared lock(&m_lock); return viewDirection; }
	void setViewDirection(const Direction3D& val) { LockGuard_exclusive lock(&m_lock); viewDirection = val; }
	std::chrono::system_clock::time_point getLastPositionUpdateTime() { LockGuard_shared lock(&m_lock); return lastPositionUpdateTime; }
	void setLastPositionUpdateTime(const std::chrono::system_clock::time_point& val) { LockGuard_exclusive lock(&m_lock); lastPositionUpdateTime = val; }
	std::string getCurrentTransmittingFrequency() { LockGuard_shared lock(&m_lock); return currentTransmittingFrequency; }
	void setCurrentTransmittingFrequency(const std::string&val) { LockGuard_exclusive lock(&m_lock); currentTransmittingFrequency = val; }
	std::string getCurrentTransmittingSubtype() { LockGuard_shared lock(&m_lock); return currentTransmittingSubtype; }
	void setCurrentTransmittingSubtype(const std::string& val) { LockGuard_exclusive lock(&m_lock); currentTransmittingSubtype = val; }
	std::string getVehicleId() { LockGuard_shared lock(&m_lock); return vehicleId; }
	void setVehicleId(const std::string& val) { LockGuard_exclusive lock(&m_lock); vehicleId = val; }

	float effectiveDistanceTo(std::shared_ptr<clientData>& other);





	//Types that are small enough not to need locks
	bool pluginEnabled = false;
	std::chrono::system_clock::time_point pluginEnabledCheck;
	const TSClientID clientId;
	OVER_RADIO_TYPE currentTransmittingTangentOverType = OVER_RADIO_TYPE::LISTEN_TO_NONE; //Radio type client is currently transmitting on
	int voiceVolume = 0;  //#TODO request on PluginInit from clients in current channel
	int range = 0;

	bool canSpeak = true;
	bool clientTalkingNow = false;
	int dataFrame = 0;

	bool canUseSWRadio = false;
	bool canUseLRRadio = false;
	bool canUseDDRadio = false;

	int terrainInterception = 0;
	int objectInterception = 0;
	float voiceVolumeMultiplifier = 1.f;


	clientDataEffects effects;
private:
	using LockGuard_shared = LockGuard_shared<ReadWriteLock>;
	using LockGuard_exclusive = LockGuard_exclusive<ReadWriteLock>;
	ReadWriteLock m_lock;


	std::string nickname;

	Position3D clientPosition;
	Direction3D viewDirection;
    std::chrono::system_clock::time_point lastPositionUpdateTime;

	std::string currentTransmittingFrequency;//Frequency client is currently transmitting on
 	std::string currentTransmittingSubtype;//subtype client is currently transmitting on

	std::string vehicleId;



	
	

	

	
};

