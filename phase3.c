/*
 * Programmers: Omar Mendivil & Ayman Mohamed
 * Phase3
 * Creates user processes and semaphores, and handles system calls for
 * process control and synchronization
 */

#include "phase3.h"
#include "phase1.h"
#include "phase2.h"
#include "phase3_kernelInterfaces.h"
#include "usloss.h"
#include <stdbool.h>
#include <stdio.h>

typedef struct {
  int count;
  int mailBoxID;
  bool taken;
  int parentPid;
} semaphore;

typedef struct {
  int pid;
  bool taken;
  int (*userFunc)(void *arg);
  void *userArg;
} proc;

// protypes
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

// Runs user function in user mode and uses SYS_TERMINATE with the return value
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

// intializes semaphores, shadowProcTable, and system call handlers
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

// Creates a new semaphore with the given value
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

  int mailBoxID = MboxCreate(10, sizeof(char));
  if (mailBoxID < 0) {
    return -1;
  }

  semArr[slot].count = value;
  semArr[slot].mailBoxID = mailBoxID;
  semArr[slot].taken = true;
  semArr[slot].parentPid = getpid();

  *semaphore = slot;
  return 0;
}

// Decrements the semaphore count
// if count is negative it blocks by recieving a mailbox
int kernSemP(int semaphore) {
  if (semaphore < 0 || semaphore >= MAXSEMS || !semArr[semaphore].taken) {
    return -1;
  }

  semArr[semaphore].count--;

  if (semArr[semaphore].count < 0) {
    char flagByte = 'x';
    int result =
        MboxRecv(semArr[semaphore].mailBoxID, &flagByte, sizeof(flagByte));
    if (result < 0) {
      return -1;
    }
  }

  return 0;
}

// Increments the semaphore count
// A process is waiting if the count is 0 or less and unblocks the process
int kernSemV(int semaphore) {
  if (semaphore < 0 || semaphore >= MAXSEMS || !semArr[semaphore].taken) {
    return -1;
  }

  semArr[semaphore].count++;
  if (semArr[semaphore].count <= 0) {
    char flagByte = 'x';
    int result =
        MboxSend(semArr[semaphore].mailBoxID, &flagByte, sizeof(flagByte));
    if (result < 0) {
      return -1;
    }
  }

  return 0;
}

// Handles SYS_SPAWN extracts necessary data and assigns a slot to the
// shadowProcTable and sporks new user process
void spawnHandler(USLOSS_Sysargs *args) {
  int (*func)(void *) = (int (*)(void *))args->arg1;
  void *arg = args->arg2;
  int stackSize = (int)(long)args->arg3;
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
      spork(name, trampolineFunc, (void *)(long)slot, stackSize, priority);
  shadowProcTable[slot].pid = pid;
  args->arg1 = (void *)(long)pid;

  if (pid < 0) {
    args->arg5 = (void *)(long)-1;
  } else {
    args->arg5 = (void *)(long)0;
  }
}

// Handles SYS_WAIT waits for a child to finish and stores its pid and exit
// status
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

// Handles SYS_TERMINATE clears process entry from shadowProcTable and waits for
// children
void terminateHandler(USLOSS_Sysargs *args) {
  int status = (int)(long)args->arg1;
  int childPid, childStatus;

  int currPid = getpid();
  for (int i = 0; i < MAXPROC; i++) {
    if (shadowProcTable[i].taken && shadowProcTable[i].pid == currPid) {
      shadowProcTable[i].taken = false;
      shadowProcTable[i].userFunc = NULL;
      shadowProcTable[i].userArg = NULL;
    }
  }

  while ((childPid = join(&childStatus)) != -2) {
  }

  quit(status);
}

// Handles SYS_SEMCREATE extracts initial value, calls kernSemCreate, and
// returns semaphore ID
void semCreateHandler(USLOSS_Sysargs *args) {
  int val = (int)(long)args->arg1;
  int semaphore = 0;

  int result = kernSemCreate(val, &semaphore);
  args->arg1 = (void *)(long)semaphore;
  args->arg4 = (void *)(long)result;
}

// Handles SYS_SEMP performs P operation using kernSemP and stores result code
void semPHandler(USLOSS_Sysargs *args) {
  int semaphore = (int)(long)args->arg1;

  int retVal = kernSemP(semaphore);

  if (retVal < 0) {
    args->arg4 = (void *)(long)-1;
  } else {
    args->arg4 = (void *)(long)0;
  }
}

// Handles SYS_SEMV performs V operation using kernSemV and stores result code
void semVHandler(USLOSS_Sysargs *args) {
  int semaphore = (int)(long)args->arg1;

  int retVal = kernSemV(semaphore);

  if (retVal < 0) {
    args->arg4 = (void *)(long)-1;
  } else {
    args->arg4 = (void *)(long)0;
  }
}

// Handles SYS_GETTIMEOFDAY stores current system time in microseconds
void getTimeHandler(USLOSS_Sysargs *args) {
  int time = currentTime();
  args->arg1 = (void *)(long)time;
}

void getPidHandler(USLOSS_Sysargs *args) {
  int pid = getpid();
  args->arg1 = (void *)(long)pid;
}

void dumpProcHandler(USLOSS_Sysargs *args) { dumpProcesses(); }
