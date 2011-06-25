#include "ConScribeInternals.h"
#include "Commands.h"


#define									PLUGIN_VERSION	  10

IDebugLog								gLog			("ConScribe.log");
PluginHandle							g_pluginHandle	= kPluginHandle_Invalid;
OBSESerializationInterface*				g_serialization = NULL;
OBSEStringVarInterface*					g_stringvar		= NULL;
OBSEArrayVarInterface*					g_arrayIntfc	= NULL;
OBSEMessagingInterface*					g_msgIntfc		= NULL;
OBSEScriptInterface*					g_OBSEScriptInterface = NULL;
CSEIntelliSenseInterface*				g_CSEISIntfc	= NULL;
CSEConsoleInterface*					g_CSEConsoleIntfc	= NULL;


UInt32									g_SessionCount	= 0;
const char*								g_HookMessage	= NULL;

ConScribeLog*							g_ConsoleLog	= NULL;


// SERIALIZATION CALLBACKS

static void LoadCallbackHandler(void * reserved)
{
	g_SessionCount++;
	g_ConsoleLog->HandleLoadCallback();

	LogManager::GetSingleton()->DoLoadCallback(g_serialization);

	if (g_INIManager->GetINIInt("LogBackups") == -1) {
		_MESSAGE("\nAppending log headers...\n");
		std::string LogDirectory(g_INIManager->GetINIStr("RootDirectory"));
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
	if (Msg->type == 'CSEI')
	{
		CSEInterface* Interface = (CSEInterface*)Msg->data;

		g_CSEConsoleIntfc = (CSEConsoleInterface*)Interface->InitializeInterface(CSEInterface::kCSEInterface_Console);
		g_CSEISIntfc = (CSEIntelliSenseInterface*)Interface->InitializeInterface(CSEInterface::kCSEInterface_IntelliSense);

		_MESSAGE("Received interface from CSE");

		g_CSEConsoleIntfc->PrintToConsole("ConScribe", "Registering command URLs ...");
		g_CSEISIntfc->RegisterCommandURL("Scribe", "http://cs.elderscrolls.com/constwiki/index.php/Scribe");
		g_CSEISIntfc->RegisterCommandURL("RegisterLog", "http://cs.elderscrolls.com/constwiki/index.php/RegisterLog");
		g_CSEISIntfc->RegisterCommandURL("UnregisterLog", "http://cs.elderscrolls.com/constwiki/index.php/UnregisterLog");
		g_CSEISIntfc->RegisterCommandURL("GetRegisteredLogNames", "http://cs.elderscrolls.com/constwiki/index.php/GetRegisteredLogNames");
		g_CSEISIntfc->RegisterCommandURL("ReadFromLog", "http://cs.elderscrolls.com/constwiki/index.php/ReadFromLog");
		g_CSEISIntfc->RegisterCommandURL("GetLogLineCount", "http://cs.elderscrolls.com/constwiki/index.php/GetLogLineCount");
		g_CSEISIntfc->RegisterCommandURL("DeleteLinesFromLog", "http://cs.elderscrolls.com/constwiki/index.php/DeleteLinesFromLog");
	}
}


void OBSEMessageHandler(OBSEMessagingInterface::Message* Msg)
{
	switch (Msg->type)
	{
	case OBSEMessagingInterface::kMessage_PostLoad:
		g_msgIntfc->RegisterListener(g_pluginHandle, "CSE", ConScribeMessageHandler);
		_MESSAGE("Registered to receive messages from CSE");
		break;
	case OBSEMessagingInterface::kMessage_PostPostLoad:
		_MESSAGE("Requesting an interface from CSE");
		g_msgIntfc->Dispatch(g_pluginHandle, 'CSEI', NULL, 0, "CSE");	
		break;
	}
}

//	HOUSEKEEPING & INIT

extern "C"
{

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
		g_OBSEScriptInterface = (OBSEScriptInterface*)obse->QueryInterface(kInterface_Script);
		if (!g_OBSEScriptInterface)
		{
			_MESSAGE("Script interface not found");
			return false;
		}
	}

	return true;
}

bool OBSEPlugin_Load(const OBSEInterface * obse)
{
	g_pluginHandle = obse->GetPluginHandle();

	g_INIManager->SetINIPath(std::string(obse->GetOblivionDirectory()) + "Data\\OBSE\\Plugins\\ConScribe.ini");
	g_INIManager->Initialize();

	if(!obse->isEditor)
	{
		if (!_stricmp(g_INIManager->GetINIStr("RootDirectory"), "Default"))
		{
			g_INIManager->GetINI("RootDirectory")->SetValue(std::string(std::string(obse->GetOblivionDirectory()) + "Data\\").c_str());
			_MESSAGE("Root set to default directory");
		}

		if (!CreateLogDirectories())
			return false;


		_MESSAGE("\nBacking up logs...\n");
		PerformHouseKeeping(std::string(std::string(g_INIManager->GetINIStr("RootDirectory")) + "ConScribe Logs\\Per-Script").c_str(), "*.log*", e_BackupLogs);
		PerformHouseKeeping(std::string(std::string(g_INIManager->GetINIStr("RootDirectory")) + "ConScribe Logs\\Per-Mod").c_str(), "*.log*", e_BackupLogs);
		PerformHouseKeeping(std::string(std::string(g_INIManager->GetINIStr("RootDirectory")) + "ConScribe Logs").c_str(), "Static Log.log*", e_BackupLogs);


		// CONSTRUCT CONSOLE LOG
		if (!_stricmp(g_INIManager->GetINIStr("ScribeMode"), "Static"))
			g_ConsoleLog = new ConsoleLog(std::string(std::string(g_INIManager->GetINIStr("RootDirectory")) + "ConScribe Logs\\Static Log.log").c_str(), ConScribeLog::e_Out);
		else
			g_ConsoleLog = new ConsoleLog(std::string(std::string(g_INIManager->GetINIStr("RootDirectory")) + "ConScribe Logs\\Log of " + GetTimeString() + ".log").c_str(), ConScribeLog::e_Out);


	
		// REGISTER CALLBACKS AND DO FIXUPS
		g_serialization->SetSaveCallback(g_pluginHandle, SaveCallbackHandler);
		g_serialization->SetLoadCallback(g_pluginHandle, LoadCallbackHandler);
		g_serialization->SetNewGameCallback(g_pluginHandle, NewGameCallbackHandler);

		g_stringvar = (OBSEStringVarInterface*)obse->QueryInterface(kInterface_StringVar);
		RegisterStringVarInterface(g_stringvar);

		WriteRelJump(kConsolePrintHookAddr, (UInt32)ConsolePrintHook); 		

		_MESSAGE("\nINI Options:\n\tScribeMode = %s\n\tIncludes = %s\n\tExcludes = %s\n\tTimeFormat = %s\n\tLogBackups = %s\n\tRootDirectory = %s\n",
				g_INIManager->GetINIStr("ScribeMode"),
				g_INIManager->GetINIStr("Includes"),
				g_INIManager->GetINIStr("Excludes"),
				g_INIManager->GetINIStr("TimeFormat"),
				g_INIManager->GetINIStr("LogBackups"),
				g_INIManager->GetINIStr("RootDirectory"));
	} 
	else 
	{
		g_msgIntfc = (OBSEMessagingInterface*)obse->QueryInterface(kInterface_Messaging);
		g_msgIntfc->RegisterListener(g_pluginHandle, "OBSE", OBSEMessageHandler);
	}


	obse->SetOpcodeBase(0x25A0);													// 25A0h - 25AFh
	obse->RegisterCommand(&kCommandInfo_Scribe);
	obse->RegisterCommand(&kCommandInfo_RegisterLog);
	obse->RegisterCommand(&kCommandInfo_UnregisterLog);
	obse->RegisterTypedCommand(&kCommandInfo_GetRegisteredLogNames, kRetnType_Array);
	obse->RegisterTypedCommand(&kCommandInfo_ReadFromLog, kRetnType_Array);	
	obse->RegisterCommand(&kCommandInfo_GetLogLineCount);
	obse->RegisterCommand(&kCommandInfo_DeleteLinesFromLog);	

	_MESSAGE("ConScribe initialized!\n");
	return true;
}

};