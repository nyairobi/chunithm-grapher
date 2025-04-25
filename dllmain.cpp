#include "pch.h"
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <sstream>
#include <TlHelp32.h>
#include <detours/detours.h>
#include <nlohmann/json.hpp>

int32_t g_currentScore = 0;
int32_t g_currentLife = 0;

int32_t(__fastcall* OrigGetLostScore)(void* arg) = (int32_t(__fastcall*)(void*))0x00bee430;
static int32_t __fastcall GetLostScore(void* arg)
{
      g_currentScore = (*OrigGetLostScore)(arg);
      return g_currentScore;
}

int32_t(__fastcall* OrigGetLifeRemaining)(void* arg) = (int32_t(__fastcall*)(void*))0x00bf77d0;
static int32_t __fastcall GetLifeRemaining(void* arg)
{
      g_currentLife = (*OrigGetLifeRemaining)(arg);
      return g_currentLife;
}

static uintptr_t GetModuleBaseAddress(DWORD64 procId, const wchar_t* modName)
{
      uintptr_t modBaseAddr = 0;
      HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, (DWORD)procId);
      if (hSnap != INVALID_HANDLE_VALUE) {
            MODULEENTRY32W modEntry;
            modEntry.dwSize = sizeof(modEntry);
            if (Module32FirstW(hSnap, &modEntry)) {
                  do {
                        if (!_wcsicmp(modEntry.szModule, modName)) {
                              modBaseAddr = (uintptr_t)modEntry.modBaseAddr;
                              break;
                        }
                  } while (Module32NextW(hSnap, &modEntry));
            }
      }
      CloseHandle(hSnap);
      return modBaseAddr;
}

// This music play detecting logic is taken from Chuniwide--it works pretty well
static DWORD WINAPI Listen(LPVOID)
{
      auto gameplayflag = false;
      auto procID = GetCurrentProcessId();
      auto addr1 = GetModuleBaseAddress(procID, L"chusanApp.exe") + 0x1EE6AF8;
      auto commaflag = false;

      std::vector<int32_t> scoreGraph;
      std::vector<int32_t> lifeGraph;
      while (1) {
            auto gameplayread = *((std::uint32_t*)addr1);

            if (gameplayread > 0 && gameplayflag == false) {
                  gameplayflag = true;

                  // Skip loading time, skill activation etc.
                  Sleep(12000);

                  scoreGraph.clear();
                  lifeGraph.clear();
            }

            if (gameplayread == 0 && gameplayflag == true) {
                  gameplayflag = false;

                  std::ofstream ofs("score-" + std::to_string(time(nullptr)) + ".json");

                  nlohmann::json json;
                  json["scoreGraph"] = scoreGraph;
                  json["lifeGraph"] = lifeGraph;

                  ofs << json;
            }

            if (gameplayflag == 0) {
                  Sleep(100);
            } else {
                  scoreGraph.push_back(1010000 - g_currentScore);
                  lifeGraph.push_back(g_currentLife);
                  Sleep(1000);
            }
      }
      return 0;
}

BOOL WINAPI DllMain(HINSTANCE hinst, DWORD dwReason, LPVOID reserved) 
{
      if (DetourIsHelperProcess()) {
            return TRUE;
      }

      if (dwReason == DLL_PROCESS_ATTACH) {
            DetourRestoreAfterWith();

            DetourTransactionBegin();
            DetourUpdateThread(GetCurrentThread());

            auto rv = DetourAttach(&(PVOID&)OrigGetLostScore, GetLostScore);
            if (rv != NO_ERROR) {
                  std::ofstream ofs("grapher.log");
                  ofs << "DetourAttach #1 failed: " << rv << std::endl;
                  return FALSE;
            }
            rv = DetourAttach(&(PVOID&)OrigGetLifeRemaining, GetLifeRemaining);
            if (rv != NO_ERROR) {
                  std::ofstream ofs("grapher.log");
                  ofs << "DetourAttach #2 failed: " << rv << std::endl;
                  return FALSE;
            }
            rv = DetourTransactionCommit();
            if (rv != NO_ERROR) {
                  std::ofstream ofs("grapher.log");
                  ofs << "DetourTransactionCommit failed: " << rv << std::endl;
                  return FALSE;
            }

            CreateThread(nullptr, 0, Listen, nullptr, 0, nullptr);
      }
      return TRUE;
}