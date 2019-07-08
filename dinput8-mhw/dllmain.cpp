// dllmain.cpp : Defines the entry point for the DLL application.
#include <SDKDDKVer.h>
#include <windows.h>

#include <iostream>
#include <fstream>
#include <filesystem>
#include <vector>
#include <string>
#include <sstream>

#include "MinHook.h"
#include "log.h"

typedef HRESULT(WINAPI* tDirectInput8Create)(HINSTANCE inst_handle, DWORD version, const IID& r_iid, LPVOID* out_wrapper, LPUNKNOWN p_unk);
tDirectInput8Create oDirectInput8Create = nullptr;



class Quest {
public:
	static std::vector<Quest> Quests;
	static void PopulateQuests() 
	{
		for (auto& entry : std::filesystem::directory_iterator("nativePC/quest"))
		{
			std::string name = entry.path().filename().string();
			if (entry.path().filename().extension().string() != ".mib") continue;

			int id;
			if (sscanf_s(name.c_str(), "questData_%d.mib", &id) != 1) continue;
			if (id < 90000) continue;

			std::cout << "found quest: " << id << std::endl;
			Quests.push_back(Quest(id));
		}
	}


	int file_id;
	int starcount = 10;
	std::string questPath;

	Quest(int id) : file_id(id) 
	{
		char buf[250];
		sprintf_s(buf, "quest\\questData_%05d", id);
		questPath = buf;
	}
};

std::vector<Quest> Quest::Quests;

typedef bool(__fastcall* tCheckIfQuestIsUnlocked)(void* this_ptr, unsigned int id);

tCheckIfQuestIsUnlocked originalQuestUnlocked;
bool __fastcall QuestUnlocked(void* this_ptr, unsigned int id)
{
	LOG(INFO) << "QuestUnlocked";
	for (auto quest : Quest::Quests)
	{
		if (quest.file_id == id) {
			LOG(INFO) << "QuestUnlocked : " << id;
			return true;
		}
	}
	return originalQuestUnlocked(this_ptr, id);
}


typedef int(__fastcall* tGetQuestCount)();
tGetQuestCount originalQuestCount;

int __fastcall GetQuestCount()
{
	LOG(INFO) << "QuestCount";
	return originalQuestCount() + Quest::Quests.size();
}


typedef int(__fastcall* tGetQuestFromIndex)(void* this_ptr, int index);
tGetQuestFromIndex originalQuestFromIndex;

int __fastcall GetQuestFromIndex(void* this_ptr, int index)
{
	if (index >= originalQuestCount())
	{
		LOG(INFO) << "QuestFromIndex :" << index << ":" << Quest::Quests[index - originalQuestCount()].file_id;
		return Quest::Quests[index - originalQuestCount()].file_id;
	}
	return originalQuestFromIndex(this_ptr, index);
}

typedef bool(__fastcall* tCheckStarAndCategory)(int questID, int category, int starCount);
tCheckStarAndCategory originalCheckStarAndCategory;

bool __fastcall CheckStarAndCategory(int questID, int category, int starCount)
{
	LOG(INFO) << "CheckStarCategory " << questID;
	auto ret = originalCheckStarAndCategory(questID, category, starCount);
	for (auto quest : Quest::Quests)
	{
		if (questID != quest.file_id) continue;
		//std::cout << "CheckStarAndCategory" << questID << std::endl;
		if (category == 1 && starCount == quest.starcount) return true;
		return false;
	}
	return ret;
}

typedef long long(__fastcall* tGetQuestCategory)(int questID, int unkn);
tGetQuestCategory originalGetQuestCategory;

long long __fastcall GetQuestCategory(int questID, int unkn)
{
	LOG(INFO) << "QuestCategory " << questID;
	auto ret = originalGetQuestCategory(questID, unkn);
	if (questID > 90000) {
		return 1;
	}
	return ret;
	LOG(INFO) << "QuestCategory " << questID << "end";
}

typedef void* (__fastcall* tLoadFile)(void* this_ptr, void* loaderPtr, char* path, int flag);
tLoadFile originalLoadFile;
void* __fastcall LoadFilePath(void* this_ptr, void* loaderPtr, char* path, int flag)
{
	LOG(INFO) << "LoadFile :" << path;
	void* ret = originalLoadFile(this_ptr, loaderPtr, path, flag);

	if (loaderPtr == (void*)0x143bd8a38) 
	{
		for (auto quest : Quest::Quests) 
		{
			if (quest.questPath == path) {
				*(int*)((char*)ret + 0xa8 + 0x70) = quest.file_id;
				LOG(INFO) << "Overrode id to " << quest.file_id << " in path " << path;
			}
		}
	}
	return ret;
}

typedef void(__fastcall* tGetMonsterString)(char* buf, size_t size, unsigned int id);
tGetMonsterString originalGetString;

void __fastcall GetMonsterString(char *buf, size_t size, unsigned int id)
{
	LOG(INFO) << "MonsterString ";
	originalGetString(buf, size, id % 256);
	if (id >= 256) 
	{
		id = id >> 16;
		sprintf_s(buf + 6, size - 6, "%02d", id);
		LOG(INFO) << "mod variant called:" << id << " return value : " << buf;
	}
}

tGetMonsterString originalGetString2;
void __fastcall GetString2(char* buf, int monster_id, unsigned int id)
{
	originalGetString2(buf, monster_id % 256, id);
}


static int next_id = 0;
typedef void (__fastcall* tCreateMonster)(void* this_ptr, void *unkn, void *ptr, char flag);
tCreateMonster originalCreateMonster;
void __fastcall CreateMonster(void* this_ptr, void* unkn, void* ptr, char flag)
{
	auto monster_id_ptr = (int*)((char*)this_ptr + 0x158);
	LOG(INFO) << "Creating Monster : " <<
		(*monster_id_ptr & 0xFFFF) << ":" << (*monster_id_ptr >> 16) << " flags " << (int)flag;
	if (*monster_id_ptr >= 256)
	{
		next_id = *monster_id_ptr >> 16;
		*monster_id_ptr &= 0xFFFF;
	}
	return originalCreateMonster(this_ptr, unkn, ptr, flag);
}

typedef void* (__fastcall* tConstructMonster)(void* this_ptr, unsigned int monster_id, unsigned int variant);
tConstructMonster originalConstructMonster;
void* __fastcall ConstructMonster(void* this_ptr, unsigned int monster_id, unsigned int variant)
{
	LOG(INFO) << "Setting Subspecies :" << next_id;
	if (next_id) {
		variant = next_id;
		next_id = 0;
	}
	return originalConstructMonster(this_ptr, monster_id, variant);
}

typedef void(__fastcall* tSpawnLogic)(void* this_ptr, unsigned int id);
tSpawnLogic originalSpawnLogic;
void(__fastcall SpawnLogic)(void* this_ptr, unsigned int id)
{
	LOG(INFO) << "SpwanLogic  " << id;
	auto base_id = *(int*)((char*)this_ptr + 0x158);
	*(int*)((char*)this_ptr + 0x158) = base_id & 0xFFFF;
	originalSpawnLogic(this_ptr, id);
	*(int*)((char*)this_ptr + 0x158) = base_id;

}

void Initialize()
{
	char syspath[MAX_PATH];
	GetSystemDirectory(syspath, MAX_PATH);
	strcat_s(syspath, "\\dinput8.dll");
	HMODULE hMod = LoadLibrary(syspath);
	oDirectInput8Create = (tDirectInput8Create)GetProcAddress(hMod, "DirectInput8Create");

	Quest::PopulateQuests();

	std::cout << "Hooking\n";
	MH_Initialize();
	MH_CreateHook((void*)0x14862cda0, &QuestUnlocked, (LPVOID *) &originalQuestUnlocked);	
	MH_EnableHook((void*)0x14862cda0);

	MH_CreateHook((void*)0x14862d1e0, &GetQuestCount, (LPVOID*)& originalQuestCount);
	MH_EnableHook((void*)0x14862d1e0);

	MH_CreateHook((void*)0x14862d220, &GetQuestFromIndex, (LPVOID*)& originalQuestFromIndex);
	MH_EnableHook((void*)0x14862d220);
	
	MH_CreateHook((void*)0x14a8d87a0, &GetQuestCategory, (LPVOID*)& originalGetQuestCategory);
	MH_EnableHook((void*)0x14a8d87a0);
	
	MH_CreateHook((void*)0x147735140, &CheckStarAndCategory, (LPVOID*)& originalCheckStarAndCategory);
	MH_EnableHook((void*)0x147735140);

	MH_CreateHook((void*)0x14d10bfe0, &LoadFilePath, (LPVOID*)&originalLoadFile);
	MH_EnableHook((void*)0x14d10bfe0);

	MH_CreateHook((void*)0x149ed99b0, &GetMonsterString, (LPVOID*)&originalGetString);
	MH_EnableHook((void*)0x149ed99b0);


	MH_CreateHook((void*)0x14b210f90, &SpawnLogic, (LPVOID*)& originalSpawnLogic);
	MH_EnableHook((void*)0x14b210f90);

	MH_CreateHook((void*)0x14b210830, &CreateMonster, (LPVOID*)& originalCreateMonster);
	MH_EnableHook((void*)0x14b210830);

	MH_CreateHook((void*)0x1418d2bb0, &ConstructMonster, (LPVOID*)& originalConstructMonster);
	MH_EnableHook((void*)0x1418d2bb0);

	std::cout << "Hooking OK\n";

}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	if (ul_reason_for_call == DLL_PROCESS_ATTACH)
	{
		AllocConsole();
		FILE* fp;
		freopen_s(&fp, "CONOUT$", "w", stdout);
		DisableThreadLibraryCalls(hModule);
		Initialize();
	}
	return TRUE;
}

HRESULT WINAPI DirectInput8Create(HINSTANCE inst_handle, DWORD version, const IID& r_iid, LPVOID* out_wrapper, LPUNKNOWN p_unk)
{
	return oDirectInput8Create(inst_handle, version, r_iid, out_wrapper, p_unk);
}