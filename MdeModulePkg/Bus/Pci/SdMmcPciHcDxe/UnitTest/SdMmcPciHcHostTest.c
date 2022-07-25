/** @file
  UEFI OS based application for unit testing the SafeIntLib.

  Copyright (c) 2022, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <PiPei.h>
#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UnitTestLib.h>
#include <Protocol/PciIo.h>

#include "../SdMmcPciHcDxe.h"
#include "../SdMmcPciHci.h"
#include "FunctionsForTesting.h"

#define UNIT_TEST_NAME     "SdMmcPciHc driver unit tests"
#define UNIT_TEST_VERSION  "0.1"

typedef enum {
  Byte = 0,
  Word,
  DWord,
  QWord
} REGISTER_SIZE;

typedef struct {
  UINT64                     Offset;
  CHAR16                     *Name;
  REGISTER_SIZE              Size;
  UINT64                     Value;
} REGISTER_DESCRIPTOR;

typedef struct {
  BOOLEAN  Enabled;
  CHAR16   *Name;
  UINTN    NumOfRegs;
  REGISTER_DESCRIPTOR  *RegisterMap;
} REGISTER_SPACE;


typedef struct {
  REGISTER_SPACE  *ConfigSpace;
  REGISTER_SPACE  *Bar[5]; // BARs 0-4
} MOCK_PCI_DEVICE;

EFI_STATUS
InitializeMockPciDevice (
  IN REGISTER_SPACE  *ConfigSpace,
  IN REGISTER_SPACE  **Bars,
  IN UINT32               BarCount,
  OUT MOCK_PCI_DEVICE     **PciDev
  )
{
  UINT32  Index;

  *PciDev = AllocateZeroPool (sizeof (MOCK_PCI_DEVICE));

  (*PciDev)->ConfigSpace = ConfigSpace;
  for (Index = 0; Index < BarCount; Index++) {
    (*PciDev)->Bar[Index] = Bars[Index];
  }

  return EFI_SUCCESS;
}

EFI_STATUS
DestroyMockPciDevice (
  IN MOCK_PCI_DEVICE  *PciDev
  )
{
  if (PciDev == NULL) {
    return EFI_SUCCESS;
  }

  FreePool (PciDev);

  return EFI_SUCCESS;
}

REGISTER_DESCRIPTOR  SdControllerPciRegs[] = {
  {0x0, L"PCI_VENDOR_ID", Word, 0x8081}
};

REGISTER_DESCRIPTOR  SdControllerBarRegs[] = {
  {SD_MMC_HC_HOST_CTRL2, L"SD_MMC_HC_HOST_CTRL2", Byte, 0x0}
};

REGISTER_SPACE  SdControllerPciSpace = {
  TRUE,
  L"Sd controller PCI config",
  ARRAY_SIZE (SdControllerPciRegs),
  SdControllerPciRegs
};

REGISTER_SPACE  SdControllerMemSpace = {
  TRUE,
  L"SD controller MMIO space",
  ARRAY_SIZE (SdControllerBarRegs),
  SdControllerBarRegs
};

typedef struct {
  EFI_PCI_IO_PROTOCOL  PciIo;
  MOCK_PCI_DEVICE      *MockPci;
} MOCK_PCI_IO;

EFI_STATUS
MockPciIoPollMem (
  IN EFI_PCI_IO_PROTOCOL           *This,
  IN  EFI_PCI_IO_PROTOCOL_WIDTH    Width,
  IN  UINT8                        BarIndex,
  IN  UINT64                       Offset,
  IN  UINT64                       Mask,
  IN  UINT64                       Value,
  IN  UINT64                       Delay,
  OUT UINT64                       *Result
  )
{
  return EFI_UNSUPPORTED;
}

EFI_STATUS
MockPciIoPollIo (
  IN EFI_PCI_IO_PROTOCOL           *This,
  IN  EFI_PCI_IO_PROTOCOL_WIDTH    Width,
  IN  UINT8                        BarIndex,
  IN  UINT64                       Offset,
  IN  UINT64                       Mask,
  IN  UINT64                       Value,
  IN  UINT64                       Delay,
  OUT UINT64                       *Result
  )
{
  return EFI_UNSUPPORTED;
}

EFI_STATUS
MockPciIoReadMem (
  IN EFI_PCI_IO_PROTOCOL              *This,
  IN     EFI_PCI_IO_PROTOCOL_WIDTH    Width,
  IN     UINT8                        BarIndex,
  IN     UINT64                       Offset,
  IN     UINTN                        Count,
  IN OUT VOID                         *Buffer
  )
{
  return EFI_UNSUPPORTED;
}

EFI_STATUS
MockPciIoWriteMem (
  IN EFI_PCI_IO_PROTOCOL              *This,
  IN     EFI_PCI_IO_PROTOCOL_WIDTH    Width,
  IN     UINT8                        BarIndex,
  IN     UINT64                       Offset,
  IN     UINTN                        Count,
  IN OUT VOID                         *Buffer
  )
{
  return EFI_UNSUPPORTED;
}

EFI_STATUS
MockPciIoReadIo (
  IN EFI_PCI_IO_PROTOCOL              *This,
  IN     EFI_PCI_IO_PROTOCOL_WIDTH    Width,
  IN     UINT8                        BarIndex,
  IN     UINT64                       Offset,
  IN     UINTN                        Count,
  IN OUT VOID                         *Buffer
  )
{
  return EFI_UNSUPPORTED;
}

EFI_STATUS
MockPciIoWriteIo (
  IN EFI_PCI_IO_PROTOCOL              *This,
  IN     EFI_PCI_IO_PROTOCOL_WIDTH    Width,
  IN     UINT8                        BarIndex,
  IN     UINT64                       Offset,
  IN     UINTN                        Count,
  IN OUT VOID                         *Buffer
  )
{
  return EFI_UNSUPPORTED;
}

EFI_STATUS
MockPciIoConfigRead (
  IN EFI_PCI_IO_PROTOCOL              *This,
  IN     EFI_PCI_IO_PROTOCOL_WIDTH    Width,
  IN     UINT32                       Offset,
  IN     UINTN                        Count,
  IN OUT VOID                         *Buffer
  )
{
  return EFI_UNSUPPORTED;
}

EFI_STATUS
MockPciIoConfigWrite (
  IN EFI_PCI_IO_PROTOCOL              *This,
  IN     EFI_PCI_IO_PROTOCOL_WIDTH    Width,
  IN     UINT32                       Offset,
  IN     UINTN                        Count,
  IN OUT VOID                         *Buffer
  )
{
  return EFI_UNSUPPORTED;
}

EFI_STATUS
MockPciIoCopyMem (
  IN EFI_PCI_IO_PROTOCOL              *This,
  IN     EFI_PCI_IO_PROTOCOL_WIDTH    Width,
  IN     UINT8                        DestBarIndex,
  IN     UINT64                       DestOffset,
  IN     UINT8                        SrcBarIndex,
  IN     UINT64                       SrcOffset,
  IN     UINTN                        Count
  )
{
  return EFI_UNSUPPORTED;
}

EFI_STATUS
MockPciIoMap (
  IN EFI_PCI_IO_PROTOCOL                *This,
  IN     EFI_PCI_IO_PROTOCOL_OPERATION  Operation,
  IN     VOID                           *HostAddress,
  IN OUT UINTN                          *NumberOfBytes,
  OUT    EFI_PHYSICAL_ADDRESS           *DeviceAddress,
  OUT    VOID                           **Mapping
  )
{
  return EFI_UNSUPPORTED;
}

EFI_STATUS
MockPciIoUnmap (
  IN EFI_PCI_IO_PROTOCOL           *This,
  IN  VOID                         *Mapping
  )
{
  return EFI_UNSUPPORTED;
}

EFI_STATUS
MockPciIoAllocateBuffer (
  IN EFI_PCI_IO_PROTOCOL           *This,
  IN  EFI_ALLOCATE_TYPE            Type,
  IN  EFI_MEMORY_TYPE              MemoryType,
  IN  UINTN                        Pages,
  OUT VOID                         **HostAddress,
  IN  UINT64                       Attributes
  )
{
  return EFI_UNSUPPORTED;
}

EFI_STATUS
MockPciIoFreeBuffer (
  IN EFI_PCI_IO_PROTOCOL           *This,
  IN  UINTN                        Pages,
  IN  VOID                         *HostAddress
  )
{
  return EFI_UNSUPPORTED;
}

EFI_STATUS
MockPciIoFlush (
  IN EFI_PCI_IO_PROTOCOL  *This
  )
{
  return EFI_UNSUPPORTED;
}

EFI_STATUS
MockPciIoGetLocation (
  IN EFI_PCI_IO_PROTOCOL          *This,
  OUT UINTN                       *SegmentNumber,
  OUT UINTN                       *BusNumber,
  OUT UINTN                       *DeviceNumber,
  OUT UINTN                       *FunctionNumber
  )
{
  return EFI_UNSUPPORTED;
}

EFI_STATUS
MockPciIoProtocolAttributes (
  IN EFI_PCI_IO_PROTOCOL                       *This,
  IN  EFI_PCI_IO_PROTOCOL_ATTRIBUTE_OPERATION  Operation,
  IN  UINT64                                   Attributes,
  OUT UINT64                                   *Result OPTIONAL
  )
{
  return EFI_UNSUPPORTED;
}

EFI_STATUS
MockPciIoGetBarAttributes (
  IN EFI_PCI_IO_PROTOCOL             *This,
  IN  UINT8                          BarIndex,
  OUT UINT64                         *Supports  OPTIONAL,
  OUT VOID                           **Resources OPTIONAL
  )
{
  return EFI_UNSUPPORTED;
}

EFI_STATUS
MockPciIoSetBarAttributes (
  IN EFI_PCI_IO_PROTOCOL              *This,
  IN     UINT64                       Attributes,
  IN     UINT8                        BarIndex,
  IN OUT UINT64                       *Offset,
  IN OUT UINT64                       *Length
  )
{
  return EFI_UNSUPPORTED;
}

EFI_STATUS
CreatePciIoForMockPciDevice (
  IN MOCK_PCI_DEVICE  *MockPci,
  OUT EFI_PCI_IO_PROTOCOL  **PciIo
  )
{
  MOCK_PCI_IO  *MockPciIo;

  MockPciIo = AllocateZeroPool (sizeof (MOCK_PCI_IO));

  MockPciIo->MockPci = MockPci;

  MockPciIo->PciIo.PollMem = MockPciIoPollMem;
  MockPciIo->PciIo.PollIo = MockPciIoPollIo;
  MockPciIo->PciIo.Mem.Read = MockPciIoReadMem;
  MockPciIo->PciIo.Mem.Write = MockPciIoWriteMem;
  MockPciIo->PciIo.Io.Read = MockPciIoReadIo;
  MockPciIo->PciIo.Io.Write = MockPciIoWriteIo;
  MockPciIo->PciIo.CopyMem = MockPciIoCopyMem;
  MockPciIo->PciIo.Map = MockPciIoMap;
  MockPciIo->PciIo.Unmap = MockPciIoUnmap;
  MockPciIo->PciIo.AllocateBuffer = MockPciIoAllocateBuffer;
  MockPciIo->PciIo.FreeBuffer = MockPciIoFreeBuffer;
  MockPciIo->PciIo.Flush = MockPciIoFlush;
  MockPciIo->PciIo.GetLocation = MockPciIoGetLocation;
  MockPciIo->PciIo.Attributes = MockPciIoProtocolAttributes;
  MockPciIo->PciIo.GetBarAttributes = MockPciIoGetBarAttributes;
  MockPciIo->PciIo.SetBarAttributes = MockPciIoSetBarAttributes;
  MockPciIo->PciIo.RomSize = 0;
  MockPciIo->PciIo.RomImage = NULL;

  *PciIo = (EFI_PCI_IO_PROTOCOL*) MockPciIo;

  return EFI_SUCCESS;
}


EFI_STATUS
EFIAPI
UefiTestMain (
  VOID
  )
{
  MOCK_PCI_DEVICE  *MockPciDevice;
  REGISTER_SPACE   *Bars[3];
  EFI_PCI_IO_PROTOCOL  *MockPciIo;

  Bars[0] = &SdControllerMemSpace;
  InitializeMockPciDevice (&SdControllerPciSpace, Bars, 1, &MockPciDevice);

  CreatePciIoForMockPciDevice (MockPciDevice, &MockPciIo);

  DestroyMockPciDevice (MockPciDevice);

  return EFI_SUCCESS;
}

int
main (
  int   argc,
  char  *argv[]
  )
{
  return UefiTestMain ();
}