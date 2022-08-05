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

#define UNIT_TEST_NAME     "SdMmcPciHc driver unit tests"
#define UNIT_TEST_VERSION  "0.1"

#define SD_CONTROLLER_MODEL_NUM_OF_BLOCKS  5
#define SD_CONTROLLER_MODE_BLOCK_SIZE  512

typedef struct {
  BOOLEAN  PioTrasnferStart;
  UINTN    CurrentPioIndex;
  UINT8    Block[SD_CONTROLLER_MODEL_NUM_OF_BLOCKS][SD_CONTROLLER_MODE_BLOCK_SIZE];
} SD_CONTROLLER_CONTEXT;

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
  OUT SD_MMC_HC_PRIVATE_DATA  **Private
  )
{
  MOCK_PCI_DEVICE  *MockPciDevice;
  EFI_PCI_IO_PROTOCOL  *MockPciIo;
  MAP_BASED_REGISTER_SPACE  *SdBar;
  SD_CONTROLLER_CONTEXT     *Context;
  UINTN                     Index;

  *Private = AllocateCopyPool (sizeof (SD_MMC_HC_PRIVATE_DATA), &gSdMmcPciHcTemplate);
  Context = AllocateZeroPool (sizeof (SD_CONTROLLER_CONTEXT));

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
  OUT SD_MMC_HC_PRIVATE_DATA  **Private
  )
{
  MOCK_PCI_DEVICE  *MockPciDevice;
  EFI_PCI_IO_PROTOCOL  *MockPciIo;
  MAP_BASED_REGISTER_SPACE  *SdBar;
  UINTN                     Index;
  SD_CONTROLLER_CONTEXT     *Context;

  *Private = AllocateCopyPool (sizeof (SD_MMC_HC_PRIVATE_DATA), &gSdMmcPciHcTemplate);
  Context = AllocateZeroPool (sizeof (SD_CONTROLLER_CONTEXT));

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
  UT_ASSERT_MEM_EQUAL (MemoryBlock, gTestBlock, sizeof (gTestBlock));

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

  AddTestCase (SdMmcPassThruTest, "SingleBlockTestSdma", "SingleBlockTestSdma", SdMmcSignleBlockReadShouldReturnDataBlockFromDevice, NULL, NULL, PrivateForSdma);
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