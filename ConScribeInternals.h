#pragma once

#include <cstdlib>
#include <string>
#include <vector>
#include <fstream>
#include <map>
#include "windows.h"
#include "time.h"
#include "intrin.h"

#include "obse/PluginAPI.h"
#include "obse/ParamInfos.h"
#include "obse/ScriptUtils.h"
#include "obse/GameData.h"
#include "obse_common/SafeWrite.h"
#include "common/IDirectoryIterator.h"
#include "[ Libraries ]/INI Mananger/INIManager.h"

using namespace SME;
using namespace INI;

#define SAVE_VERSION									3
#define MAX_BACKUPS										5

//	TODO +++++++++++++++++++

extern const char*										g_HookMessage;
extern char												g_Buffer[0x4000];

#if OBLIVION_VERSION == OBLIVION_VERSION_1_2_416
	const UInt32 kConsolePrintHookAddr = 0x00585D2E;
	const UInt32 kConsolePrintRetnAddr = 0x00585D36;
	const UInt32 kConsolePrintCallAddr = 0x00574A80;
#else
	#error unsupported oblivion version
#endif

void ConsolePrintHook(void);


class ConScribeINIManager : public INIManager
{
public:
	void					Initialize();
};

extern ConScribeINIManager*		g_INIManager;


class ConScribeLog 
{
	ConScribeLog();
protected:
	std::fstream										FileStream;
	char*												FilePath;
public:
	enum												OpenMode
															{
																e_OutAp = 0,
																e_Out,
																e_In
															};
	virtual												~ConScribeLog();

	ConScribeLog(const char* FileName, OpenMode Mode);

	void												Open(const char* FileName, OpenMode Mode);
	void												Close(void);
	virtual void										WriteOutput(const char* Message);
	void												AppendLoadHeader(void);
	virtual void										HandleLoadCallback(void) { return; }
	void												ReadAllLines(std::vector<std::string>& LogContents);
	UInt32												GetLineCount(void);
	void												DeleteSlice(UInt32 Lower, UInt32 Upper);
};

class ConsoleLog : public ConScribeLog
{
public:
	ConsoleLog(const char* FileName, OpenMode Mode)	: ConScribeLog(FileName, Mode) {}

	void												WriteOutput(const char* Message);	
	void												HandleLoadCallback(void);
};

extern ConScribeLog*									g_ConsoleLog;

class LogData
{
public:
	int													DefaultLog;
	std::vector<std::string>							RegisteredLogs;

	LogData();
};

class LogManager
{
	static LogManager*									Singleton;
	void												SetDefaultLog(const char* ModName, UInt32 Idx);		// used by serialization code
public:
	static LogManager*									GetSingleton();

	static enum											ScribeOp
															{
																e_Script,
																e_Default,
																e_Mod
															};

	typedef std::map<std::string, LogData*>				_LogDB;
	_LogDB												LogDB;

	void												DumpDatabase(void);
	void												PurgeDatabase(void);

	bool												IsModRegistered(const char* ModName);
	bool												IsLogRegistered(const char* ModName, const char* LogName);
	bool												IsLogRegistered(const char* LogName);

	void												RegisterMod(const char* ModName);
	void												SetDefaultLog(const char* ModName, const char* LogName);
	const char*											GetDefaultLog(const char* ModName);
	void												RegisterLog(const char* ModName, const char* LogName);
	void												UnregisterLog(const char* ModName, const char* LogName);
	void												UnregisterLog(const char* ModName);		// clears log table
	void												ScribeToLog(const char* Message, const char* ModName, UInt32 RefID, UInt32 PrintC);	
	void												DeleteSliceFromLog(const char* ModName, const char* LogName, UInt32 Lower, UInt32 Upper);
	int													GetLogLineCount(const char* ModName, const char* LogName);

	LogData*											GetModLogData(const char* ModName);

	void												DoLoadCallback(OBSESerializationInterface* Interface);
	void												DoSaveCallback(OBSESerializationInterface* Interface);	

	void												ConvertDeprecatedRecordCSRB(std::string Record);
};



enum													HouseKeepingOps
															{
																e_BackupLogs,
																e_AppendHeaders
															};



std::string												GetTimeString(void);
UInt32													GetInString(std::string String1, std::string String2);
void													LogWinAPIErrorMessage(DWORD ErrorID);

bool													CreateLogDirectories(void);

void													PerformHouseKeeping(const char* DirectoryPath, const char* File, HouseKeepingOps Operation);
void													DoBackup(std::string FilePath, std::string FileName);
void													AppendHeader(std::string FilePath);

extern ConScribeLog*									g_ConsoleLog;
extern UInt32											g_SessionCount;
extern OBSEArrayVarInterface*							g_arrayIntfc;

typedef OBSEArrayVarInterface::Array					OBSEArray;
typedef OBSEArrayVarInterface::Element					OBSEElement;