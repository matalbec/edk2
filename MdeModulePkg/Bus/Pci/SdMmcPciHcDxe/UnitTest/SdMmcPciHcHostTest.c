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

#define UNIT_TEST_NAME     "SdMmcPciHc driver unit tests"
#define UNIT_TEST_VERSION  "0.1"

typedef struct  _REGISTER_SPACE REGISTER_SPACE;

typedef
EFI_STATUS
(*REGISTER_SPACE_READ) (
  IN REGISTER_SPACE  *RegisterSpace,
  IN UINT64          Address,
  IN UINT32          Size,
  OUT UINT64         *Value
  );

typedef
EFI_STATUS
(*REGISTER_SPACE_WRITE) (
  IN REGISTER_SPACE  *RegisterSpace,
  IN UINT64          Address,
  IN UINT32          Size,
  IN UINT64          Value
  );

struct _REGISTER_SPACE {
  CHAR16                *Name;
  REGISTER_SPACE_READ   Read;
  REGISTER_SPACE_WRITE  Write;
};

typedef struct _REGISTER_MAP  REGISTER_MAP;
typedef struct _SIMPLE_REGISTER_SPACE SIMPLE_REGISTER_SPACE;

typedef
VOID
(*REGISTER_POST_READ_CALLBACK) (
  IN SIMPLE_REGISTER_SPACE  *RegisterSpace,
  IN UINT64        Address,
  IN UINT32        Size,
  IN OUT UINT64    *Value,
  IN VOID          *Context
  );

typedef
VOID
(*REGISTER_PRE_WRITE_CALLBACK) (
  IN SIMPLE_REGISTER_SPACE  *RegisterSpace,
  IN UINT64        Address,
  IN UINT32        Size,
  IN OUT UINT64    *Value,
  IN VOID          *Context
  );

struct _REGISTER_MAP {
  UINT64                       Offset;
  CHAR16*                      Name;
  UINT32                       SizeInBytes;
  UINT64                       Value;
};

GLOBAL_REMOVE_IF_UNREFERENCED  VOID  *MemoryBlock;

GLOBAL_REMOVE_IF_UNREFERENCED  UINT32  TestBlock[128] = {
  0xDEADBEEF,
  0xDEADBEEF,
  0xDEADBEEF,
  0xDEADBEEF,
  0xDEADBEEF,
  0xDEADBEEF,
  0xDEADBEEF
};

GLOBAL_REMOVE_IF_UNREFERENCED UINT32 PioIndex = 0;

EFI_STATUS
SdControllerMemRead (
  IN REGISTER_SPACE  *RegisterSpace,
  IN UINT64          Address,
  IN UINT32          Size,
  OUT UINT64         *Value
  );

EFI_STATUS
SdControllerMemWrite (
  IN REGISTER_SPACE  *RegisterSpace,
  IN UINT64          Address,
  IN UINT32          Size,
  IN UINT64          Value
  );

struct _SIMPLE_REGISTER_SPACE {
  REGISTER_SPACE  RegisterSpace;
  REGISTER_POST_READ_CALLBACK  PostRead;
  VOID                         *PostReadContext;
  REGISTER_PRE_WRITE_CALLBACK  PreWrite;
  VOID                         *PreWriteContext;
  UINTN           MapSize;
  REGISTER_MAP    *Map;
};

VOID
SdMmcPreWrite (
  IN SIMPLE_REGISTER_SPACE  *RegisterSpace,
  IN UINT64        Address,
  IN UINT32        Size,
  IN OUT UINT64    *Value,
  IN VOID          *Context
  );

VOID
SdMmcPostRead (
  IN SIMPLE_REGISTER_SPACE  *RegisterSpace,
  IN UINT64        Address,
  IN UINT32        Size,
  IN OUT UINT64    *Value,
  IN VOID          *Context
  )
{
  RegisterSpace->PreWrite = NULL;
  RegisterSpace->PostRead = NULL;
  if (Address == SD_MMC_HC_BUF_DAT_PORT) {
    if (PioIndex < ARRAY_SIZE (TestBlock)) {
      RegisterSpace->RegisterSpace.Write (
        &RegisterSpace->RegisterSpace,
        SD_MMC_HC_BUF_DAT_PORT,
        4,
        TestBlock[PioIndex]
      );
      PioIndex++;
    } else {
      RegisterSpace->RegisterSpace.Write (
        &RegisterSpace->RegisterSpace,
        SD_MMC_HC_NOR_INT_STS,
        4,
        BIT1
      );
    }
  }
  RegisterSpace->PostRead = SdMmcPostRead;
  RegisterSpace->PreWrite = SdMmcPreWrite;
}

VOID
SdMmcPreWrite (
  IN SIMPLE_REGISTER_SPACE  *RegisterSpace,
  IN UINT64        Address,
  IN UINT32        Size,
  IN OUT UINT64    *Value,
  IN VOID          *Context
  )
{
  UINT64  SdmaAddr;
  UINT64  TransferMode;
  UINT64  NormalInterrupt;

  RegisterSpace->PreWrite = NULL;
  RegisterSpace->PostRead = NULL;

  if (Address == SD_MMC_HC_COMMAND) {
    RegisterSpace->RegisterSpace.Read (
      &RegisterSpace->RegisterSpace,
      SD_MMC_HC_TRANS_MOD,
      2,
      &TransferMode
    );
    if (TransferMode & BIT0) {
      DEBUG ((DEBUG_INFO, "DMA transfer\n"));
      RegisterSpace->RegisterSpace.Read (
                                      &RegisterSpace->RegisterSpace,
                                      SD_MMC_HC_SDMA_ADDR,
                                      4,
                                      &SdmaAddr
                                      );
      DEBUG ((DEBUG_INFO, "SDMA address %X\n", SdmaAddr));
      if (SdmaAddr == 0x20) {
          DEBUG ((DEBUG_INFO, "Copying block\n"));
          DEBUG ((DEBUG_INFO, "Copying to %X from %X size %d\n", MemoryBlock, TestBlock, sizeof (TestBlock)));
          CopyMem (MemoryBlock, TestBlock, sizeof (TestBlock));
          RegisterSpace->RegisterSpace.Write (
            &RegisterSpace->RegisterSpace,
            SD_MMC_HC_NOR_INT_STS,
            4,
            0x3
            );
      }
    } else {
      DEBUG ((DEBUG_INFO, "PIO transfer\n"));
      PioIndex = 0;
      RegisterSpace->RegisterSpace.Write (
        &RegisterSpace->RegisterSpace,
        SD_MMC_HC_BUF_DAT_PORT,
        4,
        TestBlock[PioIndex]
      );
      PioIndex++;
      RegisterSpace->RegisterSpace.Write (
        &RegisterSpace->RegisterSpace,
        SD_MMC_HC_NOR_INT_STS,
        4,
        BIT5 | BIT0
      );
    }
  } else if (Address == SD_MMC_HC_NOR_INT_STS) {
    RegisterSpace->RegisterSpace.Read (
      &RegisterSpace->RegisterSpace,
      SD_MMC_HC_NOR_INT_STS,
      4,
      &NormalInterrupt
    );
    *Value = NormalInterrupt & ~(*Value);
    DEBUG ((DEBUG_INFO, "New Normal %X\n", *Value));
  } else if (Address == SD_MMC_HC_ERR_INT_STS) {
    RegisterSpace->RegisterSpace.Read (
      &RegisterSpace->RegisterSpace,
      SD_MMC_HC_ERR_INT_STS,
      4,
      &NormalInterrupt
    );
    *Value = NormalInterrupt & ~(*Value);
  }

  RegisterSpace->PostRead = SdMmcPostRead;
  RegisterSpace->PreWrite = SdMmcPreWrite;
}

GLOBAL_REMOVE_IF_UNREFERENCED REGISTER_MAP gSdMemMap[] = {
  {SD_MMC_HC_PRESENT_STATE, L"SD_MMC_HC_PRESENT_STATE", 0x4, 0x0},
  {SD_MMC_HC_NOR_INT_STS, L"SD_MMC_HC_NOR_INT_STS", 0x4, 0x0},
  {SD_MMC_HC_ERR_INT_STS, L"SD_MMC_HC_ERR_INT_STS", 0x4, 0x0},
  {SD_MMC_HC_HOST_CTRL1, L"SD_MMC_HC_HOST_CTRL1", 0x4, 0x0},
  {SD_MMC_HC_SDMA_ADDR, L"SD_MMC_HC_SDMA_ADDR", 0x4, 0x0},
  {SD_MMC_HC_BLK_SIZE, L"SD_MMC_HC_BLK_SIZE", 0x4, 0x0},
  {SD_MMC_HC_BLK_COUNT, L"SD_MMC_HC_BLK_COUNT", 0x4, 0x0},
  {SD_MMC_HC_ARG1, L"SD_MMC_HC_ARG1", 0x4, 0x0},
  {SD_MMC_HC_TRANS_MOD, L"SD_MMC_HC_TRANS_MOD", 0x4, 0x0},
  {SD_MMC_HC_COMMAND, L"SD_MMC_HC_COMMAND", 0x4, 0x0},
  {SD_MMC_HC_RESPONSE, L"SD_MMC_HC_RESPONSE0", 0x4, 0x0},
  {SD_MMC_HC_RESPONSE + 4, L"SD_MMC_HC_RESPONSE1", 0x4, 0x0},
  {SD_MMC_HC_RESPONSE + 8, L"SD_MMC_HC_RESPONSE2", 0x4, 0x0},
  {SD_MMC_HC_RESPONSE + 12, L"SD_MMC_HC_RESPONSE3", 0x4, 0x0},
  {SD_MMC_HC_BUF_DAT_PORT, L"SD_MMC_HC_BUF_DAT_PORT", 0x4, 0x0}
};

GLOBAL_REMOVE_IF_UNREFERENCED SIMPLE_REGISTER_SPACE gSdSimpleMem = {
  {L"SD controller MMIO space", SdControllerMemRead, SdControllerMemWrite},
  SdMmcPostRead,
  NULL,
  SdMmcPreWrite,
  NULL,
  ARRAY_SIZE (gSdMemMap),
  gSdMemMap
};

typedef struct {
  REGISTER_SPACE  *ConfigSpace;
  REGISTER_SPACE  *Bar[5]; // BARs 0-4
} MOCK_PCI_DEVICE;

EFI_STATUS
MockPciDeviceInitialize (
  IN REGISTER_SPACE       *ConfigSpace,
  OUT MOCK_PCI_DEVICE     **PciDev
  )
{
  *PciDev = AllocateZeroPool (sizeof (MOCK_PCI_DEVICE));

  (*PciDev)->ConfigSpace = ConfigSpace;

  return EFI_SUCCESS;
}

EFI_STATUS
MockPciDeviceRegisterBar (
  IN MOCK_PCI_DEVICE  *PciDev,
  IN REGISTER_SPACE   *BarRegisterSpace,
  IN UINT32           BarIndex
  )
{
  if (PciDev == NULL || BarIndex > 4) {
    return EFI_INVALID_PARAMETER;
  }
  PciDev->Bar[BarIndex] = BarRegisterSpace;
  return EFI_SUCCESS;
}

EFI_STATUS
MockPciDeviceDestroy (
  IN MOCK_PCI_DEVICE  *PciDev
  )
{
  if (PciDev == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  FreePool (PciDev);

  return EFI_SUCCESS;
}

EFI_STATUS
SdControllerPciRead (
  IN REGISTER_SPACE  *RegisterSpace,
  IN UINT64          Address,
  IN UINT32          Size,
  OUT UINT64         *Value
  )
{
  return EFI_SUCCESS;
}

EFI_STATUS
SdControllerPciWrite (
  IN REGISTER_SPACE  *RegisterSpace,
  IN UINT64          Address,
  IN UINT32          Size,
  IN UINT64          Value
  )
{
  return EFI_SUCCESS;
}

GLOBAL_REMOVE_IF_UNREFERENCED  REGISTER_SPACE  SdControllerPciSpace = {
  L"SD controller PCI config",
  SdControllerPciRead,
  SdControllerPciWrite
};

EFI_STATUS
SdControllerMemRead (
  IN REGISTER_SPACE  *RegisterSpace,
  IN UINT64          Address,
  IN UINT32          Size,
  OUT UINT64         *Value
  )
{
  UINT32  Index;
  SIMPLE_REGISTER_SPACE  *SimpleRegisterSpace;

  SimpleRegisterSpace = (SIMPLE_REGISTER_SPACE*) RegisterSpace;

  for (Index = 0; Index < SimpleRegisterSpace->MapSize; Index++) {
    if (SimpleRegisterSpace->Map[Index].Offset == Address) {
      DEBUG ((DEBUG_INFO, "Reading reg %s, Value = %X\n", SimpleRegisterSpace->Map[Index].Name, SimpleRegisterSpace->Map[Index].Value));
      switch (Size) {
        case 1:
          *(UINT8*)Value = (UINT8)SimpleRegisterSpace->Map[Index].Value;
          break;
        case 2:
          *(UINT16*)Value = (UINT16)SimpleRegisterSpace->Map[Index].Value;
          break;
        case 4:
          *(UINT32*)Value = (UINT32)SimpleRegisterSpace->Map[Index].Value;
          break;
        case 8:
        default:
          *Value = SimpleRegisterSpace->Map[Index].Value;
          break;
      }
      if (SimpleRegisterSpace->PostRead != NULL) {
        SimpleRegisterSpace->PostRead(SimpleRegisterSpace, Address, Size, Value, SimpleRegisterSpace->PostReadContext);
      }
      return EFI_SUCCESS;
    }
  }
  DEBUG ((DEBUG_WARN, "Unsupported register read at Address = %X\n", Address));
  return EFI_UNSUPPORTED;
}

EFI_STATUS
SdControllerMemWrite (
  IN REGISTER_SPACE  *RegisterSpace,
  IN UINT64          Address,
  IN UINT32          Size,
  IN UINT64          Value
  )
{
  UINT32  Index;
  SIMPLE_REGISTER_SPACE  *SimpleRegisterSpace;

  SimpleRegisterSpace = (SIMPLE_REGISTER_SPACE*) RegisterSpace;

  for (Index = 0; Index < SimpleRegisterSpace->MapSize; Index++) {
    if (SimpleRegisterSpace->Map[Index].Offset == Address) {
      if (SimpleRegisterSpace->PreWrite != NULL) {
        SimpleRegisterSpace->PreWrite(SimpleRegisterSpace, Address, Size, &Value, SimpleRegisterSpace->PreWriteContext);
      }
      DEBUG ((DEBUG_INFO, "Writing reg %s with %X\n", SimpleRegisterSpace->Map[Index].Name, Value));
      SimpleRegisterSpace->Map[Index].Value = Value;
      return EFI_SUCCESS;
    }
  }
  DEBUG ((DEBUG_WARN, "Unsupported register write at Address = %X\n", Address));
  return EFI_UNSUPPORTED;
}

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
  MOCK_PCI_IO  *PciIo;
  MOCK_PCI_DEVICE  *PciDev;
  UINT32  Size;
  UINT32  *Uint32Buffer;
  UINT32  Index;
  UINT64  Val;

  PciIo = (MOCK_PCI_IO*) This;
  PciDev = PciIo->MockPci;

  if (BarIndex > 4) {
    return EFI_UNSUPPORTED;
  }

  if (PciDev->Bar[BarIndex] == NULL) {
    DEBUG ((DEBUG_INFO, "NULL Bar\n"));
    return EFI_UNSUPPORTED;
  }

  if (Width == EfiPciIoWidthFifoUint32) {
    Uint32Buffer = (UINT32*) Buffer;
    for (Index = 0; Index < Count; Index++) {
      PciDev->Bar[BarIndex]->Read (PciDev->Bar[BarIndex], Offset, 4, &Val);
      Uint32Buffer[Index] = (UINT32) Val;
    }
    return EFI_SUCCESS;
  }

  switch (Width) {
    case EfiPciIoWidthUint8:
      Size = 1;
      break;
    case EfiPciIoWidthUint16:
      Size = 2;
      break;
    case EfiPciIoWidthUint32:
      Size = 4;
      break;
    case EfiPciIoWidthUint64:
      Size = 8;
      break;
    default:
      DEBUG ((DEBUG_INFO, "Unsupported width\n"));
      return EFI_UNSUPPORTED;
  }

  return PciDev->Bar[BarIndex]->Read (PciDev->Bar[BarIndex], Offset, Size, (UINT64*)Buffer);
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
  MOCK_PCI_IO  *PciIo;
  MOCK_PCI_DEVICE  *PciDev;
  UINT32  Size;

  PciIo = (MOCK_PCI_IO*) This;
  PciDev = PciIo->MockPci;

  if (BarIndex > 4) {
    return EFI_UNSUPPORTED;
  }

  if (PciDev->Bar[BarIndex] == NULL) {
    DEBUG ((DEBUG_INFO, "NULL Bar\n"));
    return EFI_UNSUPPORTED;
  }

  switch (Width) {
    case EfiPciIoWidthUint8:
      Size = 1;
      break;
    case EfiPciIoWidthUint16:
      Size = 2;
      break;
    case EfiPciIoWidthUint32:
      Size = 4;
      break;
    case EfiPciIoWidthUint64:
      Size = 8;
      break;
    default:
      return EFI_UNSUPPORTED;
  }

  return PciDev->Bar[BarIndex]->Write (PciDev->Bar[BarIndex], Offset, Size, *(UINT64*)Buffer);
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
  DEBUG ((DEBUG_INFO, "Calling to map\n"));
  *DeviceAddress = 0x20;
  *Mapping = HostAddress;
  return EFI_SUCCESS;
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
MockPciIoDestroy (
  IN EFI_PCI_IO_PROTOCOL  *PciIo
  )
{
  if (PciIo == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  FreePool (PciIo);
  return EFI_SUCCESS;
}

extern SD_MMC_HC_PRIVATE_DATA  gSdMmcPciHcTemplate;

EFI_STATUS
SdMmcPrivateDataBuildControllerReadyToTransfer (
  OUT SD_MMC_HC_PRIVATE_DATA  **Private
  )
{
  MOCK_PCI_DEVICE  *MockPciDevice;
  EFI_PCI_IO_PROTOCOL  *MockPciIo;

  *Private = AllocateCopyPool (sizeof (SD_MMC_HC_PRIVATE_DATA), &gSdMmcPciHcTemplate);

  InitializeListHead (&(*Private)->Queue);

  MockPciDeviceInitialize (&SdControllerPciSpace, &MockPciDevice);

  MockPciDeviceRegisterBar (MockPciDevice, (REGISTER_SPACE*) &gSdSimpleMem, 0);

  CreatePciIoForMockPciDevice (MockPciDevice, &MockPciIo);

  (*Private)->Slot[0].Enable = TRUE;
  (*Private)->Slot[0].MediaPresent = TRUE;
  (*Private)->Slot[0].Initialized = TRUE;
  (*Private)->Slot[0].CardType = SdCardType;
  (*Private)->Capability[0].Adma2 = FALSE;
  (*Private)->Capability[0].Sdma = TRUE;
  (*Private)->ControllerVersion[0] = SD_MMC_HC_CTRL_VER_300;
  (*Private)->PciIo = MockPciIo;

  return EFI_SUCCESS;
}

EFI_STATUS
SdMmcBuildControllerReadyForPioTransfer (
  OUT SD_MMC_HC_PRIVATE_DATA  **Private
  )
{
  MOCK_PCI_DEVICE  *MockPciDevice;
  EFI_PCI_IO_PROTOCOL  *MockPciIo;

  *Private = AllocateCopyPool (sizeof (SD_MMC_HC_PRIVATE_DATA), &gSdMmcPciHcTemplate);

  InitializeListHead (&(*Private)->Queue);

  MockPciDeviceInitialize (&SdControllerPciSpace, &MockPciDevice);

  MockPciDeviceRegisterBar (MockPciDevice, (REGISTER_SPACE*) &gSdSimpleMem, 0);

  CreatePciIoForMockPciDevice (MockPciDevice, &MockPciIo);

  (*Private)->Slot[0].Enable = TRUE;
  (*Private)->Slot[0].MediaPresent = TRUE;
  (*Private)->Slot[0].Initialized = TRUE;
  (*Private)->Slot[0].CardType = SdCardType;
  (*Private)->Capability[0].Adma2 = FALSE;
  (*Private)->Capability[0].Sdma = FALSE;
  (*Private)->ControllerVersion[0] = SD_MMC_HC_CTRL_VER_300;
  (*Private)->PciIo = MockPciIo;

  return EFI_SUCCESS;
}

VOID
SdMmcCreateSingleBlockTransferPacket (
  IN VOID*  BlockBuffer,
  IN UINT32  BlockSize,
  OUT EFI_SD_MMC_PASS_THRU_COMMAND_PACKET  *Packet,
  OUT EFI_SD_MMC_COMMAND_BLOCK             *CommandBlock,
  OUT EFI_SD_MMC_STATUS_BLOCK              *StatusBlock
  )
{
  ZeroMem (Packet, sizeof (EFI_SD_MMC_PASS_THRU_COMMAND_PACKET));
  ZeroMem (CommandBlock, sizeof (EFI_SD_MMC_COMMAND_BLOCK));
  ZeroMem (StatusBlock, sizeof (EFI_SD_MMC_STATUS_BLOCK));
  Packet->Timeout = 5;
  Packet->SdMmcCmdBlk = CommandBlock;
  Packet->SdMmcStatusBlk = StatusBlock;
  Packet->InDataBuffer = BlockBuffer;
  Packet->OutDataBuffer = NULL;
  Packet->InTransferLength = BlockSize;
  Packet->OutTransferLength = 0;
  Packet->TransactionStatus = EFI_SUCCESS;
  CommandBlock->CommandIndex = SD_READ_SINGLE_BLOCK;
  CommandBlock->CommandArgument = 0;
  CommandBlock->CommandType = SdMmcCommandTypeAdtc;
  CommandBlock->ResponseType = SdMmcResponseTypeR1;
}

UNIT_TEST_STATUS
EFIAPI
SdMmcSignleBlockReadShouldReturnDataBlockFromDevice (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  SD_MMC_HC_PRIVATE_DATA  *Private;
  EFI_SD_MMC_PASS_THRU_COMMAND_PACKET  Packet;
  EFI_SD_MMC_COMMAND_BLOCK  CommandBlock;
  EFI_SD_MMC_STATUS_BLOCK   StatusBlock;
  EFI_STATUS                Status;

  Private = (SD_MMC_HC_PRIVATE_DATA*) Context;

  MemoryBlock = AllocateZeroPool (512);
  SdMmcCreateSingleBlockTransferPacket (MemoryBlock, 512, &Packet, &CommandBlock, &StatusBlock);
  Status = Private->PassThru.PassThru (&Private->PassThru, 0, &Packet, NULL);

  UT_ASSERT_EQUAL (Status, EFI_SUCCESS);
  UT_ASSERT_MEM_EQUAL (MemoryBlock, TestBlock, sizeof (TestBlock));

  return UNIT_TEST_PASSED;
}

EFI_STATUS
SdMmcStall (
  IN UINTN  Microseconds
)
{
  DEBUG ((DEBUG_INFO, "Called Stall\n"));
  return EFI_SUCCESS;
}

EFI_TPL
SdMmcRaiseTpl (
  IN EFI_TPL      NewTpl
  )
{
  return NewTpl;
}

VOID
SdMmcRestoreTpl (
  IN EFI_TPL      OldTpl
  )
{
}

EFI_STATUS
EFIAPI
UefiTestMain (
  VOID
  )
{
  EFI_STATUS                  Status;
  UNIT_TEST_FRAMEWORK_HANDLE  Framework;
  UNIT_TEST_SUITE_HANDLE      SdMmcPassThruTest;
  SD_MMC_HC_PRIVATE_DATA  *PrivateForPio;
  SD_MMC_HC_PRIVATE_DATA  *PrivateForSdma;

  gBS = AllocateZeroPool (sizeof (EFI_BOOT_SERVICES));
  gBS->Stall = SdMmcStall;
  gBS->RaiseTPL = SdMmcRaiseTpl;
  gBS->RestoreTPL = SdMmcRestoreTpl;

  SdMmcBuildControllerReadyForPioTransfer (&PrivateForPio);
  SdMmcPrivateDataBuildControllerReadyToTransfer (&PrivateForSdma);

  Framework = NULL;

  DEBUG ((DEBUG_INFO, "%a v%a\n", UNIT_TEST_NAME, UNIT_TEST_VERSION));

  Status = InitUnitTestFramework (&Framework, UNIT_TEST_NAME, gEfiCallerBaseName, UNIT_TEST_VERSION);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Failed in InitUnitTestFramework. Status = %r\n", Status));
    return Status;
  }

  Status = CreateUnitTestSuite (&SdMmcPassThruTest, Framework, "SdMmcPassThruTests", "SdMmc.PassThru", NULL, NULL);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  //AddTestCase (SdMmcPassThruTest, "SingleBlockTestSdma", "SingleBlockTestSdma", SdMmcSignleBlockReadShouldReturnDataBlockFromDevice, NULL, NULL, PrivateForSdma);
  AddTestCase (SdMmcPassThruTest, "SingleBlockTestPio", "SingleBlockTestPio", SdMmcSignleBlockReadShouldReturnDataBlockFromDevice, NULL, NULL, PrivateForPio);

  Status = RunAllTestSuites (Framework);

  if (Framework) {
    FreeUnitTestFramework (Framework);
  }

  return Status;
}

int
main (
  int   argc,
  char  *argv[]
  )
{
  return UefiTestMain ();
}