#include "ConScribeInternals.h"

// HOOKS

void __declspec(naked) ConsolePrintHook(void)
{
    __asm
    {
		pushad
		mov		ecx, [ecx]
		mov		g_HookMessage, ecx
	}

	g_ConsoleLog->WriteOutput(g_HookMessage);
										
	__asm
	{
		popad

		push	ecx
		mov		ecx, eax
		call    kConsolePrintCallAddr

		jmp		kConsolePrintRetnAddr
	}
} 

ConScribeINIManager*		g_INIManager = new ConScribeINIManager();
char						g_Buffer[0x4000];

void ConScribeINIManager::Initialize()
{
	_MESSAGE("INI Path: %s", GetINIPath());
	bool CreateINI = false;
	std::fstream INIStream(GetINIPath(), std::fstream::in);

	if (INIStream.fail()) 
	{
		CreateINI = true;			
		_MESSAGE("INI File not found; Creating one...");
	}

	INIStream.close();
	INIStream.clear();

	RegisterSetting(new INISetting(this, "ScribeMode", "ConsoleLog", "Static", "The mode of logging used by the console log(s)"), (CreateINI == false));
	RegisterSetting(new INISetting(this, "Includes", "ConsoleLog", "", "Include string"), (CreateINI == false));
	RegisterSetting(new INISetting(this, "Excludes", "ConsoleLog", "", "Exclude string"), (CreateINI == false));
	RegisterSetting(new INISetting(this, "TimeFormat", "General", "%m-%d-%Y %H-%M-%S", "Data format string"), (CreateINI == false));
	RegisterSetting(new INISetting(this, "LogBackups", "General", "-1", "Number of log backups"), (CreateINI == false));
	RegisterSetting(new INISetting(this, "RootDirectory", "General", "Default", "Location of the ConScribe logs folder"), (CreateINI == false));
	RegisterSetting(new INISetting(this, "SaveDataToCoSave", "General", "1", "Save log registeration data to the OBSE co-save"), (CreateINI == false));

	if (CreateINI)		SaveSettingsToINI();
	else				ReadSettingsFromINI();
}

// CONSCRIBE LOG

ConScribeLog::ConScribeLog(const char* FileName, OpenMode Mode)
{
	FilePath = new char[0x104];
	Open(FileName, Mode);
}

void ConScribeLog::Open(const char* FileName, OpenMode Mode)
{
	if (FileStream.is_open())	Close();

	switch(Mode)
	{
	case e_OutAp:
		FileStream.open(FileName, std::fstream::out|std::fstream::app);
		break;
	case e_Out:
		FileStream.open(FileName, std::fstream::out);
		break;
	case e_In:
		FileStream.open(FileName, std::fstream::in);
		break;
	}
	sprintf_s(FilePath, 0x104, "%s", FileName);
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
	if (!Message)				return;
	FileStream << Message << std::endl;
}

void ConScribeLog::AppendLoadHeader()
{
	std::string TimeString(GetTimeString()), Padding(TimeString.length() + 28, '=');
	
	FileStream << Padding << std::endl
			  << "Game Instance : " << g_SessionCount << " | Time : " << TimeString << std::endl 
			  << Padding << std::endl;
}

void ConScribeLog::ReadAllLines(std::vector<std::string>& LogContents)
{
	if (!FileStream.fail())
	{
		while (!FileStream.eof())
		{
			FileStream.getline(g_Buffer, sizeof(g_Buffer));
			LogContents.push_back(g_Buffer);
		}
	}
	else
		LogContents.push_back("Log file not found");
}

UInt32 ConScribeLog::GetLineCount(void)
{
	UInt32 LineCount = 0;

	if (!FileStream.fail())	
	{
		while (!FileStream.eof()) 
		{
			FileStream.getline(g_Buffer, sizeof(g_Buffer));
			LineCount++;
		}
	}
	return LineCount;
}

void ConScribeLog::DeleteSlice(UInt32 Lower, UInt32 Upper)
{
	if (Lower > Upper)	return;

	std::string TempPath = std::string(FilePath) + ".tmp";
	std::fstream TempLog(TempPath.c_str(), std::fstream::out);
	UInt32 LineCount = 1;

	if (!FileStream.fail())
	{
		while (!FileStream.eof()) 
		{
			FileStream.getline(g_Buffer, sizeof(g_Buffer));
			if (LineCount > Upper || LineCount < Lower)
				TempLog << g_Buffer << "\n";			
			LineCount++;
		}

		TempLog.flush();
		TempLog.close();
		TempLog.clear();
		Close();

		if (!MoveFileEx(TempPath.c_str(), FilePath, MOVEFILE_REPLACE_EXISTING))
		{
			_MESSAGE("DeleteSlice failed! Existing Name = %s ; New Name = %s", TempPath.c_str(), FilePath);
			LogWinAPIErrorMessage(GetLastError());
		}

		Open(FilePath, e_In);
	}
}

// CONSOLE LOG

void ConsoleLog::WriteOutput(const char* Message)
{
	if (!Message)												return;
	std::string Includes(g_INIManager->GetINIStr("Includes")), 
				Excludes(g_INIManager->GetINIStr("Excludes"));

	if (Includes != "" && !GetInString(Message, Includes))		return;
	if (Excludes != "" && GetInString(Message, Includes) > 0)	return;

	FileStream << Message << std::endl;
}

void ConsoleLog::HandleLoadCallback(void)
{
	if (!_stricmp(g_INIManager->GetINIStr("ScribeMode"), "PerLoad"))
	{
		g_ConsoleLog->Open(std::string(std::string(g_INIManager->GetINIStr("RootDirectory")) + "ConScribe Logs\\Log of " + GetTimeString() + ".log").c_str(), ConScribeLog::e_Out);
	}
	else
		AppendLoadHeader();
}


// LOG DATA

LogData::LogData()
{
	DefaultLog = -1;
	RegisteredLogs.reserve(5);
}


// LOG MANAGER

LogManager*	LogManager::Singleton = NULL;

LogManager*	LogManager::GetSingleton()
{
	if (!Singleton)
	{
		Singleton = new LogManager;
	}
	return Singleton;
}

void LogManager::DumpDatabase()
{
	_MESSAGE("Registered Logs\n================\n");

	for (_LogDB::const_iterator Itr = LogDB.begin(); Itr != LogDB.end(); Itr++)
	{
		_MESSAGE("%s : [ default: %s ]", Itr->first.c_str(), GetDefaultLog(Itr->first.c_str()));

		std::vector<std::string>::const_iterator ItrVec = Itr->second->RegisteredLogs.begin();
		while (ItrVec != Itr->second->RegisteredLogs.end())
		{
			_MESSAGE("\t%s", ItrVec->c_str());
			ItrVec++;
		}
	}
	_MESSAGE("\n");
}

void LogManager::PurgeDatabase()
{
	for (_LogDB::iterator Itr = LogDB.begin(); Itr != LogDB.end(); Itr++)
		delete Itr->second;

	LogDB.clear();
}

bool LogManager::IsModRegistered(const char* ModName)
{
	for (_LogDB::iterator Itr = LogDB.begin(); Itr != LogDB.end(); Itr++)
	{
		if (!_stricmp(ModName, Itr->first.c_str()))
			return true;
	}
	return false;
}

bool LogManager::IsLogRegistered(const char* ModName, const char* LogName)
{
	if (!IsModRegistered(ModName))		return false;

	std::vector<std::string>* LogList = &(LogDB.find(ModName)->second->RegisteredLogs);
	for (std::vector<std::string>::iterator Itr = LogList->begin(); Itr != LogList->end(); Itr++) 
	{
		if (!_stricmp(LogName, Itr->c_str()))
			return true;
	}
	return false;
}

bool LogManager::IsLogRegistered(const char* LogName)
{
	for (_LogDB::const_iterator Itr = LogDB.begin(); Itr != LogDB.end(); Itr++)
	{
		std::vector<std::string>::const_iterator ItrVec = Itr->second->RegisteredLogs.begin();
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
	if (IsModRegistered(ModName))		return;

	LogDB.insert(std::make_pair<const char*, LogData*>(ModName, new LogData));
	_MESSAGE("Mod '%s' registered", ModName);
}

void LogManager::SetDefaultLog(const char* ModName, const char* LogName)
{
	if (!IsModRegistered(ModName))
		return;
	else if (!LogName)
	{
		LogDB[ModName]->DefaultLog = -1;
		_MESSAGE("Default log for '%s' reset", ModName);
		return;
	}
	else if (!IsLogRegistered(ModName, LogName))
		return;

	int Idx = 0;
	for (std::vector<std::string>::const_iterator Itr = LogDB[ModName]->RegisteredLogs.begin(); Itr != LogDB[ModName]->RegisteredLogs.end(); Itr++, Idx++)
	{
		if (!_stricmp(LogName, Itr->c_str()) && LogDB[ModName]->DefaultLog != Idx)
		{
			LogDB[ModName]->DefaultLog = Idx;
			_MESSAGE("Default log index for '%s' set to %d", ModName, Idx);
			return;
		}
	}
}

void LogManager::SetDefaultLog(const char* ModName, UInt32 Idx)
{
	if (!IsModRegistered(ModName))		return;

	if (Idx < LogDB[ModName]->RegisteredLogs.size())
		LogDB[ModName]->DefaultLog = Idx;
	else 
		LogDB[ModName]->DefaultLog = -1;
}

const char* LogManager::GetDefaultLog(const char* ModName)
{
	if (!IsModRegistered(ModName))		return NULL;

	if (LogDB[ModName]->DefaultLog != -1)
		return LogDB[ModName]->RegisteredLogs[LogDB[ModName]->DefaultLog].c_str();
	else return NULL;
}

void LogManager::RegisterLog(const char* ModName, const char* LogName)
{
	if (!IsModRegistered(ModName))				return;
	else if (IsLogRegistered(ModName, LogName))	return;

	LogDB[ModName]->RegisteredLogs.push_back(LogName);
	_MESSAGE("Mod '%s' registered '%s' as a log", ModName, LogName);
}

void LogManager::UnregisterLog(const char* ModName, const char* LogName)
{
	if (!IsModRegistered(ModName))					return;
	else if (!IsLogRegistered(ModName, LogName))	return;

	int Idx = 0;
	for (std::vector<std::string>::const_iterator Itr = LogDB[ModName]->RegisteredLogs.begin(); Itr != LogDB[ModName]->RegisteredLogs.end(); Itr++, Idx++) 
	{
		if (!_stricmp(LogName, Itr->c_str()))
		{
			LogDB[ModName]->RegisteredLogs.erase(Itr);
			_MESSAGE("Mod '%s' unregistered log '%s'", ModName, LogName);
			if (LogDB[ModName]->DefaultLog == Idx )
				SetDefaultLog(ModName, (const char*)NULL);

			return;
		}
	}
}

void LogManager::UnregisterLog(const char* ModName)
{
	if (!IsModRegistered(ModName))	
		return;

	LogDB[ModName]->RegisteredLogs.clear();
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
		if (!GetDefaultLog(ModName))
			ScribeOperation = e_Script;
		else	
			ScribeOperation = e_Default;
	}
	else if (!_stricmp(LogName.c_str(), "Script"))
		ScribeOperation = e_Script;
	else if (!IsLogRegistered(ModName, LogName.c_str()))
		return;
	else								
		ScribeOperation = e_Mod;

	if (PrintC)
	{
		if (MessageBuffer.length() < 512)
			Console_Print(MessageBuffer.c_str());
		else
			Console_Print_Long(MessageBuffer.c_str());
	}

	switch(ScribeOperation)
	{
	case e_Script:
		char Buffer[0x10];
		_sprintf_p(Buffer, sizeof(Buffer), "%08X", RefID);
		FormID = Buffer; FormID.erase(0,2);
		FilePath = std::string(g_INIManager->GetINIStr("RootDirectory")) + "ConScribe Logs\\Per-Script\\" + std::string(ModName) + " - [XX]" + FormID + ".log";
		break;
	case e_Mod:
		FilePath = std::string(g_INIManager->GetINIStr("RootDirectory")) + "ConScribe Logs\\Per-Mod\\" + LogName + ".log";
		break;
	case e_Default:
		FilePath = std::string(g_INIManager->GetINIStr("RootDirectory")) + "ConScribe Logs\\Per-Mod\\" + std::string(GetDefaultLog(ModName)) + ".log";
		break;
	}

	ConScribeLog TempLog(FilePath.c_str(), ConScribeLog::e_OutAp);
	TempLog.WriteOutput(MessageBuffer.c_str());
}

LogData* LogManager::GetModLogData(const char* ModName)
{
	if (!IsModRegistered(ModName))	
		return NULL;

	return LogDB[ModName];
}

/* 	serialization data:
	CSMN - mod name. starting record	(string)
	CSRL - registered log				(string)
	CSDI - default log index			(signed int)					

	CSEC - end of chunk. final record	(signed int/null)
	records are expected in the same order :
	CSMN, CSRL...(n), CSDI, CSEC, ... 
*/

void LogManager::DoLoadCallback(OBSESerializationInterface* Interface)
{																																					
	PurgeDatabase();
	if (g_INIManager->GetINIInt("SaveDataToCoSave"))
		return;

	_MESSAGE("\nLoadGame callback! Reading records ...\n");					
	UInt32	Type, Version, Length;											
	char Buffer[0x200], ModName[0x200];										
	int Idx;

	while (Interface->GetNextRecordInfo(&Type, &Version, &Length))
	{
		switch (Type)
		{
		case 'CSRB':		
			Interface->ReadRecordData(&Buffer, Length);
			ConvertDeprecatedRecordCSRB(Buffer);
			break;
		case 'CSMN':
			Interface->ReadRecordData(&ModName, Length);
			ModName[Length] = 0;

			const ModEntry* ParentMod = (*g_dataHandler)->LookupModByName(ModName);
			if (!ModName || !ParentMod || !ParentMod->IsLoaded())
			{
				_MESSAGE("Mod %s is not loaded/Invalid Mod. Skipping corresponding records ...", ModName);
				do { Interface->GetNextRecordInfo(&Type, &Version, &Length); } 
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
			break;
		} 
	}

	_MESSAGE("\nDone loading from cosave; Dumping database ...\n");
	DumpDatabase();
}

void LogManager::DoSaveCallback(OBSESerializationInterface* Interface)
{	
	if (g_INIManager->GetINIInt("SaveDataToCoSave"))
		return;

	_MESSAGE("\nSaveGame callback! Dumping records ...");
	int EC = 0;
	for (_LogDB::const_iterator Itr = LogDB.begin(); Itr != LogDB.end(); Itr++) 
	{
		Interface->WriteRecord('CSMN', SAVE_VERSION, Itr->first.c_str(), Itr->first.length());

		std::vector<std::string>::const_iterator ItrVec = Itr->second->RegisteredLogs.begin();
		while (ItrVec != Itr->second->RegisteredLogs.end()) 
		{
			Interface->WriteRecord('CSRL', SAVE_VERSION, ItrVec->c_str(), ItrVec->length());			
			ItrVec++;
		}

		Interface->WriteRecord('CSDI', SAVE_VERSION, &Itr->second->DefaultLog, sizeof(Itr->second->DefaultLog));	
		Interface->WriteRecord('CSEC', SAVE_VERSION, &EC, 1);	
	}

	_MESSAGE("Done dumping database to cosave\n");
}

void LogManager::ConvertDeprecatedRecordCSRB(std::string Record)
{
	_MESSAGE("\tDeprecated record 'CSRB'. Converting ...");
	std::string::size_type DelimiterIdx = Record.find("|");
	std::string ModName = Record.substr(0, DelimiterIdx), LogList = Record.erase(0, DelimiterIdx + 1);

	const ModEntry* ParentMod = (*g_dataHandler)->LookupModByName(ModName.c_str());
	if (!ParentMod || !ParentMod->IsLoaded())
	{
		_MESSAGE("Mod %s is not loaded. Skipping corresponding records ...", ModName.c_str());
		return;
	}

	RegisterMod(ModName.c_str());
	DelimiterIdx = 0;

	while (DelimiterIdx <= LogList.length())
	{
		DelimiterIdx = LogList.find(";");
		if (DelimiterIdx == std::string::npos)
			break;

		RegisterLog(ModName.c_str(), LogList.substr(0, DelimiterIdx).c_str());
		LogList.erase(0, DelimiterIdx + 1);
	}

	std::string DefaultLog = LogList.erase(0, 1);	
	SetDefaultLog(ModName.c_str(), DefaultLog.c_str());
}

void LogManager::DeleteSliceFromLog(const char* ModName, const char* LogName, UInt32 Lower, UInt32 Upper)
{
	if (!IsModRegistered(ModName))
		return;
	else if (!IsLogRegistered(ModName, LogName))
		return;

	std::string LogPath =  std::string(g_INIManager->GetINIStr("RootDirectory")) + "ConScribe Logs\\Per-Mod\\" + LogName + ".log";
	ConScribeLog TempLog(LogPath.c_str(), ConScribeLog::e_In);
	TempLog.DeleteSlice(Lower, Upper);
}

int LogManager::GetLogLineCount(const char* ModName, const char* LogName)
{
	if (!IsModRegistered(ModName))		
		return 0;
	else if (!IsLogRegistered(ModName, LogName))
		return 0;

	std::string LogPath =  std::string(g_INIManager->GetINIStr("RootDirectory")) + "ConScribe Logs\\Per-Mod\\" + LogName + ".log";
	ConScribeLog TempLog(LogPath.c_str(), ConScribeLog::e_In);
	return TempLog.GetLineCount();
}



// MISC

std::string GetTimeString(void)
{
	char TimeString[0x200];
	__time32_t TimeData;
	tm LocalTime;

	_time32(&TimeData);
	_localtime32_s(&LocalTime, &TimeData);

	if (!strftime(TimeString, sizeof(TimeString), g_INIManager->GetINIStr("TimeFormat"), &LocalTime))
	{
		_MESSAGE("Couldn't parse TimeFormat string. Using default format");
		strftime(TimeString, sizeof(TimeString), "%m-%d-%Y %H-%M-%S", &LocalTime);
	}
	return TimeString;
}

UInt32 GetInString(std::string String1, std::string String2)
{
	std::string::size_type Idx = 0;
	std::string Extract;
	UInt32 Hits = 0;

	if (String2[String2.length() - 1] != ';')
		String2 += ";";

	while (String2.find(";") != std::string::npos)
	{
		Idx = String2.find(";");
		Extract = String2.substr(0, Idx);	

		if (String1.find(Extract) != std::string::npos)
			Hits++;

		String2.erase(0, Idx + 1);
	}
	return Hits;
}

void LogWinAPIErrorMessage(DWORD ErrorID)
{
	LPVOID ErrorMsg;
	FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | 
		FORMAT_MESSAGE_FROM_SYSTEM |
		FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL,
		ErrorID,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR) &ErrorMsg,
		0, NULL );

	_MESSAGE("\tError Message: %s", ErrorMsg); 
	LocalFree(ErrorMsg);
}

bool CreateLogDirectories()
{
	std::string LogDirectory = g_INIManager->GetINIStr("RootDirectory");
	if (LogDirectory[LogDirectory.length() - 1] != '\\')
		LogDirectory += "\\";			// append leading backward slash when not found

	if ((CreateDirectory((LogDirectory + "ConScribe Logs").c_str(), NULL) == 0 && GetLastError() != ERROR_ALREADY_EXISTS)||
		(CreateDirectory((LogDirectory + "ConScribe Logs\\Per-Script").c_str(), NULL) == 0 && GetLastError() != ERROR_ALREADY_EXISTS)||
		(CreateDirectory((LogDirectory + "ConScribe Logs\\Per-Mod").c_str(), NULL) == 0 && GetLastError() != ERROR_ALREADY_EXISTS))
	{
		_MESSAGE("Error encountered while creating log directories");
		LogWinAPIErrorMessage(GetLastError());
		return false;
	}

	return true;
}

void PerformHouseKeeping(const char* DirectoryPath, const char* File, HouseKeepingOps Operation)
{
	for (IDirectoryIterator Itr(DirectoryPath, File); !Itr.Done(); Itr.Next())
	{
		switch (Operation)
		{
		case e_BackupLogs:
			DoBackup(DirectoryPath, Itr.Get()->cFileName);
			break;
		case e_AppendHeaders:
			std::string FileName(Itr.Get()->cFileName), 
						FileNameExt(FileName.substr(0, FileName.find(".")));
			if (std::string(DirectoryPath).find("Per-Script") == std::string::npos && LogManager::GetSingleton()->IsLogRegistered(FileNameExt.c_str()))		
				AppendHeader(Itr.GetFullPath());
			break;
		}
	}
}

void DoBackup(std::string FilePath, std::string FileName)
{
	int NoOfBackups = g_INIManager->GetINIInt("LogBackups"), ID = 0;
	if (NoOfBackups == -1)
		return;													// a time header is appended instead
	else if (NoOfBackups > MAX_BACKUPS)
		NoOfBackups = MAX_BACKUPS;


	char Buffer[0x08];
	std::string FullPath(FilePath + "\\" + FileName), FileNameExt(FileName.substr(0, FileName.find(".")));

	sprintf_s(Buffer, sizeof(Buffer), "%d", ID);
	std::fstream FileStream(std::string(FilePath + "\\" + FileNameExt + ".log" + std::string(Buffer)).c_str(), std::fstream::in);
	while (FileStream.fail() && ID <= NoOfBackups)
	{
		ID++;
		sprintf_s(Buffer, sizeof(Buffer), "%d", ID);
		FileStream.close(), FileStream.clear();
		FileStream.open(std::string(FilePath + "\\" + FileNameExt + ".log" + std::string(Buffer)).c_str(), std::fstream::in);
	}

	FileStream.close(), FileStream.clear();

	if (ID == NoOfBackups)
	{									// delete file
		if (DeleteFileA(FullPath.c_str()))	
			_MESSAGE("Deleted '%s'", FullPath.c_str());
		else 
		{
			_MESSAGE("Couldn't delete '%s'", FullPath.c_str());
			LogWinAPIErrorMessage(GetLastError());			
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
		_MESSAGE("Couldn't rename '%s'", FullPath.c_str());
		LogWinAPIErrorMessage(GetLastError());
	}
}

void AppendHeader(std::string FilePath)
{
	ConScribeLog SavedLog(FilePath.c_str(), ConScribeLog::e_OutAp);
	SavedLog.AppendLoadHeader();
}