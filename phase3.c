#include "phase3.h"
#include "phase1.h"
#include "phase2.h"
#include "phase3_kernelInterfaces.h"
#include "phase3_usermode.h"
#include "usloss.h"

#include <stdbool.h>
#include <stdio.h>

typedef struct {
  int count;
  int mailBoxID;
  bool taken;
} semaphore;

typedef struct {
  int pid;
  bool taken;
  int (*userFunc)(void *arg);
  void *userArg;
} proc;

// protyped
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
proc shadowProcTable[MAXPROC];

void phase3_start_service_processes() {};

int trampolineFunc(void *arg) {
  int slot = (int)(long)arg;

  unsigned int psr = USLOSS_PsrGet();
  psr = psr & ~USLOSS_PSR_CURRENT_MODE;
  int result = USLOSS_PsrSet(psr);

  if (result != USLOSS_DEV_OK) {
    USLOSS_Halt(1);
  }

  int (*func)(void *) = shadowProcTable[slot].userFunc;
  void *userArg = shadowProcTable[slot].userArg;

  int retval = func(userArg);

  USLOSS_Sysargs sysArgs;
  sysArgs.number = SYS_TERMINATE;
  sysArgs.arg1 = (void *)(long)retval;
  USLOSS_Syscall(&sysArgs);

  return 0;
}

void phase3_init(void) {
  for (int i = 0; i < MAXSEMS; i++) {
    semArr[i].taken = false;
    semArr[i].count = 0;
    semArr[i].mailBoxID = -1;
  }

  for (int i = 0; i < MAXPROC; i++) {
    shadowProcTable[i].pid = -1;
    shadowProcTable[i].taken = false;
    shadowProcTable[i].userFunc = NULL;
    shadowProcTable[i].userArg = NULL;
  }

  systemCallVec[SYS_SPAWN] = spawnHandler;
  systemCallVec[SYS_WAIT] = waitHandler;
  systemCallVec[SYS_TERMINATE] = terminateHandler;
  systemCallVec[SYS_SEMCREATE] = semCreateHandler;
  systemCallVec[SYS_SEMP] = semPHandler;
  systemCallVec[SYS_SEMV] = semVHandler;
  systemCallVec[SYS_GETTIMEOFDAY] = getTimeHandler;
  systemCallVec[SYS_GETPID] = getPidHandler;
  systemCallVec[SYS_DUMPPROCESSES] = dumpProcHandler;
}

int kernSemCreate(int value, int *semaphore) {
  if (value < 0) {
    return -1;
  }

  int slot = -1;
  for (int i = 0; i < MAXSEMS; i++) {
    if (!semArr[i].taken) {
      semArr[i].taken = true;
      slot = i;
      break;
    }
  }

  if (slot < 0) {
    return -1;
  }

  int mailBoxID = MboxCreate(1, 0);
  if (mailBoxID < 0) {
    return -1;
  }

  semArr[slot].count = value;
  semArr[slot].mailBoxID = mailBoxID;
  semArr[slot].taken = true;

  *semaphore = slot;
  return 0;
}

int kernSemP(int semaphore) {
  if (semaphore < 0 || semaphore >= MAXSEMS || !semArr[semaphore].taken) {
    return -1;
  }

  semArr[semaphore].count--;

  if (semArr[semaphore].count < 0) {
    char placeHolder = 'x';
    int result = MboxRecv(semArr[semaphore].mailBoxID, &placeHolder,
                          sizeof(placeHolder));
    if (result < 0) {
      return -1;
    }
  }

  return 0;
}

int kernSemV(int semaphore) {
  if (semaphore < 0 || semaphore >= MAXSEMS || !semArr[semaphore].taken) {
    return -1;
  }

  semArr[semaphore].count++;

  if (semArr[semaphore].count <= 0) {
    char placeHolder = 'x';
    int result = MboxSend(semArr[semaphore].mailBoxID, &placeHolder,
                          sizeof(placeHolder));
    if (result < 0) {
      return -1;
    }
  }

  return 0;
}

// Handlers

void spawnHandler(USLOSS_Sysargs *args) {
  int (*func)(void *) = (int (*)(void *))args->arg1;
  void *arg = args->arg2;
  int stack_size = (int)(long)args->arg3;
  int priority = (int)(long)args->arg4;
  char *name = (char *)args->arg5;

  if (name == NULL || func == NULL || priority < 0 || priority > 5) {
    args->arg1 = (void *)-1;
    args->arg4 = (void *)-1;
    return;
  }

  int slot = -1;
  for (int i = 0; i < MAXPROC; i++) {
    if (!shadowProcTable[i].taken) {
      slot = i;
      break;
    }
  }

  if (slot < 0) {
    args->arg1 = (void *)-1;
    args->arg4 = (void *)-1;
    return;
  }

  shadowProcTable[slot].taken = true;
  shadowProcTable[slot].userFunc = func;
  shadowProcTable[slot].userArg = arg;

  int pid =
      spork(name, trampolineFunc, (void *)(long)slot, stack_size, priority);

  args->arg1 = (void *)(long)pid;

  if (pid < 0) {
    args->arg5 = (void *)(long)-1;
  } else {
    args->arg5 = (void *)(long)0;
  }
}

void waitHandler(USLOSS_Sysargs *args) {
  int pid, status;
  pid = join(&status);

  args->arg1 = (void *)(long)pid;
  args->arg2 = (void *)(long)status;

  if (pid != -2) {
    args->arg4 = (void *)(long)0;
  } else {
    args->arg4 = (void *)(long)-2;
  }
}

void terminateHandler(USLOSS_Sysargs *args) {
  int status = (int)(long)args->arg1;
  int pid, childStatus;

  for (int i = 0; i < MAXSEMS; i++) {
    if (semArr[i].taken) {
      MboxRelease(semArr[i].mailBoxID);

      semArr[i].taken = false;
      semArr[i].count = 0;
      semArr[i].mailBoxID = -1;
    }
  }

  int currPid = getpid();
  for (int i = 0; i < MAXPROC; i++) {
    if (shadowProcTable[i].taken && shadowProcTable[i].pid == currPid) {
      shadowProcTable[i].taken = false;
      shadowProcTable[i].userFunc = NULL;
      shadowProcTable[i].userArg = NULL;
    }
  }

  while ((pid = join(&childStatus)) != -2) {
  }

  quit(status);
}

void semCreateHandler(USLOSS_Sysargs *args) {
  int val = (int)(long)args->arg1;
  int semaphore = 0;

  int result = kernSemCreate(val, &semaphore);
  args->arg1 = (void *)(long)semaphore;
  args->arg4 = (void *)(long)result;
}

void semPHandler(USLOSS_Sysargs *args) {
  int semaphore = (int)(long)args->arg1;

  int retVal = kernSemP(semaphore);

  if (retVal < 0) {
    args->arg4 = (void *)(long)-1;
  } else {
    args->arg4 = (void *)(long)0;
  }
}

void semVHandler(USLOSS_Sysargs *args) {
  int semaphore = (int)(long)args->arg1;

  int retVal = kernSemV(semaphore);

  if (retVal < 0) {
    args->arg4 = (void *)(long)-1;
  } else {
    args->arg4 = (void *)(long)0;
  }
}

void getTimeHandler(USLOSS_Sysargs *args) {
  int time = currentTime();
  args->arg1 = (void *)(long)time;
}

void getPidHandler(USLOSS_Sysargs *args) {
  int pid = getpid();
  args->arg1 = (void *)(long)pid;
}

void dumpProcHandler(USLOSS_Sysargs *args) { dumpProcesses(); }
