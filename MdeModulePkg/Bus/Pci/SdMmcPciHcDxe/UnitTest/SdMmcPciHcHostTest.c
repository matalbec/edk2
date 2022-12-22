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
#include <MockPciLib.h>
#include <RegisterSpaceMock.h>
#include <MapBasedRegisterSpaceLib.h>

#include "../SdMmcPciHcDxe.h"
#include "../SdMmcPciHci.h"

#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <assert.h>
#include <stdarg.h>
#include <glib.h>
#include <qemu/compiler.h>
#include <libqos/libqos-pc.h>
#include <libqos/pci-pc.h>

#define UNIT_TEST_NAME     "SdMmcPciHc driver unit tests"
#define UNIT_TEST_VERSION  "0.1"

#define SD_CONTROLLER_MODEL_NUM_OF_BLOCKS  5
#define SD_CONTROLLER_MODE_BLOCK_SIZE  512

typedef struct {
  BOOLEAN  PioTrasnferStart;
  UINTN    CurrentPioIndex;
  UINT8    Block[SD_CONTROLLER_MODEL_NUM_OF_BLOCKS][SD_CONTROLLER_MODE_BLOCK_SIZE];
  BOOLEAN  LedWasEnabled;
} SD_CONTROLLER_CONTEXT;


typedef struct {
  SD_MMC_HC_PRIVATE_DATA  *Private;
  SD_CONTROLLER_CONTEXT   *ControllerContext;
} TEST_CONTEXT;

GLOBAL_REMOVE_IF_UNREFERENCED  VOID  *MemoryBlock;

GLOBAL_REMOVE_IF_UNREFERENCED  UINT8  gTestBlock[SD_CONTROLLER_MODE_BLOCK_SIZE] = {
  0xEF, 0xBE, 0xAD, 0xDE,
  0xEF, 0xBE, 0xAD, 0xDE,
  0xEF, 0xBE, 0xAD, 0xDE,
  0xEF, 0xBE, 0xAD, 0xDE,
  0xEF, 0xBE, 0xAD, 0xDE,
  0xEF, 0xBE, 0xAD, 0xDE,
  0xEF, 0xBE, 0xAD, 0xDE,
  0xEF, 0xBE, 0xAD, 0xDE,
};

VOID
SdMmcPostRead (
  IN MAP_BASED_REGISTER_SPACE  *RegisterSpace,
  IN UINT64        Address,
  IN UINT32        Size,
  IN OUT UINT64    *Value,
  IN VOID          *Context
  )
{
  SD_CONTROLLER_CONTEXT  *SdController;
  UINT32 *Block32;

  SdController = (SD_CONTROLLER_CONTEXT*) Context;

  if (Address == SD_MMC_HC_BUF_DAT_PORT) {
    if (SdController->PioTrasnferStart && (SdController->CurrentPioIndex < SD_CONTROLLER_MODE_BLOCK_SIZE / sizeof (UINT32))) {
      Block32 = (UINT32*) SdController->Block[0];
      *Value = Block32[SdController->CurrentPioIndex];
      SdController->CurrentPioIndex++;
      if (SdController->CurrentPioIndex >= SD_CONTROLLER_MODE_BLOCK_SIZE / sizeof (UINT32)) {
        RegisterSpace->RegisterSpace.Write (
          &RegisterSpace->RegisterSpace,
          SD_MMC_HC_NOR_INT_STS,
          4,
          BIT1
        );
      }
    }
  }
}

VOID
SdMmcPreWrite (
  IN MAP_BASED_REGISTER_SPACE  *RegisterSpace,
  IN UINT64        Address,
  IN UINT32        Size,
  IN OUT UINT64    *Value,
  IN VOID          *Context
  )
{
  UINT64  SdmaAddr;
  UINT64  TransferMode;
  UINT64  NormalInterrupt;
  SD_CONTROLLER_CONTEXT  *SdController;

  SdController = (SD_CONTROLLER_CONTEXT*) Context;

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
      if ((UINT32)SdmaAddr == 0x20) {
          DEBUG ((DEBUG_INFO, "Copying block\n"));
          DEBUG ((DEBUG_INFO, "Copying to %X from %X size %d\n", MemoryBlock, SdController->Block[0], SD_CONTROLLER_MODE_BLOCK_SIZE));
          CopyMem (MemoryBlock, SdController->Block[0], SD_CONTROLLER_MODE_BLOCK_SIZE);
          RegisterSpace->RegisterSpace.Write (
            &RegisterSpace->RegisterSpace,
            SD_MMC_HC_NOR_INT_STS,
            4,
            0x3
            );
      }
    } else {
      DEBUG ((DEBUG_INFO, "PIO transfer\n"));
      SdController->PioTrasnferStart = TRUE;
      SdController->CurrentPioIndex = 0;
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
  } else if (Address == SD_MMC_HC_HOST_CTRL1) {
    if ((*Value) & BIT0) {
      SdController->LedWasEnabled = TRUE;
    }
  }
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

EFI_STATUS
SdControllerPciRead (
  IN REGISTER_SPACE_MOCK  *RegisterSpace,
  IN UINT64          Address,
  IN UINT32          Size,
  OUT UINT64         *Value
  )
{
  return EFI_SUCCESS;
}

EFI_STATUS
SdControllerPciWrite (
  IN REGISTER_SPACE_MOCK  *RegisterSpace,
  IN UINT64          Address,
  IN UINT32          Size,
  IN UINT64          Value
  )
{
  return EFI_SUCCESS;
}

GLOBAL_REMOVE_IF_UNREFERENCED  REGISTER_SPACE_MOCK  SdControllerPciSpace = {
  L"SD controller PCI config",
  SdControllerPciRead,
  SdControllerPciWrite
};

extern SD_MMC_HC_PRIVATE_DATA  gSdMmcPciHcTemplate;

EFI_STATUS
SdMmcPrivateDataBuildControllerReadyToTransfer (
  OUT SD_MMC_HC_PRIVATE_DATA  **Private,
  OUT SD_CONTROLLER_CONTEXT   **ControllerContext
  )
{
  MOCK_PCI_DEVICE  *MockPciDevice;
  EFI_PCI_IO_PROTOCOL  *MockPciIo;
  MAP_BASED_REGISTER_SPACE  *SdBar;
  SD_CONTROLLER_CONTEXT     *Context;
  UINTN                     Index;

  *Private = AllocateCopyPool (sizeof (SD_MMC_HC_PRIVATE_DATA), &gSdMmcPciHcTemplate);
  Context = AllocateZeroPool (sizeof (SD_CONTROLLER_CONTEXT));
  *ControllerContext = Context;

  for (Index = 0; Index < SD_CONTROLLER_MODEL_NUM_OF_BLOCKS; Index++) {
    CopyMem (Context->Block[Index], gTestBlock, SD_CONTROLLER_MODE_BLOCK_SIZE);
  }


  InitializeListHead (&(*Private)->Queue);

  MockPciDeviceInitialize (&SdControllerPciSpace, &MockPciDevice);

  MapBasedRegisterSpaceCreate (
    L"SD BAR",
    gSdMemMap,
    ARRAY_SIZE (gSdMemMap),
    SdMmcPreWrite,
    Context,
    SdMmcPostRead,
    Context,
    &SdBar
  );

  MockPciDeviceRegisterBar (MockPciDevice, (REGISTER_SPACE_MOCK*) SdBar, 0);

  MockPciIoCreate (MockPciDevice, &MockPciIo);

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
  OUT SD_MMC_HC_PRIVATE_DATA  **Private,
  OUT SD_CONTROLLER_CONTEXT   **ControllerContext
  )
{
  MOCK_PCI_DEVICE  *MockPciDevice;
  EFI_PCI_IO_PROTOCOL  *MockPciIo;
  MAP_BASED_REGISTER_SPACE  *SdBar;
  UINTN                     Index;
  SD_CONTROLLER_CONTEXT     *Context;

  *Private = AllocateCopyPool (sizeof (SD_MMC_HC_PRIVATE_DATA), &gSdMmcPciHcTemplate);
  Context = AllocateZeroPool (sizeof (SD_CONTROLLER_CONTEXT));
  *ControllerContext = Context;

  for (Index = 0; Index < SD_CONTROLLER_MODEL_NUM_OF_BLOCKS; Index++) {
    CopyMem (Context->Block[Index], gTestBlock, SD_CONTROLLER_MODE_BLOCK_SIZE);
  }

  InitializeListHead (&(*Private)->Queue);

  MapBasedRegisterSpaceCreate (
    L"SD BAR",
    gSdMemMap,
    ARRAY_SIZE (gSdMemMap),
    SdMmcPreWrite,
    Context,
    SdMmcPostRead,
    Context,
    &SdBar
  );

  MockPciDeviceInitialize (&SdControllerPciSpace, &MockPciDevice);

  MockPciDeviceRegisterBar (MockPciDevice, (REGISTER_SPACE_MOCK*) SdBar, 0);

  MockPciIoCreate (MockPciDevice, &MockPciIo);

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

VOID
SdMmcCreateSingleBlockWritePacket (
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
  Packet->InDataBuffer = NULL;
  Packet->OutDataBuffer = BlockBuffer;
  Packet->InTransferLength = 0;
  Packet->OutTransferLength = BlockSize;
  Packet->TransactionStatus = EFI_SUCCESS;
  CommandBlock->CommandIndex = SD_WRITE_SINGLE_BLOCK;
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
  TEST_CONTEXT  *TestContext;
  EFI_SD_MMC_PASS_THRU_COMMAND_PACKET  Packet;
  EFI_SD_MMC_COMMAND_BLOCK  CommandBlock;
  EFI_SD_MMC_STATUS_BLOCK   StatusBlock;
  EFI_STATUS                Status;
  UINT8                     HostCtl1;

  TestContext = (TEST_CONTEXT*) Context;

  MemoryBlock = AllocateZeroPool (512);
  SdMmcCreateSingleBlockTransferPacket (MemoryBlock, 512, &Packet, &CommandBlock, &StatusBlock);
  Status = TestContext->Private->PassThru.PassThru (&TestContext->Private->PassThru, 0, &Packet, NULL);

  UT_ASSERT_EQUAL (Status, EFI_SUCCESS);
  UT_ASSERT_MEM_EQUAL (MemoryBlock, gTestBlock, sizeof (gTestBlock));
  UT_ASSERT_EQUAL (TestContext->ControllerContext->LedWasEnabled, TRUE);
  TestContext->Private->PciIo->Mem.Read (
    TestContext->Private->PciIo,
    EfiPciIoWidthUint8,
    0,
    SD_MMC_HC_HOST_CTRL1,
    1,
    &HostCtl1
  );
  UT_ASSERT_EQUAL (HostCtl1 & BIT0, 0); // Test that LED is disabled after command completion

  return UNIT_TEST_PASSED;
}

EFIAPI
UNIT_TEST_STATUS
SdMmcLedShouldBeEnabledForBlockTransfer (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  TEST_CONTEXT  *TestContext;
  EFI_SD_MMC_PASS_THRU_COMMAND_PACKET  Packet;
  EFI_SD_MMC_COMMAND_BLOCK  CommandBlock;
  EFI_SD_MMC_STATUS_BLOCK   StatusBlock;
  EFI_STATUS                Status;
  UINT8                     HostCtl1;

  TestContext = (TEST_CONTEXT*) Context;

  TestContext->ControllerContext->LedWasEnabled = FALSE;
  MemoryBlock = AllocateZeroPool (512);
  SdMmcCreateSingleBlockTransferPacket (MemoryBlock, 512, &Packet, &CommandBlock, &StatusBlock);
  Status = TestContext->Private->PassThru.PassThru (&TestContext->Private->PassThru, 0, &Packet, NULL);

  UT_ASSERT_EQUAL (Status, EFI_SUCCESS);
  UT_ASSERT_EQUAL (TestContext->ControllerContext->LedWasEnabled, TRUE);
  TestContext->Private->PciIo->Mem.Read (
    TestContext->Private->PciIo,
    EfiPciIoWidthUint8,
    0,
    SD_MMC_HC_HOST_CTRL1,
    1,
    &HostCtl1
  );
  UT_ASSERT_EQUAL (HostCtl1 & BIT0, 0); // Test that LED is disabled after command completion

  return UNIT_TEST_PASSED;
}

typedef enum {
  QemuPciCfg,
  QemuBar
} QEMU_REGISTER_SPACE_TYPE;

typedef struct {
  REGISTER_SPACE_MOCK       RegisterSpace;
  QEMU_REGISTER_SPACE_TYPE  Type;
  QPCIDevice                *Device;
  QPCIBar                   Bar;
} QEMU_REGISTER_SPACE_MOCK;

EFI_STATUS
QemuPciRegisterSpaceRead (
  IN REGISTER_SPACE_MOCK  *RegisterSpace,
  IN UINT64               Address,
  IN UINT32               Size,
  OUT UINT64              *Value
  )
{
  QEMU_REGISTER_SPACE_MOCK  *QemuRegisterSpace;

  QemuRegisterSpace = (QEMU_REGISTER_SPACE_MOCK*) RegisterSpace;

  //DEBUG ((DEBUG_INFO, "Qemu read\n"));
  //DEBUG ((DEBUG_INFO, "Address = %X\n", Address));
  //DEBUG ((DEBUG_INFO, "Size = %X\n", Size));

  switch (QemuRegisterSpace->Type) {
    case QemuPciCfg:
      //DEBUG ((DEBUG_INFO, "Type = PCI\n"));
      switch (Size) {
        case 1:
          *(UINT8*)Value = qpci_config_readb (QemuRegisterSpace->Device, (uint8_t) Address);
          break;
        case 2:
          *(UINT16*)Value = qpci_config_readw (QemuRegisterSpace->Device, (uint8_t) Address);
          break;
        case 4:
          *(UINT32*)Value = qpci_config_readl (QemuRegisterSpace->Device, (uint8_t) Address);
          break;
        default:
          DEBUG ((DEBUG_ERROR, "Incorrect data width\n"));
          *Value = 0xFFFFFFFFFFFFFFFF;
          return EFI_DEVICE_ERROR;
      }
      break;
    case QemuBar:
      //DEBUG ((DEBUG_INFO, "Type = BAR\n"));
      switch (Size) {
        case 1:
          *(UINT8*)Value = qpci_io_readb (QemuRegisterSpace->Device, QemuRegisterSpace->Bar, Address);
          break;
        case 2:
          *(UINT16*)Value = qpci_io_readw (QemuRegisterSpace->Device, QemuRegisterSpace->Bar, Address);
          break;
        case 4:
          *(UINT32*)Value = qpci_io_readl (QemuRegisterSpace->Device, QemuRegisterSpace->Bar, Address);
          break;
        default:
          DEBUG ((DEBUG_ERROR, "Incorrect data width\n"));
          *Value = 0xFFFFFFFFFFFFFFFF;
          return EFI_DEVICE_ERROR;
      }
      break;
  }
  //DEBUG ((DEBUG_INFO, "Value %X\n", *Value));
  return EFI_SUCCESS;
}

EFI_STATUS
QemuPciRegisterSpaceWrite (
  IN REGISTER_SPACE_MOCK  *RegisterSpace,
  IN UINT64               Address,
  IN UINT32               Size,
  IN UINT64               Value
  )
{
  QEMU_REGISTER_SPACE_MOCK  *QemuRegisterSpace;

  QemuRegisterSpace = (QEMU_REGISTER_SPACE_MOCK*) RegisterSpace;

  //DEBUG ((DEBUG_INFO, "QemuWrite\n"));
  //DEBUG ((DEBUG_INFO, "Address = %X\n", Address));
  //DEBUG ((DEBUG_INFO, "Size = %X\n", Size));

  switch (QemuRegisterSpace->Type) {
    case QemuPciCfg:
      //DEBUG ((DEBUG_INFO, "Type = PCI\n"));
      switch (Size) {
        case 1:
          qpci_config_writeb (QemuRegisterSpace->Device, (uint8_t) Address, (uint8_t) Value);
          break;
        case 2:
          qpci_config_writew (QemuRegisterSpace->Device, (uint8_t) Address, (uint16_t) Value);
          break;
        case 4:
          qpci_config_writel (QemuRegisterSpace->Device, (uint8_t) Address, (uint32_t) Value);
          break;
        default:
          DEBUG ((DEBUG_ERROR, "Incorrect data width\n"));
          return EFI_UNSUPPORTED;
      }
      break;
    case QemuBar:
      //DEBUG ((DEBUG_INFO, "Type = BAR\n"));
      switch (Size) {
        case 1:
          qpci_io_writeb (QemuRegisterSpace->Device, QemuRegisterSpace->Bar, Address, (uint8_t) Value);
          break;
        case 2:
          qpci_io_writew (QemuRegisterSpace->Device, QemuRegisterSpace->Bar, Address, (uint16_t) Value);
          break;
        case 4:
          qpci_io_writel (QemuRegisterSpace->Device, QemuRegisterSpace->Bar, Address, (uint32_t) Value);
          break;
        default:
          DEBUG ((DEBUG_ERROR, "Incorrect data width\n"));
          return EFI_UNSUPPORTED;
      }
      break;
  }
  return EFI_SUCCESS;
}

VOID
QemuRegisterSpaceInit (
  IN CHAR16                    *RegisterSpaceName,
  IN QEMU_REGISTER_SPACE_TYPE  Type,
  IN UINTN                     BarNo,
  IN QOSState                  *Qs,
  OUT REGISTER_SPACE_MOCK      **RegisterSpaceMock
  )
{
  QEMU_REGISTER_SPACE_MOCK  *QemuRegisterSpace;
  QPCIBus                   *PciBus;
  QPCIDevice                *SdhciDevice;
  UINTN                     Device;
  UINTN                     Function;

  QemuRegisterSpace = (QEMU_REGISTER_SPACE_MOCK*) AllocateZeroPool (sizeof (QEMU_REGISTER_SPACE_MOCK));
  QemuRegisterSpace->RegisterSpace.Name = RegisterSpaceName;
  QemuRegisterSpace->RegisterSpace.Read = QemuPciRegisterSpaceRead;
  QemuRegisterSpace->RegisterSpace.Write = QemuPciRegisterSpaceWrite;
  QemuRegisterSpace->Type = Type;
  QemuRegisterSpace->Device = NULL;
  *RegisterSpaceMock = &QemuRegisterSpace->RegisterSpace;

  PciBus = qpci_new_pc (Qs->qts, NULL);
  if (PciBus == NULL) {
    DEBUG ((DEBUG_INFO, "Failed to get pci bus\n"));
  }
  for (Device = 0; Device < 32; Device++) {
    for (Function = 0; Function < 8; Function++) {
      SdhciDevice = qpci_device_find (PciBus, QPCI_DEVFN (Device, Function));
      if (SdhciDevice == NULL) {
        continue;
      }
      if (qpci_config_readw (SdhciDevice, 0xA) == 0x0805) {
        DEBUG ((DEBUG_INFO, "Found SDHCI at Dev %X, Fun %X\n", Device, Function));
        if (Type == QemuBar) {
          QemuRegisterSpace->Bar = qpci_iomap (SdhciDevice, BarNo, NULL);
          qpci_device_enable (SdhciDevice);
        }
        QemuRegisterSpace->Device = SdhciDevice;
        break;
      }
      g_free (SdhciDevice);
    }
    if (QemuRegisterSpace->Device != NULL) {
      break;
    }
  }
}

EFI_STATUS
SdMmcQemuBuildControllerReadyForPioTransfer (
  IN QOSState *Qs,
  OUT EFI_PCI_IO_PROTOCOL  **PciIo
  )
{
  MOCK_PCI_DEVICE  *MockPciDevice;
  EFI_PCI_IO_PROTOCOL  *MockPciIo;
  REGISTER_SPACE_MOCK   *PciConfigRegisterSpace;
  REGISTER_SPACE_MOCK   *BarRegisterSpace;

  QemuRegisterSpaceInit (L"SD PCI config", QemuPciCfg, 0, Qs, &PciConfigRegisterSpace);
  QemuRegisterSpaceInit (L"SD MEM", QemuBar, 0, Qs, &BarRegisterSpace);

  MockPciDeviceInitialize (&SdControllerPciSpace, &MockPciDevice);

  MockPciDeviceRegisterBar (MockPciDevice, (REGISTER_SPACE_MOCK*) BarRegisterSpace, 0);

  MockPciIoCreate (MockPciDevice, &MockPciIo);

  *PciIo = MockPciIo;

  return EFI_SUCCESS;
}

extern EFI_DRIVER_BINDING_PROTOCOL  gSdMmcPciHcDriverBinding;

EFI_STATUS
EFIAPI
SdMmcPciHcDriverBindingStart (
  IN EFI_DRIVER_BINDING_PROTOCOL  *This,
  IN EFI_HANDLE                   Controller,
  IN EFI_DEVICE_PATH_PROTOCOL     *RemainingDevicePath
  );

UNIT_TEST_STATUS
EFIAPI
SdMmcDriverShouldInitializeHostController (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS  Status;
  EFI_SD_MMC_PASS_THRU_PROTOCOL *PassThru;

  Status = SdMmcPciHcDriverBindingStart ((VOID*)&gSdMmcPciHcDriverBinding, *(EFI_HANDLE*)Context, NULL);
  UT_ASSERT_EQUAL (Status, EFI_SUCCESS);

  Status = gBS->LocateProtocol (&gEfiSdMmcPassThruProtocolGuid, NULL, (VOID*) &PassThru);
  UT_ASSERT_EQUAL (Status, EFI_SUCCESS);

  return UNIT_TEST_PASSED;
}

UNIT_TEST_STATUS
EFIAPI
SdMmcSingleBlockWriteShouldSucceed (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS                Status;
  EFI_SD_MMC_PASS_THRU_COMMAND_PACKET  Packet;
  EFI_SD_MMC_COMMAND_BLOCK  CommandBlock;
  EFI_SD_MMC_STATUS_BLOCK   StatusBlock;
  EFI_SD_MMC_PASS_THRU_PROTOCOL *PassThru;

  Status = gBS->LocateProtocol (&gEfiSdMmcPassThruProtocolGuid, NULL, (VOID*) &PassThru);
  if (EFI_ERROR (Status)) {
    return UNIT_TEST_ERROR_PREREQUISITE_NOT_MET;
  }

  SdMmcCreateSingleBlockWritePacket (gTestBlock, 512, &Packet, &CommandBlock, &StatusBlock);
  Status = PassThru->PassThru (PassThru, 0, &Packet, NULL);

  UT_ASSERT_EQUAL (Status, EFI_SUCCESS);

  return UNIT_TEST_PASSED;
}

UNIT_TEST_STATUS
EFIAPI
SdMmcSingleBlockReadShouldReturnDataBlock (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS                Status;
  EFI_SD_MMC_PASS_THRU_COMMAND_PACKET  Packet;
  EFI_SD_MMC_COMMAND_BLOCK  CommandBlock;
  EFI_SD_MMC_STATUS_BLOCK   StatusBlock;
  EFI_SD_MMC_PASS_THRU_PROTOCOL *PassThru;

  Status = gBS->LocateProtocol (&gEfiSdMmcPassThruProtocolGuid, NULL, (VOID*) &PassThru);
  if (EFI_ERROR (Status)) {
    return UNIT_TEST_ERROR_PREREQUISITE_NOT_MET;
  }

  MemoryBlock = AllocateZeroPool (512);
  SdMmcCreateSingleBlockTransferPacket (MemoryBlock, 512, &Packet, &CommandBlock, &StatusBlock);
  Status = PassThru->PassThru (PassThru, 0, &Packet, NULL);

  UT_ASSERT_EQUAL (Status, EFI_SUCCESS);
  UT_ASSERT_MEM_EQUAL (gTestBlock, MemoryBlock, sizeof (gTestBlock));

  return UNIT_TEST_PASSED;
}

VOID
InitializeBootServices (
  VOID
  );

EFI_STATUS
EFIAPI
UefiTestMain (
  VOID
  )
{
  EFI_STATUS                  Status;
  UNIT_TEST_FRAMEWORK_HANDLE  Framework;
  UNIT_TEST_SUITE_HANDLE      SdMmcPassThruTest;
  UNIT_TEST_SUITE_HANDLE      SdMmcPassThruQemuTest;
  TEST_CONTEXT            SdmaTestContext;
  TEST_CONTEXT            PioTestContext;

  InitializeBootServices ();

  SdMmcBuildControllerReadyForPioTransfer (&PioTestContext.Private, &PioTestContext.ControllerContext);
  SdMmcPrivateDataBuildControllerReadyToTransfer (&SdmaTestContext.Private, &SdmaTestContext.ControllerContext);

  Framework = NULL;

  DEBUG ((DEBUG_INFO, "%a v%a\n", UNIT_TEST_NAME, UNIT_TEST_VERSION));

  Status = InitUnitTestFramework (&Framework, UNIT_TEST_NAME, gEfiCallerBaseName, UNIT_TEST_VERSION);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Failed in InitUnitTestFramework. Status = %r\n", Status));
    return Status;
  }

  Status = CreateUnitTestSuite (&SdMmcPassThruTest, Framework, "SdMmcPassThruTestsWithLocalModel", "SdMmc.PassThru", NULL, NULL);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  AddTestCase (SdMmcPassThruTest, "SingleBlockTestSdma", "SingleBlockTestSdma", SdMmcSignleBlockReadShouldReturnDataBlockFromDevice, NULL, NULL, &SdmaTestContext);
  AddTestCase (SdMmcPassThruTest, "SingleBlockTestPio", "SingleBlockTestPio", SdMmcSignleBlockReadShouldReturnDataBlockFromDevice, NULL, NULL, &PioTestContext);
  AddTestCase (SdMmcPassThruTest, "LedControlTest", "LedControlTest", SdMmcLedShouldBeEnabledForBlockTransfer, NULL, NULL, &SdmaTestContext);

  Status = CreateUnitTestSuite (&SdMmcPassThruQemuTest, Framework, "SdMmcPassThruTestsWithQemuModel", "SdMmmc.PassThru", NULL, NULL);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  // QEMU tests

  const char*  cli = "-M q35 -device sdhci-pci -device sd-card,drive=mydrive -drive id=mydrive,if=none,format=raw,file=/home/matalbec/vm_images/sdcard.img";
  QOSState *qs;
  EFI_HANDLE Controller = NULL;
  EFI_PCI_IO_PROTOCOL  *PciIo;

  qs = qtest_pc_boot(cli);

  SdMmcQemuBuildControllerReadyForPioTransfer (qs, &PciIo);
  gBS->InstallProtocolInterface (&Controller, &gEfiPciIoProtocolGuid, EFI_NATIVE_INTERFACE, (VOID*) PciIo);

  AddTestCase (SdMmcPassThruQemuTest, "HostControllerInit", "HostControllerInit", SdMmcDriverShouldInitializeHostController, NULL, NULL, &Controller); // Has to be done first
  AddTestCase (SdMmcPassThruQemuTest, "BlockIoPio", "Write", SdMmcSingleBlockWriteShouldSucceed, NULL, NULL, NULL); // Has to be done before read test
  AddTestCase (SdMmcPassThruQemuTest, "BlockIoPio", "Read", SdMmcSingleBlockReadShouldReturnDataBlock, NULL, NULL, NULL);

  Status = RunAllTestSuites (Framework);

  qtest_shutdown (qs);

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
