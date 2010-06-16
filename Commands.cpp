#include "Commands.h"


// COMMAND HANDLERS

static bool Cmd_ConScribe_Scribe_Execute(COMMAND_ARGS)
{
	UInt32 PrintC = 0;
	const char* ModName = ResolveModName(scriptObj);
	char Buffer[kMaxMessageLength];

	if (!ExtractFormatStringArgs(0, Buffer, paramInfo, arg1, opcodeOffsetPtr, scriptObj, eventList, kCommandInfo_Scribe.numParams, &PrintC))
		return true;
	else if (Buffer == NULL || ModName == NULL)
		return true;

	LogManager::GetSingleton()->ScribeToLog(Buffer, ModName, scriptObj->refID, PrintC);
	return true;
}


static bool Cmd_ConScribe_RegisterLog_Execute(COMMAND_ARGS)
{
	UInt32 DefaultFlag = 0;
	char Buffer[kMaxMessageLength];
	const char* ModName = ResolveModName(scriptObj);

	if (Buffer == NULL || ModName == NULL)
		return true;
	else if (!ExtractFormatStringArgs(0, Buffer, paramInfo, arg1, opcodeOffsetPtr, scriptObj, eventList, kCommandInfo_RegisterLog.numParams, &DefaultFlag))
		return true;

	LogManager::GetSingleton()->RegisterMod(ModName);

	static std::string InvalidChars = "\\/*:?\"<>;|.";

	std::string StringBuffer(Buffer);
	if (StringBuffer.find_first_of(InvalidChars) != std::string::npos) {
		_MESSAGE("Invalid log name '%s' passed in script %08x", Buffer, scriptObj->refID);
		return true;
	} else	
		LogManager::GetSingleton()->RegisterLog(ModName, Buffer);

	if (DefaultFlag)			LogManager::GetSingleton()->SetDefaultLog(ModName, Buffer);
	return true;
}


static bool Cmd_ConScribe_UnregisterLog_Execute(COMMAND_ARGS)
{
	UInt32 DefaultFlag = 0;
	UInt32 DeleteFlag = 0;
	char Buffer[kMaxMessageLength];
	const char* ModName = ResolveModName(scriptObj);

	if (Buffer == NULL || ModName == NULL)
		return true;
	else if (!ExtractFormatStringArgs(0, Buffer, paramInfo, arg1, opcodeOffsetPtr, scriptObj, eventList, kCommandInfo_UnregisterLog.numParams, &DefaultFlag, &DeleteFlag))
		return true;

	if (!_stricmp(Buffer, "*.*")) {
		if (!DefaultFlag) { 		
			_MESSAGE("Mod '%s' unregistered all of its logs", ModName);
			LogManager::GetSingleton()->GetModLogData(ModName)->RegisteredLogs.clear();
			DeleteFlag = 0;
		}
		LogManager::GetSingleton()->SetDefaultLog(ModName, (const char*)NULL);
	}
	else	
		LogManager::GetSingleton()->UnregisterLog(ModName, Buffer);

	if (DeleteFlag) {
		DeleteFile(std::string(std::string(GET_INI("RootDirectory")->GetValueAsString()) + "ConScribe Logs\\Per-Mod\\" + std::string(Buffer) + ".log").c_str());
		_MESSAGE("Deleted '%s'", Buffer);
	}

	return true;
}

static bool Cmd_ConScribe_GetRegisteredLogNames_Execute(COMMAND_ARGS)
{
	*result = 0;
	const char* ModName = ResolveModName(scriptObj);

	if (ModName == NULL)	return true;

	LogManager::GetSingleton()->RegisterMod(ModName);

	std::vector<OBSEElement> LogList;
	for (std::vector<std::string>::const_iterator Itr = LogManager::GetSingleton()->GetModLogData(ModName)->RegisteredLogs.begin(); Itr != LogManager::GetSingleton()->GetModLogData(ModName)->RegisteredLogs.end(); Itr++) {
		LogList.push_back(Itr->c_str());
	}
	std::map<std::string, OBSEElement> OBSEStringMap;

	OBSEArray* OBSEVector = ArrayFromStdVector(LogList, scriptObj);

	OBSEStringMap["default log"] = LogManager::GetSingleton()->GetDefaultLog(ModName);
	OBSEStringMap["log list"] = OBSEVector;
	
	OBSEArray* ResultArray = StringMapFromStdMap(OBSEStringMap, scriptObj);

	if (!ResultArray)													_MESSAGE("Couldn't create array. Passed in script %08x", scriptObj->refID);
	else if (!g_arrayIntfc->AssignCommandResult(ResultArray, result))	_MESSAGE("Couldn't assign result array. Passed in script %08x", scriptObj->refID);

	return true;
}

static bool Cmd_ConScribe_ReadFromLog_Execute(COMMAND_ARGS)
{
	*result = 0;
	const char * ModName = ResolveModName(scriptObj);
	char Buffer[kMaxMessageLength];

	if (!ExtractFormatStringArgs(0, Buffer, paramInfo, arg1, opcodeOffsetPtr, scriptObj, eventList, kCommandInfo_ReadFromLog.numParams))
		return true;
	else if (Buffer == NULL || ModName == NULL)
		return true;

	std::string LogPath;
	std::vector<OBSEElement> LogContents;

	if (!_stricmp(Buffer, "*.*") && LogManager::GetSingleton()->GetDefaultLog(ModName))
		LogPath = std::string(GET_INI("RootDirectory")->GetValueAsString()) + "ConScribe Logs\\Per-Mod\\" + std::string(LogManager::GetSingleton()->GetDefaultLog(ModName)) + ".log";
	else if (LogManager::GetSingleton()->IsLogRegistered(ModName, Buffer))
		LogPath = std::string(GET_INI("RootDirectory")->GetValueAsString()) + "ConScribe Logs\\Per-Mod\\" + std::string(Buffer) + ".log";
	else		return true;
	LogContents.push_back(LogPath.c_str());

	ConScribeLog* TempLog = new ConScribeLog(LogPath.c_str(), ConScribeLog::e_In);
	std::vector<std::string> STLVector(TempLog->ReadAllLines());

	OBSEArray* ResultArray = ArrayFromStdVector(LogContents, scriptObj);

	for (std::vector<std::string>::const_iterator Itr = STLVector.begin(); Itr != STLVector.end(); Itr++) {
		if (Itr == STLVector.end() - 1 && Itr->begin() == Itr->end())		continue;		// skip empty last lines
		g_arrayIntfc->AppendElement(ResultArray, Itr->c_str());
	}

	if (!ResultArray)													_MESSAGE("Couldn't create array. Passed in script %08x", scriptObj->refID);
	else if (!g_arrayIntfc->AssignCommandResult(ResultArray, result))	_MESSAGE("Couldn't assign result array. Passed in script %08x", scriptObj->refID);

	return true;
}


// COMMAND & PARAM INFO

static ParamInfo kParams_Scribe[SIZEOF_FMT_STRING_PARAMS + 1] =
{
	FORMAT_STRING_PARAMS,
	{	"print to console",	kParamType_Integer,	1	},
};

static ParamInfo kParams_RegisterLog[SIZEOF_FMT_STRING_PARAMS + 1] =
{
	FORMAT_STRING_PARAMS,
	{	"default flag",	kParamType_Integer,	1	},
};

static ParamInfo kParams_UnregisterLog[SIZEOF_FMT_STRING_PARAMS + 2] =
{
	FORMAT_STRING_PARAMS,
	{	"default flag",	kParamType_Integer,	1	},
	{	"delete log flag",	kParamType_Integer,	1	}
};

static ParamInfo kParams_ReadFromLog[SIZEOF_FMT_STRING_PARAMS] =
{
	FORMAT_STRING_PARAMS
};

CommandInfo kCommandInfo_Scribe =
{
	"Scribe",
	"",
	0,
	"ConScribe: Primary output function. Prints output to a specific log file and, optionally, to the console.",
	0,
	SIZEOF_FMT_STRING_PARAMS + 1,
	kParams_Scribe,
	
	Cmd_ConScribe_Scribe_Execute
};

CommandInfo kCommandInfo_RegisterLog =
{
	"RegisterLog",
	"",
	0,
	"ConScribe: Registers a log file to the calling mod as an output for the Scribe command.",
	0,
	SIZEOF_FMT_STRING_PARAMS + 1,
	kParams_RegisterLog,
	
	Cmd_ConScribe_RegisterLog_Execute
};

CommandInfo kCommandInfo_UnregisterLog =
{
	"UnregisterLog",
	"",
	0,
	"ConScribe: Unregisters the calling mod's registered log.",
	0,
	SIZEOF_FMT_STRING_PARAMS + 2,
	kParams_UnregisterLog,
	
	Cmd_ConScribe_UnregisterLog_Execute
};

CommandInfo kCommandInfo_GetRegisteredLogNames =
{
	"GetRegisteredLogNames",
	"",
	0,
	"ConScribe: Returns an array populated with the calling mod's registered logs, if any.",
	0,
	1,
	kParams_OneOptionalInt,
	
	Cmd_ConScribe_GetRegisteredLogNames_Execute
};

CommandInfo kCommandInfo_ReadFromLog =
{
	"ReadFromLog",
	"",
	0,
	"ConScribe: Returns an array populated with the registered log's contents",
	0,
	1,
	kParams_ReadFromLog,
	
	Cmd_ConScribe_ReadFromLog_Execute
};


// HELPER FUNCTIONS

inline const char* ResolveModName(Script* ScriptObj)
{
	if (ScriptObj->GetModIndex() != 0xFF) {
		if (!(*g_dataHandler)->GetNthModName(ScriptObj->GetModIndex()))		_MESSAGE("Error encountered while resolving mod name. ModIndex - %02x", ScriptObj->refID);
		else																return (*g_dataHandler)->GetNthModName(ScriptObj->GetModIndex());
	}
	return NULL;
}

OBSEArray* StringMapFromStdMap(const std::map<std::string, OBSEElement>& Data, Script* CallingScript)
{
	OBSEArray* Arr = g_arrayIntfc->CreateStringMap(NULL, NULL, 0, CallingScript);

	for (std::map<std::string, OBSEElement>::const_iterator Itr = Data.begin(); Itr != Data.end(); ++Itr) {
		g_arrayIntfc->SetElement(Arr, Itr->first.c_str(), Itr->second);
	}

	return Arr;
}

OBSEArray* ArrayFromStdVector(const std::vector<OBSEElement>& Data, Script* CallingScript)
{
	OBSEArray* Arr = g_arrayIntfc->CreateArray(&Data[0], Data.size(), CallingScript);
	return Arr;
}