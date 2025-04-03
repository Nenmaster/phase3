#include "phase3.h"
#include "phase3_kernelInterfaces.h"
#include "phase3_usermode.h"
#include "usloss.h"
#include "utils.h"

#include <stdio.h>

// prototypes
void spawnHandler(USLOSS_Sysargs *args);
void waitHandler(USLOSS_Sysargs *args);
void terminateHandler(USLOSS_Sysargs *args);
void semCreateHandler(USLOSS_Sysargs *args);
void semPHandler(USLOSS_Sysargs *args);
void semVHandler(USLOSS_Sysargs *args);
void getTimeHandler(USLOSS_Sysargs *args);
void getPidHandler(USLOSS_Sysargs *args);
void dumpProcHandler(USLOSS_Sysargs *args);

semaphore semArr[MAXSEMS];
void (*systemCallVec[])(USLOSS_Sysargs *args);

void phase3_init(void) {
  for (int i = 0; i < MAXSEMS; i++) {
    // initiate semaphore array
  }

  systemCallVec[SYS_SPAWN] = spawnHandler;
  systemCallVec[SYS_WAIT] = waitHandler;
  systemCallVec[SYS_TERMINATE] = terminateHandler;
  systemCallVec[SYS_SEMCREATE] = semCreateHandler;
  systemCallVec[SYS_SEMP] = semPHandler;
  systemCallVec[SYS_SEMV] = semVHandler;
  systemCallVec[SYS_GETTIMEOFDAY] = getTimeHandler;
  systemCallVec[SYS_GETPID] = getPidHandler;
  systemCallVec[SYS_DUMPROCESSES] = dumProcHandler;
}

void phase3_start_service_processes(void) {}
int Spawn(char *name, int (*func)(void *), void *arg, int stack_size,
          int priority, int *pid);
int Wait(int *pid, int *status);
void Terminate(int status) __attribute__((__noreturn__));
void GetTimeofDay(int *tod);
void GetPID(int *pid);
int SemCreate(int value, int *semaphore);
int SemP(int semaphore);
int SemV(int semaphore);

// NOTE: No SemFree() call, it was removed

void DumpProcesses(void);

// Handlers

void spawnHandler(USLOSS_Sysargs *args) {}

void waitHandler(USLOSS_Sysargs *args) {}

void terminateHandler(USLOSS_Sysargs *args) {}

void semCreateHandler(USLOSS_Sysargs *args) {}

void semPHandler(USLOSS_Sysargs *args) {}

void semVHandler(USLOSS_Sysargs *args) {}

void getTimeHandler(USLOSS_Sysargs *args) {}

void getPidHandler(USLOSS_Sysargs *args) {}

void dumpProcHandler(USLOSS_Sysargs *args) {}
