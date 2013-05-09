#include "Commands.h"


// COMMAND HANDLERS

static bool Cmd_ConScribe_Scribe_Execute(COMMAND_ARGS)
{
	UInt32 PrintC = 0;
	const char* ModName = ResolveModName(scriptObj);
	char Buffer[kMaxMessageLength];

	if (!Interfaces::kOBSEScript->ExtractFormatStringArgs(0, Buffer, paramInfo, arg1, opcodeOffsetPtr, scriptObj, eventList, kCommandInfo_Scribe.numParams, &PrintC))
		return true;
	else if (Buffer == NULL || ModName == NULL)
		return true;

	LogManager::Instance.ScribeToLog(Buffer, ModName, scriptObj->refID, PrintC);
	return true;
}


static bool Cmd_ConScribe_RegisterLog_Execute(COMMAND_ARGS)
{
	UInt32 DefaultFlag = 0;
	char Buffer[kMaxMessageLength];
	const char* ModName = ResolveModName(scriptObj);
	*result = 0;

	if (!Interfaces::kOBSEScript->ExtractFormatStringArgs(0, Buffer, paramInfo, arg1, opcodeOffsetPtr, scriptObj, eventList, kCommandInfo_RegisterLog.numParams, &DefaultFlag))
	{
		*result = -1;
		return true;
	}
	else if (Buffer == NULL || ModName == NULL)
	{
		*result = -1;
		return true;
	}

	LogManager::Instance.RegisterMod(ModName);

	static const std::string kInvalidChars = "\\/*:?\"<>;|.";

	std::string StringBuffer(Buffer);
	if (StringBuffer.find_first_of(kInvalidChars) != std::string::npos) 
	{
		_MESSAGE("Invalid log name '%s' passed in script %08x", Buffer, scriptObj->refID);
		*result = -1;
		return true;
	} 
	else if (LogManager::Instance.IsLogRegistered(Buffer))
	{
		_MESSAGE("Log name '%s' passed in script %08x already registered", Buffer, scriptObj->refID);
		*result = -1;
		return true;
	}
	
	LogManager::Instance.RegisterLog(ModName, Buffer);

	if (DefaultFlag)
		LogManager::Instance.SetDefaultLog(ModName, Buffer);

	return true;
}


static bool Cmd_ConScribe_UnregisterLog_Execute(COMMAND_ARGS)
{
	UInt32 DefaultFlag = 0;
	UInt32 DeleteFlag = 0;
	char Buffer[kMaxMessageLength];
	const char* ModName = ResolveModName(scriptObj);
	*result = 0;

	if (!Interfaces::kOBSEScript->ExtractFormatStringArgs(0, Buffer, paramInfo, arg1, opcodeOffsetPtr, scriptObj, eventList, kCommandInfo_UnregisterLog.numParams, &DefaultFlag, &DeleteFlag))
		return true;
	else if (Buffer == NULL || ModName == NULL)
		return true;

	if (!_stricmp(Buffer, "*.*"))
	{
		if (!DefaultFlag)
		{ 		
			_MESSAGE("Mod '%s' unregistered all of its logs", ModName);
			LogManager::Instance.UnregisterLog(ModName, NULL);
			DeleteFlag = 0;
		} 
		else if (DeleteFlag)
		{
			sprintf_s(Buffer, kMaxMessageLength, "%s", LogManager::Instance.GetDefaultLog(ModName));
		}

		LogManager::Instance.SetDefaultLog(ModName, (const char*)NULL);
	}
	else	
		LogManager::Instance.UnregisterLog(ModName, Buffer);

	if (DeleteFlag)
	{
		if (DeleteFile(std::string(std::string(Settings::kRootDirectory.GetData().s) + LOGDIR_PERMOD + std::string(Buffer) + ".log").c_str()))
			_MESSAGE("Deleted '%s'", Buffer);
		else 
		{
			_MESSAGE("Couldn't delete '%s' - Error %d", Buffer, GetLastError());
		}
	}

	return true;
}

static bool Cmd_ConScribe_GetRegisteredLogNames_Execute(COMMAND_ARGS)
{
	*result = 0;
	const char* ModName = ResolveModName(scriptObj);

	if (ModName == NULL)
		return true;

	LogManager::Instance.RegisterMod(ModName);

	std::vector<OBSEElement> LogList;
	LogManager::LogNameTableT RegedLogs;

	LogManager::Instance.GetRegisteredLogs(ModName, RegedLogs);

	for (LogManager::LogNameTableT::const_iterator Itr = RegedLogs.begin();
		Itr != RegedLogs.end();
		Itr++)
	{
		LogList.push_back(Itr->c_str());
	}

	std::map<std::string, OBSEElement> OBSEStringMap;

	OBSEArray* OBSEVector = ArrayFromStdVector(LogList, scriptObj);
	const char* DefaultLog = LogManager::Instance.GetDefaultLog(ModName);
	
	OBSEStringMap["default log"] = (DefaultLog ? DefaultLog : "");
	OBSEStringMap["log list"] = OBSEVector;
	
	OBSEArray* ResultArray = StringMapFromStdMap(OBSEStringMap, scriptObj);

	if (!ResultArray)
		_MESSAGE("Couldn't create array. Passed in script %08x", scriptObj->refID);
	else if (!Interfaces::kOBSEArrayVar->AssignCommandResult(ResultArray, result))
		_MESSAGE("Couldn't assign result array. Passed in script %08x", scriptObj->refID);

	return true;
}

static bool Cmd_ConScribe_ReadFromLog_Execute(COMMAND_ARGS)
{
	*result = 0;
	const char * ModName = ResolveModName(scriptObj);
	char Buffer[kMaxMessageLength];

	if (!Interfaces::kOBSEScript->ExtractFormatStringArgs(0, Buffer, paramInfo, arg1, opcodeOffsetPtr, scriptObj, eventList, kCommandInfo_ReadFromLog.numParams))
		return true;
	else if (Buffer == NULL || ModName == NULL)
		return true;

	std::string LogPath;
	std::vector<OBSEElement> LogContents;

	if (!_stricmp(Buffer, "*.*") && LogManager::Instance.GetDefaultLog(ModName))
		LogPath = std::string(Settings::kRootDirectory.GetData().s) + LOGDIR_PERMOD + std::string(LogManager::Instance.GetDefaultLog(ModName)) + ".log";
	else if (LogManager::Instance.IsLogRegistered(ModName, Buffer))
		LogPath = std::string(Settings::kRootDirectory.GetData().s) + LOGDIR_PERMOD + std::string(Buffer) + ".log";
	else
		return true;

	LogContents.push_back(LogPath.c_str());

	ConScribeLog::LogContentsT STLVector;
	ConScribeLog TempLog(LogPath.c_str(), ConScribeLog::kOpenMode_Read);
	TempLog.ReadAllLines(&STLVector);

	OBSEArray* ResultArray = ArrayFromStdVector(LogContents, scriptObj);

	for (std::vector<std::string>::const_iterator Itr = STLVector.begin(); Itr != STLVector.end(); Itr++)
	{
		if (Itr == STLVector.end() - 1 && Itr->begin() == Itr->end())
			continue;		// skip empty last lines

		Interfaces::kOBSEArrayVar->AppendElement(ResultArray, Itr->c_str());
	}

	if (!ResultArray)	
		_MESSAGE("Couldn't create array. Passed in script %08x", scriptObj->refID);
	else if (!Interfaces::kOBSEArrayVar->AssignCommandResult(ResultArray, result))
		_MESSAGE("Couldn't assign result array. Passed in script %08x", scriptObj->refID);

	return true;
}

static bool Cmd_ConScribe_GetLogLineCount_Execute(COMMAND_ARGS)
{
	const char* ModName = ResolveModName(scriptObj);
	char Buffer[kMaxMessageLength];
	*result = 0;

	if (!Interfaces::kOBSEScript->ExtractFormatStringArgs(0, Buffer, paramInfo, arg1, opcodeOffsetPtr, scriptObj, eventList, kCommandInfo_GetLogLineCount.numParams))
		return true;
	else if (Buffer == NULL || ModName == NULL)
		return true;

	*result = LogManager::Instance.GetLogLineCount(ModName, Buffer);
	return true;
}

static bool Cmd_ConScribe_DeleteLinesFromLog_Execute(COMMAND_ARGS)
{
	UInt32	LowerLimit = 0,
			UpperLimit = 0;
	const char* ModName = ResolveModName(scriptObj);
	char Buffer[kMaxMessageLength];
	*result = -1;

	if (!Interfaces::kOBSEScript->ExtractFormatStringArgs(0, Buffer, paramInfo, arg1, opcodeOffsetPtr, scriptObj, eventList, kCommandInfo_DeleteLinesFromLog.numParams, &LowerLimit, &UpperLimit))
		return true;
	else if (Buffer == NULL || ModName == NULL)
		return true;

	if (LowerLimit > UpperLimit)
	{
		_MESSAGE("Invalid slice passed to DeleteLinesFromLog in script %08x", scriptObj->refID);		
		return true;
	}

	LogManager::Instance.DeleteSliceFromLog(ModName, Buffer, LowerLimit, UpperLimit);
	*result = 0;
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

static ParamInfo kParams_GetLogLineCount[SIZEOF_FMT_STRING_PARAMS] =
{
	FORMAT_STRING_PARAMS
};

static ParamInfo kParams_DeleteLinesFromLog[SIZEOF_FMT_STRING_PARAMS + 2] =
{
	FORMAT_STRING_PARAMS,
	{	"lower limit",	kParamType_Integer,	0	},
	{	"upper limit",	kParamType_Integer,	0	}
};

CommandInfo kCommandInfo_Scribe =
{
	"Scribe",
	"",
	0,
	"Primary output function. Prints output to a specific log file and, optionally, to the console.",
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
	"Registers a log file to the calling mod as an output for the Scribe command.",
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
	"Unregisters the calling mod's registered log.",
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
	"Returns an array populated with the calling mod's registered logs, if any.",
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
	"Returns an array populated with the registered log's contents.",
	0,
	1,
	kParams_ReadFromLog,
	
	Cmd_ConScribe_ReadFromLog_Execute
};

CommandInfo kCommandInfo_GetLogLineCount =
{
	"GetLogLineCount",
	"",
	0,
	"Returns the number of lines of text present in the registered log.",
	0,
	SIZEOF_FMT_STRING_PARAMS,
	kParams_GetLogLineCount,
	
	Cmd_ConScribe_GetLogLineCount_Execute
};

CommandInfo kCommandInfo_DeleteLinesFromLog =
{
	"DeleteLinesFromLog",
	"",
	0,
	"Deletes a slice of lines from the registered log.",
	0,
	SIZEOF_FMT_STRING_PARAMS + 2,
	kParams_DeleteLinesFromLog,
	
	Cmd_ConScribe_DeleteLinesFromLog_Execute
};


// HELPER FUNCTIONS

inline const char* ResolveModName(Script* ScriptObj)
{
	if (ScriptObj->GetModIndex() != 0xFF)
	{
		if (!(*g_dataHandler)->GetNthModName(ScriptObj->GetModIndex()))	
			_MESSAGE("Error encountered while resolving mod name. ModIndex - %02x", ScriptObj->refID);
		else	
			return (*g_dataHandler)->GetNthModName(ScriptObj->GetModIndex());
	}
	return NULL;
}

OBSEArray* StringMapFromStdMap(const std::map<std::string, OBSEElement>& Data, Script* CallingScript)
{
	OBSEArray* Arr = Interfaces::kOBSEArrayVar->CreateStringMap(NULL, NULL, 0, CallingScript);

	for (std::map<std::string, OBSEElement>::const_iterator Itr = Data.begin(); Itr != Data.end(); ++Itr)
		Interfaces::kOBSEArrayVar->SetElement(Arr, Itr->first.c_str(), Itr->second);

	return Arr;
}

OBSEArray* ArrayFromStdVector(const std::vector<OBSEElement>& Data, Script* CallingScript)
{
	OBSEArray* Arr = Interfaces::kOBSEArrayVar->CreateArray(&Data[0], Data.size(), CallingScript);
	return Arr;
}