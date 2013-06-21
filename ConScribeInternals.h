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

#include "[Libraries]\SME Sundries\SME_Prefix.h"
#include "[Libraries]\SME Sundries\INIManager.h"
#include "[Libraries]\SME Sundries\MemoryHandler.h"
#include "[Libraries]\SME Sundries\MiscGunk.h"
#include "[Libraries]\SME Sundries\StringHelpers.h"

#define CSEAPI_NO_CODA		1

#include "Construction Set Extender\CSEInterfaceAPI.h"

using namespace SME;
using namespace SME::INI;
using namespace SME::MemoryHandler;

_DeclareMemHdlr(ConsolePrint, "extracts the sweet nectar");

namespace Interfaces
{
	extern PluginHandle						kOBSEPluginHandle;

	extern OBSEScriptInterface*				kOBSEScript;
	extern OBSEArrayVarInterface*			kOBSEArrayVar;
	extern OBSESerializationInterface*		kOBSESerialization;
	extern OBSEStringVarInterface*			kOBSEStringVar;
	extern OBSEMessagingInterface*			kOBSEMessaging;
	extern CSEIntelliSenseInterface*		kCSEIntelliSense;
	extern CSEConsoleInterface*				kCSEConsole;
}

namespace Settings
{
	extern INISetting		kScribeMode;
	extern INISetting		kIncludes;
	extern INISetting		kExcludes;
	extern INISetting		kTimeFormat;
	extern INISetting		kLogBackups;
	extern INISetting		kRootDirectory;
	extern INISetting		kSaveDataToCoSave;
}

class ConScribeINIManager : public INIManager
{
public:
	void								Initialize(const char* INIPath, void* Paramenter);

	static ConScribeINIManager			Instance;
};

class ConScribeLog 
{
protected:
	std::fstream										FileStream;
	char*												FilePath;
	
	ConScribeLog();
public:
	enum
	{
		kOpenMode_Append = 0,
		kOpenMode_Write,
		kOpenMode_Read
	};

	typedef std::vector<std::string>					LogContentsT;

	ConScribeLog(const char* FileName, UInt32 Mode);
	virtual ~ConScribeLog();


	void												Open(const char* FileName, UInt32 Mode);
	void												Close(void);

	virtual void										WriteOutput(const char* Message);
	virtual void										AppendLoadHeader(void);

	virtual void										HandleLoadCallback(void) { return; }

	UInt32												GetLineCount(void);
	UInt32												ReadAllLines(LogContentsT* LogContents = NULL);		// returns line count
	void												DeleteSlice(UInt32 Lower, UInt32 Upper);

	static UInt32										kSessionCounter;
};

class ConsoleLog : public ConScribeLog
{
	UInt32												GetSubstringHits(std::string Source, std::string SubstringChain);
public:
	ConsoleLog();
	ConsoleLog(const char* FileName, UInt32 Mode);
	virtual ~ConsoleLog();

	virtual void										WriteOutput(const char* Message);	
	virtual void										HandleLoadCallback(void);

	static ConsoleLog									Instance;
};

class LogManager
{
public:
	typedef std::vector<std::string>				LogNameTableT;

	enum
	{
		kScribe_ScriptLog	= 0,
		kScribe_DefaultLog,
		kScribe_ModLog
	};

	enum
	{
		kHouseKeeping_BackupLogs	= 0,
		kHouseKeeping_AppendHeaders
	};
private:
	struct LogData
	{
		int												DefaultLogIndex;
		LogNameTableT									RegisteredLogs;

		LogData();
	};

	enum
	{
		kSaveVersion		= 3,
		kMaxBackups			= 5,
	};

	typedef std::map<std::string, LogData*>				LogDataTableT;
	LogDataTableT										LogDataStore;

	bool												IsModRegistered(const char* ModName);
	void												SetDefaultLog(const char* ModName, UInt32 Idx);		// used by serialization code

	void												PerformHouseKeeping(const char* DirectoryPath, const char* File, UInt32 Operation);
	void												BackupLog(std::string FilePath, std::string FileName);
	
	void												Dump(void);
public:	
	bool												Initialize(void);

	void												Purge(void);

	bool												IsLogRegistered(const char* ModName, const char* LogName);
	bool												IsLogRegistered(const char* LogName);

	void												RegisterMod(const char* ModName);
	void												GetRegisteredLogs(const char* ModName, LogNameTableT& Out);

	void												RegisterLog(const char* ModName, const char* LogName);
	void												UnregisterLog(const char* ModName, const char* LogName);		// pass LogName = NULL to clear log table

	const char*											GetDefaultLog(const char* ModName);
	void												SetDefaultLog(const char* ModName, const char* LogName);

	void												ScribeToLog(const char* Message, const char* ModName, UInt32 RefID, UInt32 PrintC);	
	void												DeleteSliceFromLog(const char* ModName, const char* LogName, UInt32 Lower, UInt32 Upper);
	int													GetLogLineCount(const char* ModName, const char* LogName);

	void												HandleGameLoad(OBSESerializationInterface* Interface);
	void												HandleGameSave(OBSESerializationInterface* Interface);	

	static LogManager									Instance;
};

#define LOGDIR					"ConScribe Logs\\"
#define LOGDIR_PERMOD			"ConScribe Logs\\Per-Mod\\"
#define LOGDIR_PERSCRIPT		"ConScribe Logs\\Per-Script\\"
