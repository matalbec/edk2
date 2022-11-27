#ifndef _MOCK_PCI_LIB_H_
#define _MOCK_PCI_LIB_H_

#include <Protocol/PciIo.h>
#include <RegisterSpaceMock.h>

typedef struct {
  REGISTER_SPACE_MOCK  *ConfigSpace;
  REGISTER_SPACE_MOCK  *Bar[5]; // BARs 0-4
} MOCK_PCI_DEVICE;

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

EFI_STATUS
MockPciDeviceInitialize (
  IN REGISTER_SPACE_MOCK       *ConfigSpace,
  OUT MOCK_PCI_DEVICE     **PciDev
  );

EFI_STATUS
MockPciDeviceRegisterBar (
  IN MOCK_PCI_DEVICE  *PciDev,
  IN REGISTER_SPACE_MOCK   *BarRegisterSpace,
  IN UINT32           BarIndex
  );

EFI_STATUS
MockPciDeviceDestroy (
  IN MOCK_PCI_DEVICE  *PciDev
  );

#endif