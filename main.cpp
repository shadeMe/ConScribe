#include "ConScribeInternals.h"
#include "Commands.h"

#define									PLUGIN_VERSION	  8

IDebugLog								gLog			("ConScribe.log");
PluginHandle							g_pluginHandle	= kPluginHandle_Invalid;
OBSESerializationInterface*				g_serialization = NULL;
OBSEStringVarInterface*					g_stringvar		= NULL;
OBSEArrayVarInterface*					g_arrayIntfc	= NULL;
OBSEMessagingInterface*					g_msgIntfc		= NULL;


UInt32									g_SessionCount	= 0;
char*									g_HookMessage	= NULL;

ConScribeLog*							g_ConsoleLog	= NULL;
std::map<const char*, const char*>*		g_CSECommandMap = NULL;


// SERIALIZATION CALLBACKS

static void LoadCallbackHandler(void * reserved)
{
	g_SessionCount++;
	g_ConsoleLog->HandleLoadCallback();

	LogManager::GetSingleton()->DoLoadCallback(g_serialization);

	if (GET_INI_INT("LogBackups") == -1) {
		_MESSAGE("\nAppending log headers...\n");
		std::string LogDirectory(GET_INI_STRING("RootDirectory"));
		PerformHouseKeeping((LogDirectory + "ConScribe Logs\\Per-Mod\\").c_str(), "*.log", e_AppendHeaders);
		PerformHouseKeeping((LogDirectory + "ConScribe Logs\\Per-Script\\").c_str(), "*.log", e_AppendHeaders);
	}
}

static void SaveCallbackHandler(void * reserved)
{
	LogManager::GetSingleton()->DoSaveCallback(g_serialization);
}

static void NewGameCallbackHandler(void * reserved)
{	
	LogManager::GetSingleton()->PurgeDatabase();
}


// PLUGIN INTEROP

void ConScribeMessageHandler(OBSEMessagingInterface::Message* Msg)
{
	if (Msg->type == 'CSEL') {		
																				// populate map
		g_CSECommandMap = new std::map<const char*, const char*>;
		g_CSECommandMap->insert(std::make_pair<const char*, const char*>("Scribe", "http://cs.elderscrolls.com/constwiki/index.php/Scribe"));
		g_CSECommandMap->insert(std::make_pair<const char*, const char*>("RegisterLog", "http://cs.elderscrolls.com/constwiki/index.php/RegisterLog"));
		g_CSECommandMap->insert(std::make_pair<const char*, const char*>("UnregisterLog", "http://cs.elderscrolls.com/constwiki/index.php/UnregisterLog"));
		g_CSECommandMap->insert(std::make_pair<const char*, const char*>("GetRegisteredLogNames", "http://cs.elderscrolls.com/constwiki/index.php/GetRegisteredLogNames"));
		g_CSECommandMap->insert(std::make_pair<const char*, const char*>("ReadFromLog", "http://cs.elderscrolls.com/constwiki/index.php/ReadFromLog"));
		
																				// dispatch message
		g_msgIntfc->Dispatch(g_pluginHandle, 'CSEL', g_CSECommandMap, sizeof(g_CSECommandMap), "CSE");
		_MESSAGE("Received CSEL message - Dispatching command map");
	}
}


void OBSEMessageHandler(OBSEMessagingInterface::Message* Msg)
{
	if (Msg->type == OBSEMessagingInterface::kMessage_PostLoad) {
		g_msgIntfc->RegisterListener(g_pluginHandle, "CSE", ConScribeMessageHandler);	
		_MESSAGE("Registered to receive messages from CSE");
	}
}


//	HOUSEKEEPING & INIT

extern "C" {

bool OBSEPlugin_Query(const OBSEInterface * obse, PluginInfo * info)
{
	info->infoVersion = PluginInfo::kInfoVersion;
	info->name = "ConScribe";
	info->version = PLUGIN_VERSION;

	if(!obse->isEditor)
	{
		if(obse->obseVersion < OBSE_VERSION_INTEGER)
		{
			_MESSAGE("OBSE version too old (got %08X expected at least %08X)", obse->obseVersion, OBSE_VERSION_INTEGER);
			return false;
		}

		if(obse->oblivionVersion != OBLIVION_VERSION)
		{
			_MESSAGE("incorrect Oblivion version (got %08X need %08X)", obse->oblivionVersion, OBLIVION_VERSION);
			return false;
		}

		g_serialization = (OBSESerializationInterface *)obse->QueryInterface(kInterface_Serialization);
		if(!g_serialization)
		{
			_MESSAGE("serialization interface not found");
			return false;
		}

		if(g_serialization->version < OBSESerializationInterface::kVersion)
		{
			_MESSAGE("incorrect serialization version found (got %08X need %08X)", g_serialization->version, OBSESerializationInterface::kVersion);
			return false;
		}

		g_arrayIntfc = (OBSEArrayVarInterface*)obse->QueryInterface(kInterface_ArrayVar);
		if (!g_arrayIntfc)
		{
			_MESSAGE("Array interface not found");
			return false;
		}
	}

	return true;
}

bool OBSEPlugin_Load(const OBSEInterface * obse)
{
	g_pluginHandle = obse->GetPluginHandle();

	if(!obse->isEditor)
	{
		INIManager::GetSingleton()->Initialize(obse);

		if (!CreateLogDirectories())		return false;


		_MESSAGE("\nBacking up logs...\n");
		PerformHouseKeeping(std::string(std::string(GET_INI_STRING("RootDirectory")) + "ConScribe Logs\\Per-Script").c_str(), "*.log*", e_BackupLogs);
		PerformHouseKeeping(std::string(std::string(GET_INI_STRING("RootDirectory")) + "ConScribe Logs\\Per-Mod").c_str(), "*.log*", e_BackupLogs);
		PerformHouseKeeping(std::string(std::string(GET_INI_STRING("RootDirectory")) + "ConScribe Logs").c_str(), "Static Log.log*", e_BackupLogs);


		// CONSTRUCT CONSOLE LOG
		if (!_stricmp(GET_INI_STRING("ScribeMode"), "Static"))
			g_ConsoleLog = new ConsoleLog(std::string(std::string(GET_INI_STRING("RootDirectory")) + "ConScribe Logs\\Static Log.log").c_str(), ConScribeLog::e_Out);
		else
			g_ConsoleLog = new ConsoleLog(std::string(std::string(GET_INI_STRING("RootDirectory")) + "ConScribe Logs\\Log of " + GetTimeString() + ".log").c_str(), ConScribeLog::e_Out);


	
		// REGISTER CALLBACKS AND DO FIXUPS
		g_serialization->SetSaveCallback(g_pluginHandle, SaveCallbackHandler);
		g_serialization->SetLoadCallback(g_pluginHandle, LoadCallbackHandler);
		g_serialization->SetNewGameCallback(g_pluginHandle, NewGameCallbackHandler);

		g_stringvar = (OBSEStringVarInterface*)obse->QueryInterface(kInterface_StringVar);
		RegisterStringVarInterface(g_stringvar);

		WriteRelJump(kConsolePrintHookAddr, (UInt32)ConsolePrintHook); 

		_MESSAGE("\nINI Options:\n\tScribeMode = %s\n\tIncludes = %s\n\tExcludes = %s\n\tTimeFormat = %s\n\tLogBackups = %s\n\tRootDirectory = %s\n",
				GET_INI_STRING("ScribeMode"),
				GET_INI_STRING("Includes"),
				GET_INI_STRING("Excludes"),
				GET_INI_STRING("TimeFormat"),
				GET_INI_STRING("LogBackups"),
				GET_INI_STRING("RootDirectory"));
	} else {
		g_msgIntfc = (OBSEMessagingInterface*)obse->QueryInterface(kInterface_Messaging);
		g_msgIntfc->RegisterListener(g_pluginHandle, "OBSE", OBSEMessageHandler);
	}


	obse->SetOpcodeBase(0x25A0);													// 25A0h - 25AFh
	obse->RegisterCommand(&kCommandInfo_Scribe);
	obse->RegisterCommand(&kCommandInfo_RegisterLog);
	obse->RegisterCommand(&kCommandInfo_UnregisterLog);
	obse->RegisterTypedCommand(&kCommandInfo_GetRegisteredLogNames, kRetnType_Array);
	obse->RegisterTypedCommand(&kCommandInfo_ReadFromLog, kRetnType_Array);	

	_MESSAGE("ConScribe initialized!\n");
	return true;
}

};