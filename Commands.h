#pragma once

#include "CommandTable.h"
#include "ConScribeInternals.h"

extern CommandInfo kCommandInfo_Scribe;
extern CommandInfo kCommandInfo_RegisterLog;
extern CommandInfo kCommandInfo_UnregisterLog;
extern CommandInfo kCommandInfo_GetRegisteredLogNames;
extern CommandInfo kCommandInfo_ReadFromLog;


const char* ResolveModName(Script* ScriptObj);

OBSEArray* StringMapFromStdMap(const std::map<std::string, OBSEElement>& Data, Script* CallingScript);
OBSEArray* ArrayFromStdVector(const std::vector<OBSEElement>& Data, Script* CallingScript);