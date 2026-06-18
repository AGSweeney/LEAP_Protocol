/*******************************************************************************
 * Copyright (c) 2009, Rockwell Automation, Inc.
 * All rights reserved.
 ******************************************************************************/

#include <errno.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "opener_error.h"

const size_t kErrorMessageBufferSize = 255U;

int GetSocketErrorNumber(void) {
  return errno;
}

char *GetErrorMessage(int error_number) {
  char *error_message = (char *)malloc(kErrorMessageBufferSize);
  if (error_message == NULL) {
    return NULL;
  }

#if defined(_GNU_SOURCE) || defined(__GNU_SOURCE)
  strerror_r(error_number, error_message, kErrorMessageBufferSize);
#else
  strncpy(error_message, strerror(error_number), kErrorMessageBufferSize - 1U);
  error_message[kErrorMessageBufferSize - 1U] = '\0';
#endif
  return error_message;
}

void FreeErrorMessage(char *error_message) {
  free(error_message);
}
