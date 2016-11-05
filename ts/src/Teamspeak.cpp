#include "Teamspeak.hpp"
#include "public_errors.h"
#include "public_errors_rare.h"
#include "public_rare_definitions.h"
#include "ts3_functions.h"
#include <vector>
#include <windows.h>
#include "Logger.hpp"
#include "task_force_radio.hpp"
using namespace dataType;
struct TS3Functions ts3Functions;

TeamspeakServerData::TeamspeakServerData() {




}

TeamspeakServerData::~TeamspeakServerData() {



}

std::vector<dataType::TSClientID> TeamspeakServerData::getMutedClients() {
	LockGuard_shared lock(&m_criticalSection);
	return mutedClients;
}

void TeamspeakServerData::setClientMuteStatus(TSClientID clientID, bool muted) {
	if (!clientID) return;
	LockGuard_exclusive lock(&m_criticalSection);
	mutedClients.erase(std::remove(mutedClients.begin(), mutedClients.end(), clientID), mutedClients.end());
	if (muted)
		mutedClients.push_back(clientID);
}

void TeamspeakServerData::clearMutedClients() {
	LockGuard_exclusive lock(&m_criticalSection);
	mutedClients.clear();
}

dataType::TSClientID TeamspeakServerData::getMyClientID() {
	LockGuard_shared lock(&m_criticalSection);
	return myClientID;
}

void TeamspeakServerData::setMyClientID(dataType::TSClientID val) {
	LockGuard_exclusive lock(&m_criticalSection);
	myClientID = val;
}

std::string TeamspeakServerData::getMyOriginalNickname() {
	LockGuard_shared lock(&m_criticalSection);
	return myOriginalNickname;
}

void TeamspeakServerData::setMyOriginalNickname(std::string val) {
	LockGuard_exclusive lock(&m_criticalSection);
	myOriginalNickname = val;
}

dataType::TSChannelID TeamspeakServerData::getMyOriginalChannel() {
	LockGuard_shared lock(&m_criticalSection);
	return myOriginalChannel;
}

void TeamspeakServerData::setMyOriginalChannel(TSChannelID val) {
	LockGuard_exclusive lock(&m_criticalSection);
	myOriginalChannel = val;
}

Teamspeak::Teamspeak() {}


Teamspeak::~Teamspeak() {}

dataType::TSServerID Teamspeak::getCurrentServerConnection() {
	return ts3Functions.getCurrentServerConnectionHandlerID();
}

Teamspeak& Teamspeak::getInstance() {
	static Teamspeak instance;
	return instance;
}

void Teamspeak::unmuteAll(TSServerID serverConnectionHandlerID) {
	anyID* ids;
	DWORD error;
#ifdef unmuteAllClients
	if ((error = ts3Functions.getClientList(serverConnectionHandlerID, &ids)) != ERROR_ok) {
		log("Error getting all clients from server", error);
		return;
}
#else
	std::vector<TSClientID> mutedClients = getInstance().serverData[serverConnectionHandlerID].getMutedClients();
	mutedClients.push_back(0);//Null-terminate so we can send it to requestUnmuteClients
	ids = reinterpret_cast<anyID*>(mutedClients.data());
#endif
	//Or add a list of muted clients to server_radio_data and only unmute them and also call unmuteAll as soon as Arma disconnects from TS
	if ((error = ts3Functions.requestUnmuteClients(serverConnectionHandlerID, ids, NULL)) != ERROR_ok) {
		log("Can't unmute all clients", error);
	}
#ifdef unmuteAllClients
	ts3Functions.freeMemory(ids);
#else
	getInstance().serverData[serverConnectionHandlerID].clearMutedClients();
#endif
	}

void Teamspeak::setClientMute(TSServerID serverConnectionHandlerID, TSClientID clientID, bool mute) {
	if (!clientID)		return;
	anyID clientIds[2];
	clientIds[0] = clientID;
	clientIds[1] = 0;
	getInstance().serverData[serverConnectionHandlerID].setClientMuteStatus(clientID, mute);

	DWORD error;
	if (mute) {
		if ((error = ts3Functions.requestMuteClients(serverConnectionHandlerID, clientIds, NULL)) != ERROR_ok) {
			log("Can't mute client", error);
		}
	} else {
		if ((error = ts3Functions.requestUnmuteClients(serverConnectionHandlerID, clientIds, NULL)) != ERROR_ok) {
			log("Can't unmute client", error);
		}
	}
}

void Teamspeak::moveToSeriousChannel(TSServerID serverConnectionHandlerID) {
	auto seriousChannelID = findChannelByName(serverConnectionHandlerID, TFAR::config.get<std::string>(Setting::serious_channelName));
	if (!seriousChannelID) //Channel not found
		return;
	auto currentChannel = getCurrentChannel(serverConnectionHandlerID);
	if (currentChannel == seriousChannelID)
		return;
	getInstance().serverData[serverConnectionHandlerID].setMyOriginalChannel(currentChannel);
	std::string seriousChannelPassword = TFAR::config.get<std::string>(Setting::serious_channelPassword);
	DWORD error;
	if ((error = ts3Functions.requestClientMove(serverConnectionHandlerID, getMyId(serverConnectionHandlerID), seriousChannelID, seriousChannelPassword.c_str(), nullptr)) != ERROR_ok) {
		log("Can't join channel", error);
	}
}

void Teamspeak::moveFromSeriousChannel(TSServerID serverConnectionHandlerID) {
	TSChannelID notSeriousChannelId = getInstance().serverData[serverConnectionHandlerID].getMyOriginalChannel();
	if (!notSeriousChannelId)
		return;


	if (getCurrentChannel(serverConnectionHandlerID) == notSeriousChannelId) return;
	DWORD error;
	if ((error = ts3Functions.requestClientMove(serverConnectionHandlerID, getInstance().serverData[serverConnectionHandlerID].getMyClientID(), notSeriousChannelId, "", NULL)) != ERROR_ok) {
		log("Can't join back channel", error);
	}

	getInstance().serverData[serverConnectionHandlerID].setMyOriginalChannel(-1);
}

void Teamspeak::setMyNicknameToGameName(TSServerID serverConnectionHandlerID, const std::string& nickname) {
	if (getMyNickname(serverConnectionHandlerID) != getInstance().serverData[serverConnectionHandlerID].getMyOriginalNickname())
		getInstance().serverData[serverConnectionHandlerID].setMyOriginalNickname(getMyNickname(serverConnectionHandlerID));

	DWORD error;
	if ((error = ts3Functions.setClientSelfVariableAsString(serverConnectionHandlerID, CLIENT_NICKNAME, nickname.c_str())) != ERROR_ok) {
		log("Error setting client nickname", error);
	}
	ts3Functions.flushClientSelfUpdates(ts3Functions.getCurrentServerConnectionHandlerID(), NULL);
}

void Teamspeak::resetMyNickname(TSServerID serverConnectionHandlerID) {
	std::string origNickname = getInstance().serverData[serverConnectionHandlerID].getMyOriginalNickname();
	if (origNickname.empty())
		return;
	DWORD error;
	if ((error = ts3Functions.setClientSelfVariableAsString(serverConnectionHandlerID, CLIENT_NICKNAME, origNickname.c_str())) != ERROR_ok) {
		log("Error resetting client nickname", error);
	} else {
		getInstance().serverData[serverConnectionHandlerID].setMyOriginalNickname("");
	}
	ts3Functions.flushClientSelfUpdates(ts3Functions.getCurrentServerConnectionHandlerID(), NULL);
}

void Teamspeak::_onConnectStatusChangeEvent(TSServerID serverConnectionHandlerID, ConnectStatus newState) {

	/* Some example code following to show how to use the information query functions. */
	unsigned int errorCode;
	if (newState == STATUS_CONNECTION_ESTABLISHED) {
		if (TFAR::getInstance().getCurrentlyInGame())
			moveToSeriousChannel(serverConnectionHandlerID);	//rejoin channel at server reconnect. If still ingame and channelswitch enabled

		// Set system 3d settings
		errorCode = ts3Functions.systemset3DSettings(serverConnectionHandlerID, 1.0f, 1.0f);
		if (errorCode != ERROR_ok) {
			log("Failed to set 3d settings", errorCode);
		}
		TFAR::getInstance().onTeamspeakServerConnect(serverConnectionHandlerID);
		_onChannelSwitchedEvent(serverConnectionHandlerID, getCurrentChannel(serverConnectionHandlerID));//Calls onClientJoined for every client in channel

		//Directory has to exist.. It should be added in onTeamspeakServerConnect
		auto clientDataDir = TFAR::getServerDataDirectory()->getClientDataDirectory(serverConnectionHandlerID);

		//set our clientData ptr
		clientDataDir->myClientData = clientDataDir->getClientData(getMyId(serverConnectionHandlerID));

	} else if (newState == STATUS_DISCONNECTED) {
		TFAR::getInstance().onTeamspeakServerDisconnect(serverConnectionHandlerID);
		getInstance().serverData.erase(serverConnectionHandlerID);
	}
}

void Teamspeak::_onChannelSwitchedEvent(TSServerID serverConnectionHandlerID, TSChannelID newChannel) {
	if (getInstance().serverData[serverConnectionHandlerID].myLastKnownChannel == newChannel)
		return;
	getInstance().serverData[serverConnectionHandlerID].myLastKnownChannel = newChannel;

	TFAR::getInstance().onTeamspeakClientLeft(serverConnectionHandlerID, -2);//Switching channel and all clients already being in new channel is not possible

	std::vector<TSClientID> clients = getChannelClients(serverConnectionHandlerID, newChannel);
	for (anyID clientId : clients) {
		std::string clientNickname = getClientNickname(serverConnectionHandlerID, clientId);
		if (clientNickname.empty()) continue;
		TFAR::getInstance().onTeamspeakClientJoined(serverConnectionHandlerID, clientId, clientNickname);
	}
}

void Teamspeak::_onClientMoved(TSServerID serverConnectionHandlerID, TSClientID clientID, TSChannelID oldChannel, TSChannelID newChannel) {
	if (clientID == getMyId(serverConnectionHandlerID)) {// we switched channel
		_onChannelSwitchedEvent(serverConnectionHandlerID, newChannel);
		return;
	}

	if (getInstance().serverData[serverConnectionHandlerID].myLastKnownChannel == newChannel) {
		std::string clientNickname = getClientNickname(serverConnectionHandlerID, clientID);
		if (clientNickname.empty()) return;
		TFAR::getInstance().onTeamspeakClientJoined(serverConnectionHandlerID, clientID, clientNickname);
	} else if (getInstance().serverData[serverConnectionHandlerID].myLastKnownChannel == oldChannel) {
		TFAR::getInstance().onTeamspeakClientLeft(serverConnectionHandlerID, clientID);
	}
}

void Teamspeak::_onClientJoined(TSServerID serverConnectionHandlerID, TSClientID clientID, TSChannelID channel) {
	if (getInstance().serverData[serverConnectionHandlerID].myLastKnownChannel != channel)
		return;

	std::string clientNickname = getClientNickname(serverConnectionHandlerID, clientID);
	if (clientNickname.empty()) return;
	TFAR::getInstance().onTeamspeakClientJoined(serverConnectionHandlerID, clientID, clientNickname);
}

void Teamspeak::_onClientLeft(TSServerID serverConnectionHandlerID, TSClientID clientID) {
	TFAR::getInstance().onTeamspeakClientLeft(serverConnectionHandlerID, clientID);
}

void Teamspeak::_onClientUpdated(TSServerID serverConnectionHandlerID, TSClientID clientID) {
	std::string clientNickname = getClientNickname(serverConnectionHandlerID, clientID);
	if (clientNickname.empty()) return;
	TFAR::getInstance().onTeamspeakClientUpdated(serverConnectionHandlerID, clientID, clientNickname);
}

void Teamspeak::_onInit() {
	//Called on pluginInit. Should check what servers we are connected to and cause according events



	std::vector<TSServerID> connectedServers;
	uint64* servers = nullptr;
	if (ts3Functions.getServerConnectionHandlerList(&servers) == ERROR_ok) {
		int i = 0;
		while (servers[i]) {
			connectedServers.push_back(servers[i]);
			i++;
		}
		ts3Functions.freeMemory(servers);
	}
	for (TSServerID server : connectedServers) {
		_onConnectStatusChangeEvent(server, STATUS_CONNECTION_ESTABLISHED);
	}




}

void Teamspeak::sendPluginCommand(TSServerID serverConnectionHandlerID, const std::string& pluginID, const std::string& command, PluginTargetMode targetMode, std::vector<TSClientID> targets) {
	if (targets.empty())
		ts3Functions.sendPluginCommand(serverConnectionHandlerID, pluginID.c_str(), command.c_str(), targetMode, NULL, NULL);
	else {
		targets.push_back(0);
		ts3Functions.sendPluginCommand(serverConnectionHandlerID, pluginID.c_str(), command.c_str(), targetMode, reinterpret_cast<anyID*>(targets.data()), NULL);
	}
}

void Teamspeak::playWavFile(const std::string& filePath) {
	DWORD error;
	if ((error = ts3Functions.playWaveFile(getCurrentServerConnection(), filePath.c_str())) != ERROR_ok) {
		log("can't play sound", error, LogLevel_ERROR);
	}
}

void Teamspeak::setVoiceDisabled(TSServerID serverConnectionHandlerID, bool disabled) {
	DWORD error;
	if ((error = ts3Functions.setClientSelfVariableAsInt(serverConnectionHandlerID, CLIENT_INPUT_DEACTIVATED, disabled ? 1 : 0)) != ERROR_ok) {
		log("Can't active talking by tangent", error);
	}
	error = ts3Functions.flushClientSelfUpdates(serverConnectionHandlerID, NULL);
	if (error != ERROR_ok && error != ERROR_ok_no_update) {
		log("Can't flush self updates", error);
	}
}

bool Teamspeak::hlp_checkVad() {
	char* vad; // Is "true" or "false"
	DWORD error;
	if ((error = ts3Functions.getPreProcessorConfigValue(getCurrentServerConnection(), "vad", &vad)) == ERROR_ok) {
		bool result = strcmp(vad, "true") == 0;
		ts3Functions.freeMemory(vad);
		return result;
	} else {
		log("Failed to get VAD value", error);
		return false;
	}
}

void Teamspeak::hlp_enableVad() {
	DWORD error;
	if ((error = ts3Functions.setPreProcessorConfigValue(getCurrentServerConnection(), "vad", "true")) != ERROR_ok) {
		log("Failed to set VAD value", error);
	}
}

void Teamspeak::hlp_disableVad() {
	DWORD error;
	if ((error = ts3Functions.setPreProcessorConfigValue(getCurrentServerConnection(), "vad", "false")) != ERROR_ok) {
		log("Failure disabling VAD", error);
	}
}

void Teamspeak::log(std::string message, DWORD errorCode, LogLevel level) {
	char* errorBuffer;
	ts3Functions.getErrorMessage(errorCode, &errorBuffer);
	std::string output = std::string(message) + std::string(" : ") + std::string(errorBuffer);
	ts3Functions.freeMemory(errorBuffer);
	Logger::log(LoggerTypes::teamspeakClientlog, output, level);//Default loglevel is Info
}


bool Teamspeak::isConnected(TSServerID serverConnectionHandlerID) {
	int result;
	if (ts3Functions.getConnectionStatus(serverConnectionHandlerID, &result) != ERROR_ok) {
		return false;
	}
	return result != 0;
}

TSClientID Teamspeak::getMyId(TSServerID serverConnectionHandlerID) {
	auto myID = getInstance().serverData[serverConnectionHandlerID].getMyClientID();
	if (myID) return myID;

	if (!isConnected(serverConnectionHandlerID)) return myID;
	DWORD error;
	if ((error = ts3Functions.getClientID(serverConnectionHandlerID, reinterpret_cast<anyID*>(&myID))) != ERROR_ok) {
		log("Failure getting client ID", error);
	}
	getInstance().serverData[serverConnectionHandlerID].setMyClientID(myID);
	return myID;
}

bool Teamspeak::isInChannel(TSServerID serverConnectionHandlerID, TSClientID clientId, const char* channelToCheck) { //#TODO std::string.. wtf char*?!
	return getChannelName(serverConnectionHandlerID, clientId) == channelToCheck;
}

std::string Teamspeak::getChannelName(TSServerID serverConnectionHandlerID, TSClientID clientId) {
	if (!clientId) return "";
	uint64 channelId;
	DWORD error;
	if ((error = ts3Functions.getChannelOfClient(serverConnectionHandlerID, clientId, &channelId)) != ERROR_ok) {
		if (error != ERROR_client_invalid_id) //can happen if client disconnected while playing
			log("Can't get channel of client", error);
		return "";
	}
	char* channelName;
	if ((error = ts3Functions.getChannelVariableAsString(serverConnectionHandlerID, channelId, CHANNEL_NAME, &channelName)) != ERROR_ok) {
		log("Can't get channel name", error);
		return "";
	}
	const std::string result(channelName);
	ts3Functions.freeMemory(channelName);
	return result;
}

std::string Teamspeak::getServerName(TSServerID serverConnectionHandlerID) {
	char* name;
	DWORD error = ts3Functions.getServerVariableAsString(serverConnectionHandlerID, VIRTUALSERVER_NAME, &name);
	if (error != ERROR_ok) {
		log("Can't get server name", error, LogLevel_ERROR);
		return "ERROR_GETTING_SERVER_NAME";
	} else {
		std::string result(name);
		ts3Functions.freeMemory(name);
		return result;
	}
}

bool Teamspeak::isTalking(TSServerID currentServerConnectionHandlerID, TSClientID playerId) {
	int result = 0;
	DWORD error;
	if (playerId == getMyId(currentServerConnectionHandlerID)) {
		if ((error = ts3Functions.getClientSelfVariableAsInt(currentServerConnectionHandlerID, CLIENT_FLAG_TALKING, &result)) != ERROR_ok) {
			log("Can't get talking status", error);
		}
	} else {
		if ((error = ts3Functions.getClientVariableAsInt(currentServerConnectionHandlerID, playerId, CLIENT_FLAG_TALKING, &result)) != ERROR_ok) {
			log("Can't get talking status", error);
		}
	}
	return result != 0;
}

std::vector<TSClientID> Teamspeak::getChannelClients(TSServerID serverConnectionHandlerID, TSChannelID channelId) {
	std::vector<TSClientID> result;
	anyID* clients = nullptr;
	if (ts3Functions.getChannelClientList(serverConnectionHandlerID, channelId, &clients) == ERROR_ok) {
		int i = 0;
		while (clients[i]) {
			result.push_back(clients[i]);
			i++;
		}
		ts3Functions.freeMemory(clients);
	}
	return result;
}

TSChannelID Teamspeak::getCurrentChannel(TSServerID serverConnectionHandlerID) {
	uint64 channelId;
	DWORD error;
	if ((error = ts3Functions.getChannelOfClient(serverConnectionHandlerID, getMyId(serverConnectionHandlerID), &channelId)) != ERROR_ok) {
		log("Can't get current channel", error);
	}
	return channelId;
}

std::string Teamspeak::getMyNickname(TSServerID serverConnectionHandlerID) {
	char* bufferForNickname;
	DWORD error;
	anyID myId = getMyId(serverConnectionHandlerID);
	if (myId == anyID(-1)) return "";
	if ((error = ts3Functions.getClientVariableAsString(serverConnectionHandlerID, myId, CLIENT_NICKNAME, &bufferForNickname)) != ERROR_ok) {
		log("Error getting client nickname", error, LogLevel_DEBUG);
		return "";
	}
	std::string result(bufferForNickname);
	ts3Functions.freeMemory(bufferForNickname);
	return result;
}

bool Teamspeak::setMyNickname(TSServerID serverConnectionHandlerID, const std::string& nickname) {
	DWORD error;
	if ((error = ts3Functions.setClientSelfVariableAsString(serverConnectionHandlerID, CLIENT_NICKNAME, nickname.c_str())) != ERROR_ok) {
		log("Error setting client nickname", error);
		return false;
	}

	ts3Functions.flushClientSelfUpdates(serverConnectionHandlerID, NULL);
	return true;
}

TSChannelID Teamspeak::findChannelByName(TSServerID serverConnectionHandlerID, const std::string& wantedChannelName) {
	if (wantedChannelName.empty())
		return -1;
	DWORD error;
	TSChannelID* result;
	//#TODO cache channel names. Cache will get invalidated if a channel is created/moved/deleted
	if ((error = ts3Functions.getChannelList(serverConnectionHandlerID, reinterpret_cast<uint64**>(&result))) != ERROR_ok) {
		log("Can't get channel list", error);
	} else {
		bool joined = false;
		TSChannelID* iter = result;
		while (*iter && !joined) {
			uint64 channelId = *iter;
			iter++;
			char* curChannelName;
			if ((error = ts3Functions.getChannelVariableAsString(serverConnectionHandlerID, channelId, CHANNEL_NAME, &curChannelName)) != ERROR_ok) {
				log("Can't get channel name", error);
			} else {
				if (!strcmp(wantedChannelName.c_str(), curChannelName)) {
					ts3Functions.freeMemory(curChannelName);
					return channelId;
				}
				ts3Functions.freeMemory(curChannelName);
			}
		}
		ts3Functions.freeMemory(result);
	}
	return -1;
}

std::string Teamspeak::getMetaData(TSServerID serverConnectionHandlerID, TSClientID clientId) {
	std::string result;
	char* clientInfo;
	DWORD error;
	if ((error = ts3Functions.getClientVariableAsString(serverConnectionHandlerID, clientId, CLIENT_META_DATA, &clientInfo)) != ERROR_ok) {
		log("Can't get client metadata", error);
		return "";
	} else {
		std::string sharedMsg(clientInfo);
		if (sharedMsg.find(START_DATA) == std::string::npos || sharedMsg.find(END_DATA) == std::string::npos) {
			result = "";
		} else {
			result = sharedMsg.substr(sharedMsg.find(START_DATA) + strlen(START_DATA), sharedMsg.find(END_DATA) - sharedMsg.find(START_DATA) - strlen(START_DATA));
		}
		ts3Functions.freeMemory(clientInfo);
		return result;
	}
}

void Teamspeak::setMyMetaData(const std::string & metaData) {
	char* clientInfo;
	DWORD error;
	if ((error = ts3Functions.getClientVariableAsString(getCurrentServerConnection(), getMyId(ts3Functions.getCurrentServerConnectionHandlerID()), CLIENT_META_DATA, &clientInfo)) != ERROR_ok) {
		log("setMetaData - Can't get client metadata", error);
	} else {
		std::string to_set;
		std::string sharedMsg = clientInfo;
		if (sharedMsg.find(START_DATA) == std::string::npos || sharedMsg.find(END_DATA) == std::string::npos) {
			to_set = to_set + START_DATA + metaData + END_DATA;
		} else {	//Only set stuff between TFAR tags
			std::string before = sharedMsg.substr(0, sharedMsg.find(START_DATA));
			std::string after = sharedMsg.substr(sharedMsg.find(END_DATA) + strlen(END_DATA), std::string::npos);
			to_set = before + START_DATA + metaData + END_DATA + after;
		}
		if ((error = ts3Functions.setClientSelfVariableAsString(ts3Functions.getCurrentServerConnectionHandlerID(), CLIENT_META_DATA, to_set.c_str())) != ERROR_ok) {
			log("setMetaData - Can't set own META_DATA", error);
		}
		ts3Functions.freeMemory(clientInfo);
	}
	ts3Functions.flushClientSelfUpdates(ts3Functions.getCurrentServerConnectionHandlerID(), NULL);
}

std::string Teamspeak::getClientNickname(TSServerID serverConnectionHandlerID, TSClientID clientId) {
	DWORD error;
	char* name;
	if ((error = ts3Functions.getClientVariableAsString(serverConnectionHandlerID, clientId, CLIENT_NICKNAME, &name)) != ERROR_ok) {
		log("Error getting client nickname", error);
	} else {
		std::string nameStr(name);
		ts3Functions.freeMemory(name);
		return nameStr;
	}
	return "";
}

void Teamspeak::setMyClient3DPosition(TSServerID serverConnectionHandlerID, Position3D pos) {

	DWORD error;
	if ((error = ts3Functions.systemset3DListenerAttributes(serverConnectionHandlerID, pos, NULL, NULL)) != ERROR_ok) {
		log("can't center listener", error);
	}
}

void Teamspeak::setClient3DPosition(TSServerID serverConnectionHandlerID, TSClientID clientId, Position3D pos) {
	DWORD error;
	if ((error = ts3Functions.channelset3DAttributes(serverConnectionHandlerID, clientId, Position3D())) != ERROR_ok) {
		//We don't really care.. so don't spam our users
		//if (error != ERROR_client_invalid_id) //can happen if client disconnected while playing
		//	log("can't center client", error);
	}
}






#include "plugin.h"
/* Set TeamSpeak 3 callback functions */
void ts3plugin_setFunctionPointers(const struct TS3Functions funcs) {
	ts3Functions = funcs;
}


/****************************** Optional functions ********************************/
/*
* Following functions are optional, if not needed you don't need to implement them.
*/

/* Tell client if plugin offers a configuration window. If this function is not implemented, it's an assumed "does not offer" (PLUGIN_OFFERS_NO_CONFIGURE). */
int ts3plugin_offersConfigure() {
	/*
	* Return values:
	* PLUGIN_OFFERS_NO_CONFIGURE         - Plugin does not implement ts3plugin_configure
	* PLUGIN_OFFERS_CONFIGURE_NEW_THREAD - Plugin does implement ts3plugin_configure and requests to run this function in an own thread
	* PLUGIN_OFFERS_CONFIGURE_QT_THREAD  - Plugin does implement ts3plugin_configure and requests to run this function in the Qt GUI thread
	*/
	return PLUGIN_OFFERS_NO_CONFIGURE;  /* In this case ts3plugin_configure does not need to be implemented */
}
/*
* If the plugin wants to use error return codes, plugin commands, hotkeys or menu items, it needs to register a command ID. This function will be
* automatically called after the plugin was initialized. This function is optional. If you don't use these features, this function can be omitted.
* Note the passed pluginID parameter is no longer valid after calling this function, so you must copy it and store it in the plugin.
*/
void ts3plugin_registerPluginID(const char* id) {
	TFAR::getInstance().setPluginID(id);

	std::string message = std::string("registerPluginID: ") + std::string(id);
	Logger::log(LoggerTypes::teamspeakClientlog, message, LogLevel_INFO);
}

/* Plugin command keyword. Return NULL or "" if not used. */
const char* ts3plugin_commandKeyword() {
	return "";
}

/* Plugin processes console command. Return 0 if plugin handled the command, 1 if not handled. */
int ts3plugin_processCommand(uint64 serverConnectionHandlerID, const char* command) {
	return 0;  /* Plugin handled command */
}

/* Client changed current server connection handler */
void ts3plugin_currentServerConnectionChanged(uint64 serverConnectionHandlerID) {}

/*
* Implement the following three functions when the plugin should display a line in the server/channel/client info.
* If any of ts3plugin_infoTitle, ts3plugin_infoData or ts3plugin_freeMemory is missing, the info text will not be displayed.
*/

/* Static title shown in the left column in the info frame */
const char* ts3plugin_infoTitle() {
	std::string info = std::string("Task Force Radio Status (") + PLUGIN_VERSION + ")";
	size_t maxLen = info.length() + 1;
	char* result = static_cast<char*>(malloc(maxLen * sizeof(char)));
	memset(result, 0, maxLen);
	strncpy_s(result, maxLen, info.c_str(), info.length());
	return result;
}

/* Required to release the memory for parameter "data" allocated in ts3plugin_infoData and ts3plugin_initMenus */
void ts3plugin_freeMemory(void* data) {
	free(data);
}

/*
* Plugin requests to be always automatically loaded by the TeamSpeak 3 client unless
* the user manually disabled it in the plugin dialog.
* This function is optional. If missing, no autoload is assumed.
*/
int ts3plugin_requestAutoload() {
	return 1;  /* 1 = request autoloaded, 0 = do not request autoload */
}

int ts3plugin_onServerErrorEvent(uint64 serverConnectionHandlerID, const char* errorMessage, unsigned int error, const char* returnCode, const char* extraMessage) {
	if (returnCode) {
		/* A plugin could now check the returnCode with previously (when calling a function) remembered returnCodes and react accordingly */
		/* In case of using a a plugin return code, the plugin can return:
		* 0: Client will continue handling this error (print to chat tab)
		* 1: Client will ignore this error, the plugin announces it has handled it */
		return 1;
	}
	return 0;  /* If no plugin return code was used, the return value of this function is ignored */
}

int ts3plugin_onServerPermissionErrorEvent(uint64 serverConnectionHandlerID, const char* errorMessage, unsigned int error, const char* returnCode, unsigned int failedPermissionID) {
	return 0;  /* See onServerErrorEvent for return code description */
}



void ts3plugin_onConnectStatusChangeEvent(uint64 serverConnectionHandlerID, int newStatus, unsigned int errorNumber) {
	Logger::log(LoggerTypes::pluginCommands, "ts3plugin_onConnectStatusChangeEvent"); //#TODO remove logging on release
	Teamspeak::_onConnectStatusChangeEvent(serverConnectionHandlerID, static_cast<ConnectStatus>(newStatus));
}

void ts3plugin_onUpdateClientEvent(uint64 serverConnectionHandlerID, anyID clientID, anyID invokerID, const char* invokerName, const char* invokerUniqueIdentifier) {
	Logger::log(LoggerTypes::pluginCommands, "ts3plugin_onUpdateClientEvent");
	Teamspeak::_onClientUpdated(serverConnectionHandlerID, clientID);
}

void ts3plugin_onClientMoveEvent(uint64 serverConnectionHandlerID, anyID clientID, uint64 oldChannelID, uint64 newChannelID, int visibility, const char* moveMessage) {
	Logger::log(LoggerTypes::pluginCommands, "ts3plugin_onClientMoveEvent");
	Teamspeak::_onClientMoved(serverConnectionHandlerID, clientID, oldChannelID, newChannelID);
}

void ts3plugin_onClientMoveTimeoutEvent(uint64 serverConnectionHandlerID, anyID clientID, uint64 oldChannelID, uint64 newChannelID, int visibility, const char* timeoutMessage) {
	Logger::log(LoggerTypes::pluginCommands, "ts3plugin_onClientMoveTimeoutEvent");
	Teamspeak::_onClientMoved(serverConnectionHandlerID, clientID, oldChannelID, newChannelID);
}

void ts3plugin_onClientMoveMovedEvent(uint64 serverConnectionHandlerID, anyID clientID, uint64 oldChannelID, uint64 newChannelID, int visibility, anyID moverID, const char* moverName, const char* moverUniqueIdentifier, const char* moveMessage) {
	Logger::log(LoggerTypes::pluginCommands, "ts3plugin_onClientMoveMovedEvent");
	Teamspeak::_onClientMoved(serverConnectionHandlerID, clientID, oldChannelID, newChannelID);
}

void ts3plugin_onClientKickFromChannelEvent(uint64 serverConnectionHandlerID, anyID clientID, uint64 oldChannelID, uint64 newChannelID, int visibility, anyID kickerID, const char* kickerName, const char* kickerUniqueIdentifier, const char* kickMessage) {
	Logger::log(LoggerTypes::pluginCommands, "ts3plugin_onClientKickFromChannelEvent");
	Teamspeak::_onClientMoved(serverConnectionHandlerID, clientID, oldChannelID, newChannelID);
}

void ts3plugin_onClientKickFromServerEvent(uint64 serverConnectionHandlerID, anyID clientID, uint64 oldChannelID, uint64 newChannelID, int visibility, anyID kickerID, const char* kickerName, const char* kickerUniqueIdentifier, const char* kickMessage) {
	Logger::log(LoggerTypes::pluginCommands, "ts3plugin_onClientKickFromServerEvent");
	Teamspeak::_onClientLeft(serverConnectionHandlerID, clientID);
}

void ts3plugin_onClientBanFromServerEvent(uint64 serverConnectionHandlerID, anyID clientID, uint64 oldChannelID, uint64 newChannelID, int visibility, anyID kickerID, const char* kickerName, const char* kickerUniqueIdentifier, uint64 time, const char* kickMessage) {
	Logger::log(LoggerTypes::pluginCommands, "ts3plugin_onClientBanFromServerEvent");
	Teamspeak::_onClientLeft(serverConnectionHandlerID, clientID);
}