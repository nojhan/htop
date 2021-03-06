/*
htop - FreeBSDProcess.c
(C) 2015 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "Process.h"
#include "ProcessList.h"
#include "FreeBSDProcess.h"
#include "Platform.h"
#include "CRT.h"

#include <stdlib.h>
#include <unistd.h>
#include <sys/syscall.h>

/*{

typedef enum FreeBSDProcessFields {
   // Add platform-specific fields here, with ids >= 100
   LAST_PROCESSFIELD = 100,
} FreeBSDProcessField;

typedef struct FreeBSDProcess_ {
   Process super;
} FreeBSDProcess;

#ifndef Process_isKernelThread
#define Process_isKernelThread(_process) (_process->pgrp == 0)
#endif

#ifndef Process_isUserlandThread
#define Process_isUserlandThread(_process) (_process->pid != _process->tgid)
#endif

}*/

ProcessClass FreeBSDProcess_class = {
   .super = {
      .extends = Class(Process),
      .display = Process_display,
      .delete = Process_delete,
      .compare = FreeBSDProcess_compare
   },
   .writeField = (Process_WriteField) FreeBSDProcess_writeField,
};

ProcessFieldData Process_fields[] = {
   [0] = { .name = "", .title = NULL, .description = NULL, .flags = 0, },
   [PID] = { .name = "PID", .title = "    PID ", .description = "Process/thread ID", .flags = 0, },
   [COMM] = { .name = "Command", .title = "Command ", .description = "Command line", .flags = 0, },
   [STATE] = { .name = "STATE", .title = "S ", .description = "Process state (S sleeping, R running, D disk, Z zombie, T traced, W paging)", .flags = 0, },
   [PPID] = { .name = "PPID", .title = "   PPID ", .description = "Parent process ID", .flags = 0, },
   [PGRP] = { .name = "PGRP", .title = "   PGRP ", .description = "Process group ID", .flags = 0, },
   [SESSION] = { .name = "SESSION", .title = "   SESN ", .description = "Process's session ID", .flags = 0, },
   [TTY_NR] = { .name = "TTY_NR", .title = "  TTY ", .description = "Controlling terminal", .flags = 0, },
   [TPGID] = { .name = "TPGID", .title = "  TPGID ", .description = "Process ID of the fg process group of the controlling terminal", .flags = 0, },
   [MINFLT] = { .name = "MINFLT", .title = "     MINFLT ", .description = "Number of minor faults which have not required loading a memory page from disk", .flags = 0, },
   [MAJFLT] = { .name = "MAJFLT", .title = "     MAJFLT ", .description = "Number of major faults which have required loading a memory page from disk", .flags = 0, },
   [PRIORITY] = { .name = "PRIORITY", .title = "PRI ", .description = "Kernel's internal priority for the process", .flags = 0, },
   [NICE] = { .name = "NICE", .title = " NI ", .description = "Nice value (the higher the value, the more it lets other processes take priority)", .flags = 0, },
   [STARTTIME] = { .name = "STARTTIME", .title = "START ", .description = "Time the process was started", .flags = 0, },

   [PROCESSOR] = { .name = "PROCESSOR", .title = "CPU ", .description = "Id of the CPU the process last executed on", .flags = 0, },
   [M_SIZE] = { .name = "M_SIZE", .title = " VIRT ", .description = "Total program size in virtual memory", .flags = 0, },
   [M_RESIDENT] = { .name = "M_RESIDENT", .title = "  RES ", .description = "Resident set size, size of the text and data sections, plus stack usage", .flags = 0, },
   [ST_UID] = { .name = "ST_UID", .title = " UID ", .description = "User ID of the process owner", .flags = 0, },
   [PERCENT_CPU] = { .name = "PERCENT_CPU", .title = "CPU% ", .description = "Percentage of the CPU time the process used in the last sampling", .flags = 0, },
   [PERCENT_MEM] = { .name = "PERCENT_MEM", .title = "MEM% ", .description = "Percentage of the memory the process is using, based on resident memory size", .flags = 0, },
   [USER] = { .name = "USER", .title = "USER      ", .description = "Username of the process owner (or user ID if name cannot be determined)", .flags = 0, },
   [TIME] = { .name = "TIME", .title = "  TIME+  ", .description = "Total time the process has spent in user and system time", .flags = 0, },
   [NLWP] = { .name = "NLWP", .title = "NLWP ", .description = "Number of threads in the process", .flags = 0, },
   [TGID] = { .name = "TGID", .title = "   TGID ", .description = "Thread group ID (i.e. process ID)", .flags = 0, },
   [LAST_PROCESSFIELD] = { .name = "*** report bug! ***", .title = NULL, .description = NULL, .flags = 0, },
};

char* Process_pidFormat = "%7u ";
char* Process_tpgidFormat = "%7u ";

void Process_setupColumnWidths() {
   int maxPid = Platform_getMaxPid();
   if (maxPid == -1) return;
   if (maxPid > 99999) {
      Process_fields[PID].title =     "    PID ";
      Process_fields[PPID].title =    "   PPID ";
      Process_fields[TPGID].title =   "  TPGID ";
      Process_fields[TGID].title =    "   TGID ";
      Process_fields[PGRP].title =    "   PGRP ";
      Process_fields[SESSION].title = "   SESN ";
      Process_pidFormat = "%7u ";
      Process_tpgidFormat = "%7d ";
   } else {
      Process_fields[PID].title =     "  PID ";
      Process_fields[PPID].title =    " PPID ";
      Process_fields[TPGID].title =   "TPGID ";
      Process_fields[TGID].title =    " TGID ";
      Process_fields[PGRP].title =    " PGRP ";
      Process_fields[SESSION].title = " SESN ";
      Process_pidFormat = "%5u ";
      Process_tpgidFormat = "%5d ";
   }
}

FreeBSDProcess* FreeBSDProcess_new(Settings* settings) {
   FreeBSDProcess* this = calloc(sizeof(FreeBSDProcess), 1);
   Object_setClass(this, Class(FreeBSDProcess));
   Process_init(&this->super, settings);
   return this;
}

void Process_delete(Object* cast) {
   FreeBSDProcess* this = (FreeBSDProcess*) cast;
   Process_done((Process*)cast);
   free(this);
}

void FreeBSDProcess_writeField(Process* this, RichString* str, ProcessField field) {
   //FreeBSDProcess* fp = (FreeBSDProcess*) this;
   char buffer[256]; buffer[255] = '\0';
   int attr = CRT_colors[DEFAULT_COLOR];
   //int n = sizeof(buffer) - 1;
   switch (field) {
   // add FreeBSD-specific fields here
   default:
      Process_writeField(this, str, field);
      return;
   }
   RichString_append(str, attr, buffer);
}

long FreeBSDProcess_compare(const void* v1, const void* v2) {
   FreeBSDProcess *p1, *p2;
   Settings *settings = ((Process*)v1)->settings;
   if (settings->direction == 1) {
      p1 = (FreeBSDProcess*)v1;
      p2 = (FreeBSDProcess*)v2;
   } else {
      p2 = (FreeBSDProcess*)v1;
      p1 = (FreeBSDProcess*)v2;
   }
   switch (settings->sortKey) {
   // add FreeBSD-specific fields here
   default:
      return Process_compare(v1, v2);
   }
}

bool Process_isThread(Process* this) {
   return (Process_isKernelThread(this));
}
