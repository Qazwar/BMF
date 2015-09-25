#include "osd.h"

#include "RTSSSharedMemory.h"

#include <shlwapi.h>
#include <float.h>
#include <io.h>
#include <tchar.h>

#include "config.h"
#include "io_monitor.h"

#define OSD_M_PRINTF if (config.mem_stats)    { pszOSD += sprintf (pszOSD,
#define OSD_B_PRINTF if (config.load_balance) { pszOSD += sprintf (pszOSD,
#define OSD_S_PRINTF if (config.mem_stats &&\
                         config.sli_stats)    { pszOSD += sprintf (pszOSD,
#define OSD_C_PRINTF if (config.cpu_stats)    { pszOSD += sprintf (pszOSD,
#define OSD_D_PRINTF if (config.disk_stats)   { pszOSD += sprintf (pszOSD,
#define OSD_P_PRINTF if (config.pagefile_stats)\
                                              { pszOSD += sprintf (pszOSD,
#define OSD_I_PRINTF if (config.io_stats)     { pszOSD += sprintf (pszOSD,
#define OSD_END    ); }

char  szOSD [4096];

#include "nvapi.h"
extern NV_GET_CURRENT_SLI_STATE sli_state;
extern BOOL nvapi_init;

memory_stats_t mem_stats [MAX_GPU_NODES];
mem_info_t     mem_info  [NumBuffers];

void
BMF_DrawOSD (void)
{
  char* pszOSD = szOSD;
  *pszOSD = '\0';

  static io_perf_t
    io_counter;

  buffer_t buffer = mem_info [0].buffer;

  int nodes = mem_info [buffer].nodes;

  if (nodes > 0) {
    int i = 0;

    int afr_idx  = sli_state.currentAFRIndex,
        afr_last = sli_state.previousFrameAFRIndex,
        afr_next = sli_state.nextFrameAFRIndex;

    OSD_M_PRINTF "\n"
                   "----- [DXGI 1.4]: Local Memory -----------"
                   "------------------------------------------"
                   "-\n"
    OSD_END

    while (i < nodes) {
      OSD_M_PRINTF "  %8s %u  (Reserve:  %05u / %05u MiB  - "
                   " Budget:  %05u / %05u MiB)",
                  nodes > 1 ? (nvapi_init ? "SLI Node" : "CFX Node") : "GPU",
                  i,
                  mem_info [buffer].local [i].CurrentReservation      >> 20ULL,
                  mem_info [buffer].local [i].AvailableForReservation >> 20ULL,
                  mem_info [buffer].local [i].CurrentUsage            >> 20ULL,
                  mem_info [buffer].local [i].Budget                  >> 20ULL
      OSD_END

      //
      // SLI Status Indicator
      //
      if (afr_last == i) {
        OSD_S_PRINTF "@"
        OSD_END
      }

      if (afr_idx == i) {
        OSD_S_PRINTF "!"
        OSD_END
      }
        
      if (afr_next == i) {
        OSD_S_PRINTF "#"
        OSD_END
      }
        
      OSD_M_PRINTF "\n"
      OSD_END

      i++;
    }

    i = 0;

    OSD_M_PRINTF "----- [DXGI 1.4]: Non-Local Memory -------"
                 "------------------------------------------"
                 "\n"
    OSD_END

    while (i < nodes) {
      OSD_M_PRINTF "  %8s %u  (Reserve:  %05u / %05u MiB  -  "
                   "Budget:  %05u / %05u MiB)\n",
                       nodes > 1 ? "SLI Node" : "GPU",
                       i,
              mem_info [buffer].nonlocal [i].CurrentReservation      >> 20ULL,
              mem_info [buffer].nonlocal [i].AvailableForReservation >> 20ULL,
              mem_info [buffer].nonlocal [i].CurrentUsage            >> 20ULL,
              mem_info [buffer].nonlocal [i].Budget                  >> 20ULL
      OSD_END

      i++;
    }

    OSD_M_PRINTF "----- [DXGI 1.4]: Miscellaneous ----------"
                 "------------------------------------------"
                 "---\n"
    OSD_END

    int64_t headroom = mem_info [buffer].local [0].Budget -
                       mem_info [buffer].local [0].CurrentUsage;

    OSD_M_PRINTF "  Max. Resident Set:  %05u MiB  -"
                 "  Max. Over Budget:  %05u MiB\n"
                 "    Budget Changes:  %06u       -    "
                 "       Budget Left:  %05i MiB\n",
                                    mem_stats [0].max_usage       >> 20ULL,
                                    mem_stats [0].max_over_budget >> 20ULL,
                                    mem_stats [0].budget_changes,
                                    headroom / 1024 / 1024
    OSD_END
  }
  
  extern int gpu_prio;

  OSD_B_PRINTF "\n  GPU Priority: %+1i\n",
    gpu_prio
  OSD_END

  BMF_CountIO (io_counter, config.io_interval / 1.0e-7);

  OSD_I_PRINTF "\n  Read..: %#6.02f MiB - (%#6.01f IOPs)"
               "\n  Write.: %#6.02f MiB - (%#6.01f IOPs)"
               "\n  Other.: %#6.02f MiB - (%#6.01f IOPs)\n",
               io_counter.read_mb_sec,  io_counter.read_iop_sec,
               io_counter.write_mb_sec, io_counter.write_iop_sec,
               io_counter.other_mb_sec, io_counter.other_iop_sec
  OSD_END

  for (int i = 0; i < disk_stats.num_disks; i++) {
    OSD_D_PRINTF "\n  Disk %16s %#3u%%  -  (Read: %#3u%%   Write: %#3u%%) - "
                                   "(Read: %#5.01f MiB   Write: %#5.01f MiB)",
      disk_stats.disks [i].name,
        disk_stats.disks [i].percent_load, 
          disk_stats.disks [i].percent_read,
            disk_stats.disks [i].percent_write,
              (float)disk_stats.disks [i].read_bytes_sec / (1024.0f * 1024.0f),
              (float)disk_stats.disks [i].write_bytes_sec / (1024.0f * 1024.0f)
    OSD_END

    if (i == 0) {
      OSD_D_PRINTF "\n"
      OSD_END
    }
  }

  OSD_D_PRINTF "\n"
  OSD_END

  OSD_C_PRINTF "\n  Total %#3u%%  -  (Kernel: %#3u%%   User: %#3u%%   "
                 "Interrupt: %#3u%%)\n",
        cpu_stats.cpus [0].percent_load, 
          cpu_stats.cpus [0].percent_kernel, 
            cpu_stats.cpus [0].percent_user, 
              cpu_stats.cpus [0].percent_interrupt
  OSD_END

  for (int i = 1; i < cpu_stats.num_cpus; i++) {
    OSD_C_PRINTF "\n  CPU%d: %#3u%%  -  (Kernel: %#3u%%   User: %#3u%%   "
                 "Interrupt: %#3u%%)",
      i-1,
        cpu_stats.cpus [i].percent_load, 
          cpu_stats.cpus [i].percent_kernel, 
            cpu_stats.cpus [i].percent_user, 
              cpu_stats.cpus [i].percent_interrupt
    OSD_END
  }

  OSD_C_PRINTF "\n"
  OSD_END

  for (int i = 0; i < pagefile_stats.num_pagefiles; i++) {
    OSD_P_PRINTF "\n  Pagefile %16s %#3u MiB / %#3u MiB  (Peak: %#3u MiB)",
      pagefile_stats.pagefiles [i].name,
        pagefile_stats.pagefiles [i].usage          >> 20ULL,
          pagefile_stats.pagefiles [i].size         >> 20ULL,
            pagefile_stats.pagefiles [i].usage_peak >> 20ULL
    OSD_END

    if (i == 0) {
      OSD_P_PRINTF "\n"
      OSD_END
    }
  }

  OSD_P_PRINTF "\n"
  OSD_END

  BMF_UpdateOSD (szOSD);
}

BOOL
BMF_UpdateOSD (LPCSTR lpText)
{
  BOOL bResult = FALSE;

  HANDLE hMapFile =
    OpenFileMappingA (FILE_MAP_ALL_ACCESS, FALSE, "RTSSSharedMemoryV2");

  if (hMapFile) {
    LPVOID               pMapAddr =
      MapViewOfFile (hMapFile, FILE_MAP_ALL_ACCESS, 0, 0, 0);

    LPRTSS_SHARED_MEMORY pMem     =
      (LPRTSS_SHARED_MEMORY)pMapAddr;

    if (pMem)
    {
      if ((pMem->dwSignature == 'RTSS') && 
          (pMem->dwVersion >= 0x00020000))
      {
        for (DWORD dwPass = 0; dwPass < 2; dwPass++)
        {
          //1st Pass: Find previously captured OSD slot
          //2nd Pass: Otherwise find the first unused OSD slot and capture it

          for (DWORD dwEntry = 1; dwEntry < pMem->dwOSDArrSize; dwEntry++)
          {
            // Allow primary OSD clients (i.e. EVGA Precision / MSI Afterburner)
            //   to use the first slot exclusively, so third party applications
            //     start scanning the slots from the second one

            RTSS_SHARED_MEMORY::LPRTSS_SHARED_MEMORY_OSD_ENTRY pEntry =
              (RTSS_SHARED_MEMORY::LPRTSS_SHARED_MEMORY_OSD_ENTRY)
                ((LPBYTE)pMem + pMem->dwOSDArrOffset +
                      dwEntry * pMem->dwOSDEntrySize);

            if (dwPass)
            {
              if (! strlen (pEntry->szOSDOwner))
                strcpy (pEntry->szOSDOwner, "Batman Fix");
            }

            if (! strcmp (pEntry->szOSDOwner, "Batman Fix"))
            {
              if (pMem->dwVersion >= 0x00020007)
                //Use extended text slot for v2.7 and higher shared memory,
                // it allows displaying 4096 symbols instead of 256 for regular
                //  text slot
                strncpy (pEntry->szOSDEx, lpText, sizeof pEntry->szOSDEx - 1);
              else
                strncpy (pEntry->szOSD,   lpText, sizeof pEntry->szOSD   - 1);

              pMem->dwOSDFrame++;

              bResult = TRUE;
              break;
            }
          }

          if (bResult)
            break;
        }
      }

      UnmapViewOfFile (pMapAddr);
    }

    CloseHandle (hMapFile);
  }

  return bResult;
}
/////////////////////////////////////////////////////////////////////////////

void
BMF_ReleaseOSD (void)
{
  HANDLE hMapFile =
    OpenFileMappingA (FILE_MAP_ALL_ACCESS, FALSE, "RTSSSharedMemoryV2");

  if (hMapFile)
  {
    LPVOID               pMapAddr =
      MapViewOfFile (hMapFile, FILE_MAP_ALL_ACCESS, 0, 0, 0);
    LPRTSS_SHARED_MEMORY pMem     =
      (LPRTSS_SHARED_MEMORY)pMapAddr;

    if (pMem)
    {
      if ((pMem->dwSignature == 'RTSS') && 
          (pMem->dwVersion   >= 0x00020000))
      {
        for (DWORD dwEntry = 1; dwEntry < pMem->dwOSDArrSize; dwEntry++)
        {
          RTSS_SHARED_MEMORY::LPRTSS_SHARED_MEMORY_OSD_ENTRY pEntry =
            (RTSS_SHARED_MEMORY::LPRTSS_SHARED_MEMORY_OSD_ENTRY)
            ((LPBYTE)pMem + pMem->dwOSDArrOffset +
                  dwEntry * pMem->dwOSDEntrySize);

          if (! strcmp (pEntry->szOSDOwner, "Batman Fix"))
          {
            memset (pEntry, 0, pMem->dwOSDEntrySize);
            pMem->dwOSDFrame++;
          }
        }
      }

      UnmapViewOfFile (pMapAddr);
    }

    CloseHandle (hMapFile);
  }
}