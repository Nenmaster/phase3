#ifndef UTILS_H
#define UTILS_H

#include <stdbool.h>

typedef struct {
  int count;
  int mailBoxID;
  bool taken;

} semaphore;

typedef struct {
  bool taken;
  int (*userFunc)(void *arg);
  void *userArg;
} proc;

#endif // !
