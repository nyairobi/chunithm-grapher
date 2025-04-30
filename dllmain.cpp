#include "pch.h"
#include <cstdint>
#include <fstream>
#include <detours/detours.h>
#include <nlohmann/json.hpp>

int32_t g_currentScore = 0;
int32_t g_currentLife = 0;
std::atomic_bool g_listening = false;
std::time_t g_recentTick = 0;

static DWORD WINAPI Listen(LPVOID)
{
      if (g_listening) {
            return 0;
      }

      g_listening = true;

      std::vector<int32_t> scoreGraph;
      std::vector<int32_t> lifeGraph;
      bool running = true;
      while (running) {
            auto t = time(nullptr);
            if (t - g_recentTick > 1) {
                  std::ofstream ofs("score-" + std::to_string(t) + ".json");

                  for (int i = 0; i < 10; ++i) {
                        if (scoreGraph[0] == 1010000) {
                              scoreGraph.erase(scoreGraph.begin());
                              lifeGraph.erase(lifeGraph.begin());
                        }
                  }

                  nlohmann::json json;
                  json["scoreGraph"] = scoreGraph;
                  json["lifeGraph"] = lifeGraph;

                  ofs << json;
                  running = false;
            } else {
                  scoreGraph.push_back(1010000 - g_currentScore);
                  lifeGraph.push_back(g_currentLife);

                  Sleep(1000);
            }
      }

      g_listening = false;

      return 0;
}

int32_t(__fastcall* OrigGetLostScore)(void* arg) = (int32_t(__fastcall*)(void*))0x00bee430;
static int32_t __fastcall GetLostScore(void* arg)
{
      g_currentScore = (*OrigGetLostScore)(arg);
      g_recentTick = time(nullptr);

      if (g_currentScore == 0 && !g_listening) {
            CreateThread(nullptr, 0, Listen, nullptr, 0, nullptr);
      }

      return g_currentScore;
}

int32_t(__fastcall* OrigGetLifeRemaining)(void* arg) = (int32_t(__fastcall*)(void*))0x00bf77d0;
static int32_t __fastcall GetLifeRemaining(void* arg)
{
      g_currentLife = (*OrigGetLifeRemaining)(arg);
      return g_currentLife;
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
      }
      return TRUE;
}
