#include "ConScribeInternals.h"

// INI SETTING

INISetting::INISetting(const char* Key, const char* Section, const char* DefaultValue)
{
	char Buffer[0x200];
	this->Key = (Key);	
	if (!INIManager::GetSingleton()->ShouldCreateINI()) {
		GetPrivateProfileStringA(Section, Key, DefaultValue, Buffer, sizeof(Buffer), INIManager::GetSingleton()->GetINIPath());
		Value = Buffer;
	}
	else {
		WritePrivateProfileStringA(Section, Key, DefaultValue, INIManager::GetSingleton()->GetINIPath());
		Value = DefaultValue;
	}
}

// INI MANAGER

std::string INIManager::INIFile = "";
INIManager* INIManager::Singleton = NULL;

INIManager::INIManager()
{
	CreateINI = false;
}

INIManager* INIManager::GetSingleton()
{
	if (!Singleton) {
		Singleton = new INIManager();
	}
	return Singleton;
}

void INIManager::RegisterSetting(INISetting *Setting)
{
	INIList.push_back(Setting);
}

INISetting* INIManager::FetchSetting(const char* Key)
{
	for (_INIList::const_iterator Itr = INIList.begin(); Itr != INIList.end(); Itr++) {
		if (!_stricmp((*Itr)->Key.c_str(), Key)) {
			return *Itr;
		}
	}
	return NULL;
}

void INIManager::Initialize(const OBSEInterface* OBSE)
{
	SetINIPath(std::string(OBSE->GetOblivionDirectory()) + "Data\\OBSE\\Plugins\\ConScribe.ini");

	_MESSAGE("INI Path: %s", GetINIPath());
	std::fstream INIStream(GetINIPath(), std::fstream::in);

	if (INIStream.fail()) {
		CreateINI = true;			
		_MESSAGE("INI File not found; Creating one...");
	}

	INIStream.close();
	INIStream.clear();

	RegisterSetting(new INISetting("ScribeMode", "ConsoleLog", "Static"));
	RegisterSetting(new INISetting("Includes", "ConsoleLog", ""));
	RegisterSetting(new INISetting("Excludes", "ConsoleLog", ""));
	RegisterSetting(new INISetting("TimeFormat", "General", "%m-%d-%Y %H-%M-%S"));
	RegisterSetting(new INISetting("LogBackups", "General", "-1"));
	RegisterSetting(new INISetting("RootDirectory", "General", "Default"));

	if (!_stricmp(GET_INI("RootDirectory")->GetValueAsString(), "Default")) {
		GET_INI("RootDirectory")->SetValue(OBSE->GetOblivionDirectory());
		_MESSAGE("Root set to default directory");
	}
}

// CONSCRIBE LOG

ConScribeLog::ConScribeLog(const char* FileName, OpenMode Mode)
{
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
}

void ConScribeLog::Close()
{
	FileStream.close();
	FileStream.clear();
}

ConScribeLog::~ConScribeLog()
{
	Close();
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

std::vector<std::string> ConScribeLog::ReadAllLines()
{
	std::vector<std::string> LogContents;		// passed as a STL string to prevent identical char pointers
	char Buffer[0x4000];

	if (!FileStream.fail())	{
		while (!FileStream.eof()) {
			FileStream.getline(Buffer, sizeof(Buffer));
			LogContents.push_back(Buffer);
		}
	}
	else
		LogContents.push_back("Log file not found");

	return LogContents;
}

void ConsoleLog::WriteOutput(const char* Message)
{
	if (!Message)												return;
	std::string Includes(GET_INI("Includes")->GetValueAsString()), 
				Excludes(GET_INI("Excludes")->GetValueAsString());

	if (Includes != "" && !GetInString(Message, Includes))		return;
	if (Excludes != "" && GetInString(Message, Includes) > 0)	return;

	FileStream << Message << std::endl;
}

void ConsoleLog::HandleLoadCallback(void)
{
	if (!_stricmp(GET_INI("ScribeMode")->GetValueAsString(), "PerLoad")) {
		g_ConsoleLog->Open(std::string(std::string(GET_INI("RootDirectory")->GetValueAsString()) + "ConScribe Logs\\Log of " + GetTimeString() + ".log").c_str(), ConScribeLog::e_Out);
	}
	else		AppendLoadHeader();
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
	if (!Singleton) {
		Singleton = new LogManager;
	}
	return Singleton;
}

void LogManager::DumpDatabase()
{
	_MESSAGE("Registered Logs\n================\n");

	for (_LogDB::const_iterator Itr = LogDB.begin(); Itr != LogDB.end(); Itr++) {
		_MESSAGE("%s : [ default: %s ]", Itr->first.c_str(), GetDefaultLog(Itr->first.c_str()));

		std::vector<std::string>::const_iterator ItrVec = Itr->second->RegisteredLogs.begin();
		while (ItrVec != Itr->second->RegisteredLogs.end()) {
			_MESSAGE("\t%s", ItrVec->c_str());
			ItrVec++;
		}
	}
	_MESSAGE("\n");
}

void LogManager::PurgeDatabase()
{
	for (_LogDB::iterator Itr = LogDB.begin(); Itr != LogDB.end(); Itr++) {
		delete Itr->second;
	}
	LogDB.clear();
}

bool LogManager::IsModRegistered(const char* ModName)
{
	for (_LogDB::iterator Itr = LogDB.begin(); Itr != LogDB.end(); Itr++) {
		if (!_stricmp(ModName, Itr->first.c_str()))
			return true;
	}
	return false;
}

bool LogManager::IsLogRegistered(const char* ModName, const char* LogName)
{
	if (!IsModRegistered(ModName))		return false;

	std::vector<std::string>* LogList = &(LogDB.find(ModName)->second->RegisteredLogs);
	for (std::vector<std::string>::iterator Itr = LogList->begin(); Itr != LogList->end(); Itr++) {
		if (!_stricmp(LogName, Itr->c_str()))
			return true;
	}
	return false;
}

bool LogManager::IsLogRegistered(const char* LogName)
{
	for (_LogDB::const_iterator Itr = LogDB.begin(); Itr != LogDB.end(); Itr++) {
		std::vector<std::string>::const_iterator ItrVec = Itr->second->RegisteredLogs.begin();
		while (ItrVec != Itr->second->RegisteredLogs.end()) {
			if (!_stricmp(LogName, ItrVec->c_str()))			return true;
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
	if (!IsModRegistered(ModName))					return;
	else if (!LogName) {
		LogDB[ModName]->DefaultLog = -1;
		_MESSAGE("Default log for '%s' reset", ModName);
		return;
	}
	else if (!IsLogRegistered(ModName, LogName))	return;

	int Idx = 0;
	for (std::vector<std::string>::const_iterator Itr = LogDB[ModName]->RegisteredLogs.begin(); Itr != LogDB[ModName]->RegisteredLogs.end(); Itr++, Idx++) {
		if (!_stricmp(LogName, Itr->c_str()) && LogDB[ModName]->DefaultLog != Idx) {
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
	for (std::vector<std::string>::const_iterator Itr = LogDB[ModName]->RegisteredLogs.begin(); Itr != LogDB[ModName]->RegisteredLogs.end(); Itr++, Idx++) {
		if (!_stricmp(LogName, Itr->c_str())) {
			LogDB[ModName]->RegisteredLogs.erase(Itr);
			_MESSAGE("Mod '%s' unregistered log '%s'", ModName, LogName);
			if (LogDB[ModName]->DefaultLog == Idx )		SetDefaultLog(ModName, (const char*)NULL);
			return;
		}
	}
}

void LogManager::ScribeToLog(const char* Message, const char* ModName, UInt32 RefID)
{
	if (!IsModRegistered(ModName))		return;

	std::string MessageBuffer(Message), LogName, FilePath, FormID;
	std::string::size_type PipeIdx = MessageBuffer.rfind("|");
	if (PipeIdx != std::string::npos) {
		LogName = MessageBuffer.substr(PipeIdx + 1, MessageBuffer.length() - 1);
		MessageBuffer.erase(PipeIdx, MessageBuffer.length() - 1);
	}

	UInt32 ScribeOperation = 0;
	if (LogName == "")	{
		if (!GetDefaultLog(ModName))						ScribeOperation = e_Script;
		else												ScribeOperation = e_Default;
	}
	else if (!_stricmp(LogName.c_str(), "Script"))			ScribeOperation = e_Script;
	else if (!IsLogRegistered(ModName, LogName.c_str()))	return;
	else													ScribeOperation = e_Mod;

	switch(ScribeOperation)
	{
	case e_Script:
		char Buffer[0x10];
		_sprintf_p(Buffer, sizeof(Buffer), "%08x", RefID);
		FormID = Buffer; FormID.erase(0,2);
		FilePath = std::string(GET_INI("RootDirectory")->GetValueAsString()) + "ConScribe Logs\\Per-Script\\" + std::string(ModName) + " - [XX]" + FormID + ".log";
		break;
	case e_Mod:
		FilePath = std::string(GET_INI("RootDirectory")->GetValueAsString()) + "ConScribe Logs\\Per-Mod\\" + LogName + ".log";
		break;
	case e_Default:
		FilePath = std::string(GET_INI("RootDirectory")->GetValueAsString()) + "ConScribe Logs\\Per-Mod\\" + std::string(GetDefaultLog(ModName)) + ".log";
		break;
	}

	ConScribeLog* TempLog = new ConScribeLog(FilePath.c_str(), ConScribeLog::e_OutAp);
	TempLog->WriteOutput(MessageBuffer.c_str());
	delete TempLog;
}

LogData* LogManager::GetModLogData(const char* ModName)
{
	if (!IsModRegistered(ModName))		return NULL;
	return LogDB[ModName];
}

void LogManager::DoLoadCallback(OBSESerializationInterface* Interface)		// record types:
{																			//		CSMN - mod name. starting record	(string)
	_MESSAGE("\nLoadGame callback! Reading records ...\n");					//		CSRL - registered log				(string)
	PurgeDatabase();														//		CSDI - default log index			(signed int)					
																			//		CSEC - end of chunk. final record	(signed int/null)
	UInt32	Type, Version, Length;											// records are expected in the same order :
	char Buffer[0x200], ModName[0x200];										//		CSMN, CSRL(N)..., CSDI, CSEC, ...
	int Idx;

	while (Interface->GetNextRecordInfo(&Type, &Version, &Length)) {
		switch (Type)
		{
		case 'CSRB':		
			Interface->ReadRecordData(&Buffer, Length);
			ConvertDeprecatedRecord(Buffer);
			break;
		case 'CSMN':
			Interface->ReadRecordData(&ModName, Length);
			ModName[Length] = 0;

			if (!ModName || !(*g_dataHandler)->LookupModByName(ModName)->IsLoaded()) {
				_MESSAGE("Mod %s is not loaded/Invalid Mod. Skipping corresponding records ...", ModName);
				do { Interface->GetNextRecordInfo(&Type, &Version, &Length); } 
				while (Type != 'CSEC');
				continue;
			}

			RegisterMod(ModName);

			Interface->GetNextRecordInfo(&Type, &Version, &Length);
			while (Type != 'CSEC') {
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
	_MESSAGE("\nSaveGame callback! Dumping records ...");

	int EC = 0;
	for (_LogDB::const_iterator Itr = LogDB.begin(); Itr != LogDB.end(); Itr++) {
		Interface->WriteRecord('CSMN', SAVE_VERSION, Itr->first.c_str(), Itr->first.length());

		std::vector<std::string>::const_iterator ItrVec = Itr->second->RegisteredLogs.begin();
		while (ItrVec != Itr->second->RegisteredLogs.end()) {
			Interface->WriteRecord('CSRL', SAVE_VERSION, ItrVec->c_str(), ItrVec->length());			
			ItrVec++;
		}

		Interface->WriteRecord('CSDI', SAVE_VERSION, &Itr->second->DefaultLog, sizeof(Itr->second->DefaultLog));	
		Interface->WriteRecord('CSEC', SAVE_VERSION, &EC, 1);	
	}

	_MESSAGE("Done dumping database to cosave\n");
}

void LogManager::ConvertDeprecatedRecord(std::string Record)
{
	_MESSAGE("+ Deprecated record 'CSRB'. Converting ...");
	std::string::size_type DelimiterIdx = Record.find("|");
	std::string ModName = Record.substr(0, DelimiterIdx), LogList = Record.erase(0, DelimiterIdx + 1);

	if (!(*g_dataHandler)->LookupModByName(ModName.c_str())->IsLoaded()) {
		_MESSAGE("Mod %s is not loaded. Skipping corresponding records ...", ModName);
		return;
	}

	RegisterMod(ModName.c_str());
	DelimiterIdx = 0;

	while (DelimiterIdx <= LogList.length()) {
		DelimiterIdx = LogList.find(";");
		if (DelimiterIdx == std::string::npos)			break;

		RegisterLog(ModName.c_str(), LogList.substr(0, DelimiterIdx).c_str());
		LogList.erase(0, DelimiterIdx + 1);
	}

	std::string DefaultLog = LogList.erase(0, 1);	
	SetDefaultLog(ModName.c_str(), DefaultLog.c_str());
}



// MISC

std::string GetTimeString(void)
{
	char TimeString[0x200];
	__time32_t TimeData;
	tm LocalTime;

	_time32(&TimeData);
	_localtime32_s(&LocalTime, &TimeData);

	if (!strftime(TimeString, sizeof(TimeString), GET_INI("TimeFormat")->GetValueAsString(), &LocalTime)) {
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

	if (String2[String2.length() - 1] != ';')				String2 += ";";

	while (String2.find(";") != std::string::npos) {
		Idx = String2.find(";");
		Extract = String2.substr(0, Idx);	

		if (String1.find(Extract) != std::string::npos)		Hits++;
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
}

bool CreateLogDirectories()
{
	std::string LogDirectory = GET_INI("RootDirectory")->GetValueAsString();
	if (LogDirectory[LogDirectory.length() - 1] != '\\')	LogDirectory += "\\";			// append leading backward slash when not found

	if ((CreateDirectory((LogDirectory + "ConScribe Logs").c_str(), NULL) == 0 && GetLastError() != ERROR_ALREADY_EXISTS)||
		(CreateDirectory((LogDirectory + "ConScribe Logs\\Per-Script").c_str(), NULL) == 0 && GetLastError() != ERROR_ALREADY_EXISTS)||
		(CreateDirectory((LogDirectory + "ConScribe Logs\\Per-Mod").c_str(), NULL) == 0 && GetLastError() != ERROR_ALREADY_EXISTS)) {
			_MESSAGE("Error encountered while creating log directories");
			LogWinAPIErrorMessage(GetLastError());
			return false;
	}
	return true;
}

void PerformHouseKeeping(const char* DirectoryPath, const char* File, HouseKeepingOps Operation)
{
	for (IDirectoryIterator Itr(DirectoryPath, File); !Itr.Done(); Itr.Next()) {
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
	int NoOfBackups = GET_INI("LogBackups")->GetValueAsInteger(), ID = 0;
	if (NoOfBackups == -1)
		return;													// -1 implies that a time header is appended instead
	else if (NoOfBackups > MAX_BACKUPS)
		NoOfBackups = MAX_BACKUPS;


	char Buffer[0x08];
	std::string FullPath(FilePath + "\\" + FileName), FileNameExt(FileName.substr(0, FileName.find(".")));

	sprintf_s(Buffer, sizeof(Buffer), "%d", ID);
	std::fstream FileStream(std::string(FilePath + "\\" + FileNameExt + ".log" + std::string(Buffer)).c_str(), std::fstream::in);
	while (FileStream.fail() && ID <= NoOfBackups) {
		ID++;
		sprintf_s(Buffer, sizeof(Buffer), "%d", ID);
		FileStream.close(), FileStream.clear();
		FileStream.open(std::string(FilePath + "\\" + FileNameExt + ".log" + std::string(Buffer)).c_str(), std::fstream::in);
	}
	FileStream.close(), FileStream.clear();

	if (ID == NoOfBackups) {									// delete file
		if (DeleteFileA(FullPath.c_str()))				_MESSAGE("Deleted '%s'", FullPath.c_str());
		else {
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
	else {
		_MESSAGE("Couldn't rename '%s'", FullPath.c_str());
		LogWinAPIErrorMessage(GetLastError());
	}
}

void AppendHeader(std::string FilePath)
{
	ConScribeLog* SavedLog = new ConScribeLog(FilePath.c_str(), ConScribeLog::e_OutAp);
	SavedLog->AppendLoadHeader();

	delete SavedLog;
}