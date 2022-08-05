#ifndef _MOCK_PCI_IO_LIB_H_
#define _MOCK_PCI_IO_LIB_H_

#include <MockPciDevice.h>
#include <Protocol/PciIo.h>

typedef struct {
  EFI_PCI_IO_PROTOCOL  PciIo;
  MOCK_PCI_DEVICE      *MockPci;
} MOCK_PCI_IO;

EFI_STATUS
MockPciIoCreate (
  IN MOCK_PCI_DEVICE  *MockPci,
  OUT EFI_PCI_IO_PROTOCOL  **PciIo
  );

EFI_STATUS
MockPciIoDestroy (
  IN EFI_PCI_IO_PROTOCOL  *PciIo
  );

#endif