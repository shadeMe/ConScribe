#include "ConScribeInternals.h"
#include "Commands.h"
#include "VersionInfo.h"

IDebugLog		gLog("ConScribe.log");

static void LoadCallbackHandler(void * reserved)
{
	ConScribeLog::kSessionCounter++;
	ConsoleLog::Instance.HandleLoadCallback();

	LogManager::Instance.HandleGameLoad(Interfaces::kOBSESerialization);
}

static void SaveCallbackHandler(void * reserved)
{
	LogManager::Instance.HandleGameSave(Interfaces::kOBSESerialization);
}

static void NewGameCallbackHandler(void * reserved)
{	
	LogManager::Instance.Purge();
}

void ConScribeMessageHandler(OBSEMessagingInterface::Message* Msg)
{
	if (Msg->type == 'CSEI')
	{
		CSEInterface* Interface = (CSEInterface*)Msg->data;

		Interfaces::kCSEConsole = (CSEConsoleInterface*)Interface->InitializeInterface(CSEInterface::kCSEInterface_Console);
		Interfaces::kCSEIntelliSense = (CSEIntelliSenseInterface*)Interface->InitializeInterface(CSEInterface::kCSEInterface_IntelliSense);

		_MESSAGE("Received interface from CSE");

		Interfaces::kCSEConsole->PrintToConsole("ConScribe", "Registering command URLs ...");
		Interfaces::kCSEIntelliSense->RegisterCommandURL("Scribe", "http://cs.elderscrolls.com/constwiki/index.php/Scribe");
		Interfaces::kCSEIntelliSense->RegisterCommandURL("RegisterLog", "http://cs.elderscrolls.com/constwiki/index.php/RegisterLog");
		Interfaces::kCSEIntelliSense->RegisterCommandURL("UnregisterLog", "http://cs.elderscrolls.com/constwiki/index.php/UnregisterLog");
		Interfaces::kCSEIntelliSense->RegisterCommandURL("GetRegisteredLogNames", "http://cs.elderscrolls.com/constwiki/index.php/GetRegisteredLogNames");
		Interfaces::kCSEIntelliSense->RegisterCommandURL("ReadFromLog", "http://cs.elderscrolls.com/constwiki/index.php/ReadFromLog");
		Interfaces::kCSEIntelliSense->RegisterCommandURL("GetLogLineCount", "http://cs.elderscrolls.com/constwiki/index.php/GetLogLineCount");
		Interfaces::kCSEIntelliSense->RegisterCommandURL("DeleteLinesFromLog", "http://cs.elderscrolls.com/constwiki/index.php/DeleteLinesFromLog");
	}
}

void OBSEMessageHandler(OBSEMessagingInterface::Message* Msg)
{
	switch (Msg->type)
	{
	case OBSEMessagingInterface::kMessage_PostLoad:
		Interfaces::kOBSEMessaging->RegisterListener(Interfaces::kOBSEPluginHandle, "CSE", ConScribeMessageHandler);
		_MESSAGE("Registered to receive messages from CSE");

		break;
	case OBSEMessagingInterface::kMessage_PostPostLoad:
		_MESSAGE("Requesting an interface from CSE");
		Interfaces::kOBSEMessaging->Dispatch(Interfaces::kOBSEPluginHandle, 'CSEI', NULL, 0, "CSE");	

		break;
	}
}

extern "C"
{
	bool OBSEPlugin_Query(const OBSEInterface * obse, PluginInfo * info)
	{
		info->infoVersion = PluginInfo::kInfoVersion;
		info->name = "ConScribe";
		info->version = PACKED_SME_VERSION;

		_MESSAGE("ConScribe Initializing...");

		if (obse->isEditor == false)
		{
			if (obse->obseVersion < OBSE_VERSION_INTEGER)
			{
				_MESSAGE("OBSE version too old (got %08X expected at least %08X)", obse->obseVersion, OBSE_VERSION_INTEGER);
				return false;
			}

			if (obse->oblivionVersion != OBLIVION_VERSION)
			{
				_MESSAGE("incorrect Oblivion version (got %08X need %08X)", obse->oblivionVersion, OBLIVION_VERSION);
				return false;
			}

			Interfaces::kOBSESerialization = (OBSESerializationInterface *)obse->QueryInterface(kInterface_Serialization);
			if (!Interfaces::kOBSESerialization)
			{
				_MESSAGE("serialization interface not found");
				return false;
			}

			if (Interfaces::kOBSESerialization->version < OBSESerializationInterface::kVersion)
			{
				_MESSAGE("incorrect serialization version found (got %08X need %08X)", Interfaces::kOBSESerialization->version, OBSESerializationInterface::kVersion);
				return false;
			}

			Interfaces::kOBSEArrayVar = (OBSEArrayVarInterface*)obse->QueryInterface(kInterface_ArrayVar);
			if (!Interfaces::kOBSEArrayVar)
			{
				_MESSAGE("Array interface not found");
				return false;
			}

			Interfaces::kOBSEScript = (OBSEScriptInterface*)obse->QueryInterface(kInterface_Script);
			if (!Interfaces::kOBSEScript)
			{
				_MESSAGE("Script interface not found");
				return false;
			}
		}

		return true;
	}

	bool OBSEPlugin_Load(const OBSEInterface * obse)
	{
		Interfaces::kOBSEPluginHandle = obse->GetPluginHandle();

		ConScribeINIManager::Instance.Initialize("Data\\OBSE\\Plugins\\ConScribe.ini", NULL);

		if (obse->isEditor == false)
		{
			if (!_stricmp(Settings::kRootDirectory.GetData().s, "Default"))
			{
				Settings::kRootDirectory.SetString("%s\\Data\\", obse->GetOblivionDirectory());
				_MESSAGE("Root set to default directory");
			}

			if (LogManager::Instance.Initialize() == false)
				return false;

			Interfaces::kOBSESerialization->SetSaveCallback(Interfaces::kOBSEPluginHandle, SaveCallbackHandler);
			Interfaces::kOBSESerialization->SetLoadCallback(Interfaces::kOBSEPluginHandle, LoadCallbackHandler);
			Interfaces::kOBSESerialization->SetNewGameCallback(Interfaces::kOBSEPluginHandle, NewGameCallbackHandler);

			Interfaces::kOBSEStringVar = (OBSEStringVarInterface*)obse->QueryInterface(kInterface_StringVar);
			RegisterStringVarInterface(Interfaces::kOBSEStringVar);

			_MemHdlr(ConsolePrint).WriteJump();
		} 
		else 
		{
			Interfaces::kOBSEMessaging = (OBSEMessagingInterface*)obse->QueryInterface(kInterface_Messaging);
			Interfaces::kOBSEMessaging->RegisterListener(Interfaces::kOBSEPluginHandle, "OBSE", OBSEMessageHandler);
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