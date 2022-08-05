#ifndef _MOCK_PCI_DEVICE_H_
#define _MOCK_PCI_DEVICE_H_

#include <RegisterSpaceMock.h>

typedef struct {
  REGISTER_SPACE_MOCK  *ConfigSpace;
  REGISTER_SPACE_MOCK  *Bar[5]; // BARs 0-4
} MOCK_PCI_DEVICE;

#endif