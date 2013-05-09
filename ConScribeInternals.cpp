#include "ConScribeInternals.h"

_DefineHookHdlr(ConsolePrint, 0x00585D2E);

#define _hhName		ConsolePrint
_hhBegin()
{
	_hhSetVar(Call, 0x00574A80);
	_hhSetVar(Retn, 0x00585D36);
	
	__asm
	{
		pushad
		mov		ecx, [ecx]
		push	ecx
		lea		ecx, ConsoleLog::Instance
		call	ConsoleLog::WriteOutput
		popad

		push	ecx
		mov		ecx, eax
		call	_hhGetVar(Call)
		jmp		_hhGetVar(Retn)
	}
}

namespace Interfaces
{
	PluginHandle					kOBSEPluginHandle = kPluginHandle_Invalid;

	OBSEScriptInterface*			kOBSEScript = NULL;
	OBSEArrayVarInterface*			kOBSEArrayVar = NULL;
	OBSESerializationInterface*		kOBSESerialization = NULL;
	OBSEStringVarInterface*			kOBSEStringVar = NULL;
	OBSEMessagingInterface*			kOBSEMessaging = NULL;
	CSEIntelliSenseInterface*		kCSEIntelliSense = NULL;
	CSEConsoleInterface*			kCSEConsole = NULL;
}

namespace Settings
{
	INISetting		kScribeMode("ScribeMode", "ConsoleLog", "The mode of logging used by the console log(s)", "Static");
	INISetting		kIncludes("Includes", "ConsoleLog", "Include string", "");
	INISetting		kExcludes("Excludes", "ConsoleLog", "Exclude string", "");
	INISetting		kTimeFormat("TimeFormat", "General", "Date format string", "%m-%d-%Y %H-%M-%S");
	INISetting		kLogBackups("LogBackups", "General", "Number of log backups", (SInt32)-1);
	INISetting		kRootDirectory("RootDirectory", "General", "Location of the ConScribe logs folder", "Default");
	INISetting		kSaveDataToCoSave("SaveDataToCoSave", "General", "Save log registration data to the OBSE co-save", (SInt32)1);
}


ConScribeINIManager			ConScribeINIManager::Instance;

void ConScribeINIManager::Initialize(const char* INIPath, void* Paramenter)
{
	this->INIFilePath = INIPath;
	_MESSAGE("INI Path: %s", INIPath);

	std::fstream INIStream(INIPath, std::fstream::in);
	bool CreateINI = false;

	if (INIStream.fail())
	{
		_MESSAGE("INI File not found; Creating one...");
		CreateINI = true;
	}

	INIStream.close();
	INIStream.clear();

	RegisterSetting(&Settings::kScribeMode);
	RegisterSetting(&Settings::kIncludes);
	RegisterSetting(&Settings::kExcludes);
	RegisterSetting(&Settings::kTimeFormat);
	RegisterSetting(&Settings::kLogBackups);
	RegisterSetting(&Settings::kRootDirectory);
	RegisterSetting(&Settings::kSaveDataToCoSave);

	if (CreateINI)
		Save();
}

UInt32			ConScribeLog::kSessionCounter = 0;

ConScribeLog::ConScribeLog(const char* FileName, UInt32 Mode)
{
	FilePath = new char[MAX_PATH];
	Open(FileName, Mode);
}

ConScribeLog::ConScribeLog() :
	FileStream()
{
	FilePath = new char[MAX_PATH];
}

void ConScribeLog::Open(const char* FileName, UInt32 Mode)
{
	if (FileStream.is_open())
		Close();

	switch(Mode)
	{
	case kOpenMode_Append:
		FileStream.open(FileName, std::fstream::out|std::fstream::app);
		break;
	case kOpenMode_Write:
		FileStream.open(FileName, std::fstream::out);
		break;
	case kOpenMode_Read:
		FileStream.open(FileName, std::fstream::in);
		break;
	}

	sprintf_s(FilePath, MAX_PATH, "%s", FileName);
}

void ConScribeLog::Close()
{
	FileStream.close();
	FileStream.clear();
}

ConScribeLog::~ConScribeLog()
{
	Close();

	delete FilePath;
}

void ConScribeLog::WriteOutput(const char* Message)
{
	if (Message == NULL)
		return;

	FileStream << Message << std::endl;
}

void ConScribeLog::AppendLoadHeader()
{
	std::string TimeString;
	SME::MiscGunk::GetTimeString(TimeString, Settings::kTimeFormat.GetData().s);
	
	std::string Padding(TimeString.length() + 28, '=');

	FileStream << Padding << std::endl
			  << "Game Instance : " << kSessionCounter << " | Time : " << TimeString << std::endl
			  << Padding << std::endl;
}

UInt32 ConScribeLog::ReadAllLines(LogContentsT* LogContents)
{
	char Buffer[0x4000] = {0};
	int LineCount = 0;

	if (FileStream.fail() == false)
	{
		while (FileStream.eof() == false)
		{
			FileStream.getline(Buffer, sizeof(Buffer));
			LineCount++;

			if (LogContents)
				LogContents->push_back(Buffer);
		}
	}
	else if (LogContents)
		LogContents->push_back("Log file not found");

	return LineCount;
}

UInt32 ConScribeLog::GetLineCount(void)
{
	return ReadAllLines();
}

void ConScribeLog::DeleteSlice(UInt32 Lower, UInt32 Upper)
{
	if (Lower > Upper)
		return;

	std::string TempPath = std::string(FilePath) + ".tmp";
	std::fstream TempLog(TempPath.c_str(), std::fstream::out);
	UInt32 LineCount = 1;
	char Buffer[0x4000] = {0};

	if (FileStream.fail() == false)
	{
		while (FileStream.eof() == false)
		{
			FileStream.getline(Buffer, sizeof(Buffer));

			if (LineCount > Upper || LineCount < Lower)
				TempLog << Buffer << "\n";

			LineCount++;
		}

		TempLog.flush();
		TempLog.close();
		TempLog.clear();

		Close();

		if (!MoveFileEx(TempPath.c_str(), FilePath, MOVEFILE_REPLACE_EXISTING))
		{
			_MESSAGE("DeleteSlice failed (Error = %d) - Existing Name = %s ; New Name = %s", GetLastError(), TempPath.c_str(), FilePath);
		}

		Open(FilePath, kOpenMode_Read);
	}
}

UInt32 ConsoleLog::GetSubstringHits( std::string Source, std::string SubstringChain )
{
	std::string::size_type Idx = 0;
	std::string Extract;
	UInt32 Hits = 0;

	if (SubstringChain[SubstringChain.length() - 1] != ';')
		SubstringChain += ";";

	while (SubstringChain.find(";") != std::string::npos)
	{
		Idx = SubstringChain.find(";");
		Extract = SubstringChain.substr(0, Idx);

		if (Source.find(Extract) != std::string::npos)
			Hits++;

		SubstringChain.erase(0, Idx + 1);
	}
	return Hits;
}

void ConsoleLog::WriteOutput(const char* Message)
{
	if (Message == NULL)												
		return;

	std::string Includes(Settings::kIncludes.GetData().s),
				Excludes(Settings::kExcludes.GetData().s);

	if (Includes != "" && GetSubstringHits(Message, Includes) == 0)
		return;

	if (Excludes != "" && GetSubstringHits(Message, Includes) > 0)
		return;

	FileStream << Message << std::endl;
}

void ConsoleLog::HandleLoadCallback(void)
{
	if (!_stricmp(Settings::kScribeMode.GetData().s, "PerLoad"))
	{
		std::string Path = Settings::kRootDirectory.GetData().s;
		std::string TimeString;

		SME::MiscGunk::GetTimeString(TimeString, Settings::kTimeFormat.GetData().s);
		Path += LOGDIR; Path += "\\Log of "; Path += TimeString; Path += ".log";

		Open(Path.c_str(), kOpenMode_Write);
	}
	else
		AppendLoadHeader();
}

ConsoleLog ConsoleLog::Instance;

ConsoleLog::ConsoleLog( const char* FileName, UInt32 Mode ) :
	ConScribeLog(FileName, Mode)
{
	;//
}

ConsoleLog::ConsoleLog() :
	ConScribeLog()
{
	;//
}

ConsoleLog::~ConsoleLog()
{
	;//
}

LogManager LogManager::Instance;

LogManager::LogData::LogData()
{
	DefaultLogIndex = -1;
	RegisteredLogs.reserve(5);
}

void LogManager::Dump()
{
	_MESSAGE("Registered Logs\n================\n");

	for (LogDataTableT::const_iterator Itr = LogDataStore.begin(); Itr != LogDataStore.end(); Itr++)
	{
		_MESSAGE("%s : [ default: %s ]", Itr->first.c_str(), GetDefaultLog(Itr->first.c_str()));

		LogNameTableT::const_iterator ItrVec = Itr->second->RegisteredLogs.begin();

		while (ItrVec != Itr->second->RegisteredLogs.end())
		{
			_MESSAGE("\t%s", ItrVec->c_str());
			ItrVec++;
		}
	}

	_MESSAGE("\n");
}

void LogManager::Purge()
{
	for (LogDataTableT::iterator Itr = LogDataStore.begin(); Itr != LogDataStore.end(); Itr++)
		delete Itr->second;

	LogDataStore.clear();
}

bool LogManager::IsModRegistered(const char* ModName)
{
	for (LogDataTableT::iterator Itr = LogDataStore.begin(); Itr != LogDataStore.end(); Itr++)
	{
		if (!_stricmp(ModName, Itr->first.c_str()))
			return true;
	}

	return false;
}

bool LogManager::IsLogRegistered(const char* ModName, const char* LogName)
{
	if (IsModRegistered(ModName) == false)
		return false;

	LogNameTableT* LogList = &(LogDataStore.find(ModName)->second->RegisteredLogs);
	for (LogNameTableT::iterator Itr = LogList->begin(); Itr != LogList->end(); Itr++)
	{
		if (!_stricmp(LogName, Itr->c_str()))
			return true;
	}

	return false;
}

bool LogManager::IsLogRegistered(const char* LogName)
{
	for (LogDataTableT::const_iterator Itr = LogDataStore.begin(); Itr != LogDataStore.end(); Itr++)
	{
		LogNameTableT::const_iterator ItrVec = Itr->second->RegisteredLogs.begin();
		while (ItrVec != Itr->second->RegisteredLogs.end())
		{
			if (!_stricmp(LogName, ItrVec->c_str()))
				return true;

			ItrVec++;
		}
	}

	return false;
}

void LogManager::RegisterMod(const char* ModName)
{
	if (IsModRegistered(ModName) == true)
		return;

	LogDataStore.insert(std::make_pair(ModName, new LogData()));
	_MESSAGE("Mod '%s' registered", ModName);
}

void LogManager::SetDefaultLog(const char* ModName, const char* LogName)
{
	if (IsModRegistered(ModName) == false)
		return;
	else if (LogName == NULL)
	{
		LogDataStore[ModName]->DefaultLogIndex = -1;
		_MESSAGE("Default log for '%s' reset", ModName);

		return;
	}
	else if (IsLogRegistered(ModName, LogName) == false)
		return;

	int Idx = 0;
	for (LogNameTableT::const_iterator Itr = LogDataStore[ModName]->RegisteredLogs.begin();
			Itr != LogDataStore[ModName]->RegisteredLogs.end();
			Itr++, Idx++)
	{
		if (!_stricmp(LogName, Itr->c_str()) && LogDataStore[ModName]->DefaultLogIndex != Idx)
		{
			LogDataStore[ModName]->DefaultLogIndex = Idx;
			_MESSAGE("Default log index for '%s' set to %d", ModName, Idx);

			return;
		}
	}
}

void LogManager::SetDefaultLog(const char* ModName, UInt32 Idx)
{
	if (IsModRegistered(ModName) == false)		
		return;

	if (Idx < LogDataStore[ModName]->RegisteredLogs.size())
		LogDataStore[ModName]->DefaultLogIndex = Idx;
	else
		LogDataStore[ModName]->DefaultLogIndex = -1;
}

const char* LogManager::GetDefaultLog(const char* ModName)
{
	if (IsModRegistered(ModName) == false)		
		return NULL;

	if (LogDataStore[ModName]->DefaultLogIndex != -1)
		return LogDataStore[ModName]->RegisteredLogs[LogDataStore[ModName]->DefaultLogIndex].c_str();
	else
		return NULL;
}

void LogManager::RegisterLog(const char* ModName, const char* LogName)
{
	if (IsModRegistered(ModName) == false)				
		return;
	else if (IsLogRegistered(ModName, LogName) == true)	
		return;

	LogDataStore[ModName]->RegisteredLogs.push_back(LogName);
	_MESSAGE("Mod '%s' registered '%s' as a log", ModName, LogName);
}

void LogManager::UnregisterLog(const char* ModName, const char* LogName)
{
	if (IsModRegistered(ModName) == false)
		return;
	else if (LogName == NULL)
		LogDataStore[ModName]->RegisteredLogs.clear();
	else if (IsLogRegistered(ModName, LogName) == false)
		return;

	int Idx = 0;
	for (LogNameTableT::const_iterator Itr = LogDataStore[ModName]->RegisteredLogs.begin();
		Itr != LogDataStore[ModName]->RegisteredLogs.end();
		Itr++, Idx++)
	{
		if (!_stricmp(LogName, Itr->c_str()))
		{
			LogDataStore[ModName]->RegisteredLogs.erase(Itr);
			_MESSAGE("Mod '%s' unregistered log '%s'", ModName, LogName);

			if (LogDataStore[ModName]->DefaultLogIndex == Idx )
				SetDefaultLog(ModName, (const char*)NULL);

			return;
		}
	}
}

void LogManager::ScribeToLog(const char* Message, const char* ModName, UInt32 RefID, UInt32 PrintC)
{
	std::string MessageBuffer(Message), LogName, FilePath, FormID;
	std::string::size_type PipeIdx = MessageBuffer.rfind("|");

	if (PipeIdx != std::string::npos)
	{
		LogName = MessageBuffer.substr(PipeIdx + 1, MessageBuffer.length() - 1);
		MessageBuffer.erase(PipeIdx, MessageBuffer.length() - 1);
	}

	UInt32 ScribeOperation = 0;
	if (LogName == "")
	{
		if (GetDefaultLog(ModName) == NULL)
			ScribeOperation = kScribe_ScriptLog;
		else
			ScribeOperation = kScribe_DefaultLog;
	}
	else if (!_stricmp(LogName.c_str(), "Script"))
		ScribeOperation = kScribe_ScriptLog;
	else if (IsLogRegistered(ModName, LogName.c_str()) == false)
		return;
	else
		ScribeOperation = kScribe_ModLog;

	if (PrintC)
	{
		if (MessageBuffer.length() < 512)
			Console_Print(MessageBuffer.c_str());
		else
			Console_Print_Long(MessageBuffer.c_str());
	}

	FilePath = Settings::kRootDirectory.GetData().s;

	switch(ScribeOperation)
	{
	case kScribe_ScriptLog:
		{
			char Buffer[0x10];
			sprintf_s(Buffer, sizeof(Buffer), "%08X", RefID);

			FormID = Buffer; FormID.erase(0,2);
			FilePath += LOGDIR_PERSCRIPT + std::string(ModName) + " - [XX]" + FormID + ".log";
		}

		break;
	case kScribe_ModLog:
		{
			FilePath += LOGDIR_PERMOD + LogName + ".log";
		}

		break;
	case kScribe_DefaultLog:
		{
			FilePath += LOGDIR_PERMOD + std::string(GetDefaultLog(ModName)) + ".log";
		}

		break;
	}

	ConScribeLog TempLog(FilePath.c_str(), ConScribeLog::kOpenMode_Append);
	TempLog.WriteOutput(MessageBuffer.c_str());
}


/* 	serialization data:
	CSMN - mod name. starting record	(string)
	CSRL - registered log				(string)
	CSDI - default log index			(signed int)

	CSEC - end of chunk. final record	(signed int/null)
	records are expected in the same order :
	CSMN, CSRL...(n), CSDI, CSEC, ...
*/

void LogManager::HandleGameLoad(OBSESerializationInterface* Interface)
{
	Purge();

	if (Settings::kSaveDataToCoSave.GetData().i == 0)
		return;

//	_MESSAGE("\nLoadGame callback! Reading records ...\n");

	UInt32	Type = 0, Version = 0, Length = 0;
	char Buffer[0x200] = {0}, ModName[0x200] = {0};
	int Idx = 0;

	while (Interface->GetNextRecordInfo(&Type, &Version, &Length))
	{
		switch (Type)
		{
		case 'CSRB':
			{
				Interface->ReadRecordData(&Buffer, Length);
				_MESSAGE("Discarded outdated record");
			}

			break;
		case 'CSMN':
			{
				Interface->ReadRecordData(&ModName, Length);
				ModName[Length] = 0;

				const ModEntry* ParentMod = (*g_dataHandler)->LookupModByName(ModName);
				if (ModName == NULL || ParentMod == NULL || ParentMod->IsLoaded() == false)
				{
					_MESSAGE("Mod %s is not loaded/Invalid Mod. Skipping corresponding records ...", ModName);

					do 
					{ 
						Interface->GetNextRecordInfo(&Type, &Version, &Length);
					}
					while (Type != 'CSEC');

					continue;
				}

				RegisterMod(ModName);

				Interface->GetNextRecordInfo(&Type, &Version, &Length);

				while (Type != 'CSEC')
				{
					switch (Type)
					{
					case 'CSRL':
						Interface->ReadRecordData(&Buffer, Length);
						Buffer[Length] = 0;
						RegisterLog(ModName, Buffer);

						break;
					case 'CSDI':
						Interface->ReadRecordData(&Idx, Length);
						SetDefaultLog(ModName, Idx);

						break;
					default:
						_MESSAGE("Error encountered while loading data from cosave - Unexpected type %.4s", &Type);

						break;
					}

					Interface->GetNextRecordInfo(&Type, &Version, &Length);
				}

				Interface->ReadRecordData(&Idx, Length);
			}

			break;
		}
	}


	if (Settings::kLogBackups.GetData().i == -1)
	{
		std::string LogDirectory(Settings::kRootDirectory.GetData().s);

		PerformHouseKeeping((std::string(LogDirectory + LOGDIR_PERMOD)).c_str(), "*.log", kHouseKeeping_BackupLogs);
		PerformHouseKeeping((std::string(LogDirectory + LOGDIR_PERSCRIPT)).c_str(), "*.log", kHouseKeeping_AppendHeaders);
	}
}

void LogManager::HandleGameSave(OBSESerializationInterface* Interface)
{
	if (Settings::kSaveDataToCoSave.GetData().i == 0)
		return;

//	_MESSAGE("\nSaveGame callback! Dumping records ...");

	int EC = 0;
	for (LogDataTableT::const_iterator Itr = LogDataStore.begin(); Itr != LogDataStore.end(); Itr++)
	{
		Interface->WriteRecord('CSMN', kSaveVersion, Itr->first.c_str(), Itr->first.length());

		LogNameTableT::const_iterator ItrVec = Itr->second->RegisteredLogs.begin();
		while (ItrVec != Itr->second->RegisteredLogs.end())
		{
			Interface->WriteRecord('CSRL', kSaveVersion, ItrVec->c_str(), ItrVec->length());
			ItrVec++;
		}

		Interface->WriteRecord('CSDI', kSaveVersion, &Itr->second->DefaultLogIndex, sizeof(Itr->second->DefaultLogIndex));
		Interface->WriteRecord('CSEC', kSaveVersion, &EC, 1);
	}
}

void LogManager::DeleteSliceFromLog(const char* ModName, const char* LogName, UInt32 Lower, UInt32 Upper)
{
	if (IsModRegistered(ModName) == false)
		return;
	else if (IsLogRegistered(ModName, LogName) == false)
		return;

	std::string LogPath =  std::string(Settings::kRootDirectory.GetData().s) + LOGDIR_PERMOD + LogName + ".log";

	ConScribeLog TempLog(LogPath.c_str(), ConScribeLog::kOpenMode_Read);
	TempLog.DeleteSlice(Lower, Upper);
}

int LogManager::GetLogLineCount(const char* ModName, const char* LogName)
{
	if (IsModRegistered(ModName) == false)
		return 0;
	else if (IsLogRegistered(ModName, LogName) == false)
		return 0;

	std::string LogPath =  std::string(Settings::kRootDirectory.GetData().s) + LOGDIR_PERMOD + LogName + ".log";

	ConScribeLog TempLog(LogPath.c_str(), ConScribeLog::kOpenMode_Read);
	return TempLog.GetLineCount();
}

bool LogManager::Initialize( void )
{
	std::string LogDirectory = Settings::kRootDirectory.GetData().s;
	LogDirectory += "\\";

	if ((CreateDirectory((LogDirectory + LOGDIR).c_str(), NULL) == 0 && GetLastError() != ERROR_ALREADY_EXISTS)||
		(CreateDirectory((LogDirectory + LOGDIR_PERMOD).c_str(), NULL) == 0 && GetLastError() != ERROR_ALREADY_EXISTS)||
		(CreateDirectory((LogDirectory + LOGDIR_PERSCRIPT).c_str(), NULL) == 0 && GetLastError() != ERROR_ALREADY_EXISTS))
	{
		_MESSAGE("Error encountered while creating log directories - Error %d", GetLastError());
		return false;
	}

	_MESSAGE("\nBacking up logs...\n");

	PerformHouseKeeping((std::string(LogDirectory + LOGDIR_PERSCRIPT)).c_str(), "*.log", kHouseKeeping_BackupLogs);
	PerformHouseKeeping((std::string(LogDirectory + LOGDIR_PERMOD)).c_str(), "*.log", kHouseKeeping_BackupLogs);
	PerformHouseKeeping((std::string(LogDirectory + LOGDIR)).c_str(), "Static Log.log", kHouseKeeping_BackupLogs);

	std::string ConsoleLogPath(LogDirectory); ConsoleLogPath += LOGDIR;

	if (!_stricmp(Settings::kScribeMode.GetData().s, "Static"))
		ConsoleLogPath += "Static Log.log";
	else
	{
		std::string TimeString;
		SME::MiscGunk::GetTimeString(TimeString, Settings::kTimeFormat.GetData().s);

		ConsoleLogPath += "Log of " + TimeString + ".log";
	}

	ConsoleLog::Instance.Open(ConsoleLogPath.c_str(), ConScribeLog::kOpenMode_Write);

	return true;
}

void LogManager::PerformHouseKeeping( const char* DirectoryPath, const char* File, UInt32 Operation )
{
	for (IDirectoryIterator Itr(DirectoryPath, File); !Itr.Done(); Itr.Next())
	{
		switch (Operation)
		{
		case kHouseKeeping_BackupLogs:
			BackupLog(DirectoryPath, Itr.Get()->cFileName);

			break;
		case kHouseKeeping_AppendHeaders:
			std::string FileName(Itr.Get()->cFileName), KeinExtension(FileName.substr(0, FileName.find(".")));
			std::string DirPath(DirectoryPath), ScriptLogDir(LOGDIR_PERSCRIPT); DirPath += "\\";

			SME::StringHelpers::MakeLower(DirPath); SME::StringHelpers::MakeLower(ScriptLogDir);

			if (DirPath.find(ScriptLogDir.c_str()) == std::string::npos && IsLogRegistered(KeinExtension.c_str()))
			{
				ConScribeLog SavedLog(Itr.GetFullPath().c_str(), ConScribeLog::kOpenMode_Append);
				SavedLog.AppendLoadHeader();
			}

			break;
		}
	}
}

void LogManager::BackupLog( std::string FilePath, std::string FileName )
{
	int NoOfBackups = Settings::kLogBackups.GetData().i, ID = 0;

	if (NoOfBackups == -1)
		return;													// a time header is appended instead
	else if (NoOfBackups > kMaxBackups)
		NoOfBackups = kMaxBackups;

	char Buffer[0x08];
	std::string FullPath(FilePath + "\\" + FileName), FileNameExt(FileName.substr(0, FileName.find(".")));

	sprintf_s(Buffer, sizeof(Buffer), "%d", ID);
	std::fstream FileStream(std::string(FilePath + "\\" + FileNameExt + ".log" + std::string(Buffer)).c_str(), std::fstream::in);

	while (FileStream.fail() && ID <= NoOfBackups)
	{
		ID++;
		sprintf_s(Buffer, sizeof(Buffer), "%d", ID);

		FileStream.close();
		FileStream.clear();
		FileStream.open(std::string(FilePath + "\\" + FileNameExt + ".log" + std::string(Buffer)).c_str(), std::fstream::in);
	}

	FileStream.close();
	FileStream.clear();

	if (ID == NoOfBackups)
	{									// delete file
		if (DeleteFileA(FullPath.c_str()))
			_MESSAGE("Deleted '%s'", FullPath.c_str());
		else
		{
			_MESSAGE("Couldn't delete '%s' - Error %d", FullPath.c_str(), GetLastError());
		}

		return;
	}
	else if (ID > NoOfBackups)									// first backup (log0)
		ID = 0;
	else														// increment backup ID (log0 -> log1)
		ID++;

	sprintf_s(Buffer, sizeof(Buffer), "%d", ID);
	std::string NewName(FilePath + "\\" + FileNameExt + ".log" + std::string(Buffer));

	if (MoveFileEx(FullPath.c_str(), NewName.c_str(), MOVEFILE_REPLACE_EXISTING))
		_MESSAGE("Renamed '%s' to '%s'", FullPath.c_str(), NewName.c_str());
	else
	{
		_MESSAGE("Couldn't rename '%s' - Error %d", FullPath.c_str(), GetLastError());
	}
}

void LogManager::GetRegisteredLogs( const char* ModName, LogNameTableT& Out )
{
	if (IsModRegistered(ModName) == false)
		return;

	Out = LogDataStore[ModName]->RegisteredLogs;
}

