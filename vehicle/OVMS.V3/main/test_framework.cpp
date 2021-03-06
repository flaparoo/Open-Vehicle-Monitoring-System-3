/*
;    Project:       Open Vehicle Monitor System
;    Date:          14th March 2017
;
;    Changes:
;    1.0  Initial release
;
;    (C) 2011       Michael Stegen / Stegen Electronics
;    (C) 2011-2017  Mark Webb-Johnson
;    (C) 2011        Sonny Chen @ EPRO/DX
;
; Permission is hereby granted, free of charge, to any person obtaining a copy
; of this software and associated documentation files (the "Software"), to deal
; in the Software without restriction, including without limitation the rights
; to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
; copies of the Software, and to permit persons to whom the Software is
; furnished to do so, subject to the following conditions:
;
; The above copyright notice and this permission notice shall be included in
; all copies or substantial portions of the Software.
;
; THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
; IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
; FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
; AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
; LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
; OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
; THE SOFTWARE.
*/

#include "ovms_log.h"
static const char *TAG = "test";

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "esp_system.h"
#include "esp_event.h"
#include "esp_event_loop.h"
#include "esp_sleep.h"
#include "test_framework.h"
#include "ovms_command.h"
#include "ovms_peripherals.h"
#include "ovms_script.h"
#include "metrics_standard.h"
#include "can.h"
#include "strverscmp.h"

void test_deepsleep(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  int sleeptime = 60;
  if (argc==1)
    {
    sleeptime = atoi(argv[0]);
    }
  writer->puts("Entering deep sleep...");
  vTaskDelay(1000 / portTICK_PERIOD_MS);
  esp_deep_sleep(1000000LL * sleeptime);
  }

void test_javascript(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
#ifdef CONFIG_OVMS_SC_JAVASCRIPT_NONE
  writer->puts("No javascript engine enabled");
#endif //#ifdef CONFIG_OVMS_SC_JAVASCRIPT_NONE

#ifdef CONFIG_OVMS_SC_JAVASCRIPT_DUKTAPE
  duk_context *ctx = MyScripts.Duktape();
  duk_eval_string(ctx, "1+2");
  writer->printf("Javascript 1+2=%d\n", (int) duk_get_int(ctx, -1));
#endif //#ifdef CONFIG_OVMS_SC_JAVASCRIPT_DUKTAPE
  }

#ifdef CONFIG_OVMS_COMP_SDCARD
void test_sdcard(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  sdcard *sd = MyPeripherals->m_sdcard;

  if (!sd->isinserted())
    {
    writer->puts("Error: No SD CARD inserted");
    return;
    }
  if (!sd->ismounted())
    {
    writer->puts("Error: SD CARD not mounted");
    return;
    }

  unlink("/sd/ovmstest.txt");
  char buffer[512];
  memset(buffer,'A',sizeof(buffer));

  FILE *fd = fopen("/sd/ovmstest.txt","w");
  if (fd == NULL)
    {
    writer->puts("Error: /sd/ovmstest.txt could not be opened for writing");
    return;
    }

  writer->puts("SD CARD test starts...");
  for (int k=0;k<2048;k++)
    {
    fwrite(buffer, sizeof(buffer), 1, fd);
    if ((k % 128)==0)
      writer->printf("SD CARD written %d/%d\n",k,2048);
    }
  fclose(fd);

  writer->puts("Cleaning up");
  unlink("/sd/ovmstest.txt");

  writer->puts("SD CARD test completes");
  }
#endif // #ifdef CONFIG_OVMS_COMP_SDCARD

// Spew lines of the ASCII printable characters in the style of RFC 864.
void test_chargen(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  int numlines = 1000;
  int delay = 0;
  if (argc>=1)
    {
    numlines = atoi(argv[0]);
    }
  if (argc>=2)
    {
    delay = atoi(argv[1]);
    }
  char buf[74];
  buf[72] = '\n';
  buf[73] = '\0';
  char start = '!';
  for (int line = 0; line < numlines; ++line)
    {
    char ch = start;
    for (int col = 0; col < 72; ++col)
      {
      buf[col] = ch;
      if (++ch == 0x7F)
        ch = ' ';
      }
    if (writer->write(buf, 73) <= 0)
	break;
    if (delay)
      vTaskDelay(delay/portTICK_PERIOD_MS);
    if (++start == 0x7F)
        start = ' ';
    }
  }

bool test_echo_insert(OvmsWriter* writer, void* ctx, char ch)
  {
  if (ch == '\n')
    return false;
  writer->write(&ch, 1);
  return true;
  }

void test_echo(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  writer->puts("Type characters to be echoed, end with newline.");
  writer->RegisterInsertCallback(test_echo_insert, NULL);
  }

void test_watchdog(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  writer->puts("Spinning now (watchdog should fire in a few minutes)");
  for (;;) {}
  writer->puts("Error: We should never get here");
  }

void test_realloc(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  void* buf;
  void *interfere = NULL;

  writer->puts("First check heap integrity...");
  heap_caps_check_integrity_all(true);

  writer->puts("Now allocate 4KB RAM...");
  buf = ExternalRamMalloc(4096);

  writer->puts("Check heap integrity...");
  heap_caps_check_integrity_all(true);

  writer->puts("Now re-allocate bigger, 1,000 times...");
  for (int k=1; k<1001; k++)
    {
    buf = ExternalRamRealloc(buf, 4096+k);
    if (interfere == NULL)
      {
      interfere = ExternalRamMalloc(1024);
      }
    else
      {
      free(interfere);
      interfere = NULL;
      }
    }

  writer->puts("Check heap integrity...");
  heap_caps_check_integrity_all(true);

  writer->puts("Now re-allocate smaller, 1,000 times...");
  for (int k=1001; k>0; k--)
    {
    buf = ExternalRamRealloc(buf, 4096+k);
    if (interfere == NULL)
      {
      interfere = ExternalRamMalloc(1024);
      }
    else
      {
      free(interfere);
      interfere = NULL;
      }
    }

  writer->puts("Check heap integrity...");
  heap_caps_check_integrity_all(true);

  writer->puts("And free the buffer...");
  free(buf);
  if (interfere != NULL) free(interfere);

  writer->puts("Final check of heap integrity...");
  heap_caps_check_integrity_all(true);
  }

void test_spiram(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  writer->printf("Metrics (%p) are in %s RAM (%d bytes for a base metric)\n",
    StandardMetrics.ms_m_version,
    (((unsigned int)StandardMetrics.ms_m_version >= 0x3f800000)&&
     ((unsigned int)StandardMetrics.ms_m_version <= 0x3fbfffff))?
     "SPI":"INTERNAL",
     sizeof(OvmsMetric));
  }

void test_strverscmp(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  int c = strverscmp(argv[0],argv[1]);
  writer->printf("%s %s %s\n",
    argv[0],
    (c<0)?"<":((c==0)?"=":">"),
    argv[1]
    );
  }

void test_can(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  int64_t started = esp_timer_get_time();
  int64_t elapsed;
  bool tx = (strcmp(cmd->GetName(), "cantx")==0);

  int frames = 1000;
  if (argc>1) frames = atoi(argv[1]);

  canbus *can;
  if (argc>0)
    can = (canbus*)MyPcpApp.FindDeviceByName(argv[0]);
  else
    can = (canbus*)MyPcpApp.FindDeviceByName("can1");
  if (can == NULL)
    {
    writer->puts("Error: Cannot find specified can bus");
    return;
    }

  writer->printf("Testing %d frames on %s\n",frames,can->GetName());

  CAN_frame_t frame;
  memset(&frame,0,sizeof(frame));
  frame.origin = can;
  frame.FIR.U = 0;
  frame.FIR.B.DLC = 8;
  frame.FIR.B.FF = CAN_frame_std;

  for (int k=0;k<frames;k++)
    {
    frame.MsgID = (rand()%64)+256;
    if (tx)
      can->Write(&frame, pdMS_TO_TICKS(10));
    else
      MyCan.IncomingFrame(&frame);
    }

  elapsed = esp_timer_get_time() - started;
  int uspt = elapsed / frames;
  writer->printf("Transmitted %d frames in %lld.%06llds = %dus/frame\n",
    frames, elapsed / 1000000, elapsed % 1000000, uspt);
  }

class TestFrameworkInit
  {
  public: TestFrameworkInit();
} MyTestFrameworkInit  __attribute__ ((init_priority (5000)));

TestFrameworkInit::TestFrameworkInit()
  {
  ESP_LOGI(TAG, "Initialising TEST (5000)");

  OvmsCommand* cmd_test = MyCommandApp.RegisterCommand("test","Test framework",NULL,"",0,0,true);
  cmd_test->RegisterCommand("sleep","Test Deep Sleep",test_deepsleep,"[<seconds>]",0,1,true);
#ifdef CONFIG_OVMS_COMP_SDCARD
  cmd_test->RegisterCommand("sdcard","Test CD CARD",test_sdcard,"",0,0,true);
#endif // #ifdef CONFIG_OVMS_COMP_SDCARD
  cmd_test->RegisterCommand("javascript","Test Javascript",test_javascript,"",0,0,true);
  cmd_test->RegisterCommand("chargen","Character generator [<#lines>] [<delay_ms>]",test_chargen,"",0,2,true);
  cmd_test->RegisterCommand("echo", "Test getchar", test_echo, "", 0, 0,true);
  cmd_test->RegisterCommand("watchdog", "Test task spinning (and watchdog firing)", test_watchdog, "", 0, 0,true);
  cmd_test->RegisterCommand("realloc", "Test memory re-allocations", test_realloc, "", 0, 0,true);
  cmd_test->RegisterCommand("spiram", "Test SPI RAM memory usage", test_spiram, "", 0, 0,true);
  cmd_test->RegisterCommand("strverscmp", "Test strverscmp function", test_strverscmp, "", 2, 2, true);
  cmd_test->RegisterCommand("cantx", "Test CAN bus transmission", test_can, "[<port>] [<number>]", 0, 2, true);
  cmd_test->RegisterCommand("canrx", "Test CAN bus reception", test_can, "[<port>] [<number>]", 0, 2, true);
  }
