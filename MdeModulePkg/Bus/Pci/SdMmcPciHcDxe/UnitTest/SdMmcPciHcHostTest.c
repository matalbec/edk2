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
#include <LocalRegisterSpaceLib.h>

#include "../SdMmcPciHcDxe.h"
#include "../SdMmcPciHci.h"

#define UNIT_TEST_NAME     "SdMmcPciHc driver unit tests"
#define UNIT_TEST_VERSION  "0.1"

#define SD_CONTROLLER_MODEL_NUM_OF_BLOCKS  5
#define SD_CONTROLLER_MODE_BLOCK_SIZE  512

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

typedef struct {
  BOOLEAN  PioTransferStart;
  UINTN    CurrentPioIndex;
  UINT8    Block[SD_CONTROLLER_MODEL_NUM_OF_BLOCKS][SD_CONTROLLER_MODE_BLOCK_SIZE];
  BOOLEAN  LedWasEnabled;
  UINT32   NormalErrorInterruptStatus;
  UINT16   TransferMode;
  UINT32   SdmaAddress;
} SD_LOCAL_DEVICE_MODEL;

typedef struct {
  SD_MMC_HC_PRIVATE_DATA  *Private;
  SD_LOCAL_DEVICE_MODEL   *Device;
} TEST_CONTEXT;

STATIC
UINT32
ByteEnableToBitMask (
  IN UINT32 ByteEnable
  )
{
  UINT32 Index;
  UINT32 BitMask;

  BitMask = 0;
  for (Index = 0; Index < 4; Index++) {
    if (ByteEnable & 0x1) {
      BitMask |= (0xFF << (Index * 8));
    }
    ByteEnable = ByteEnable >> 1;
  }

  return BitMask;
}

VOID
SdMmcBarLocalModelRead (
  IN VOID                   *Context,
  IN  UINT64                Address,
  IN  UINT32                ByteEnable,
  OUT UINT32                *Value
  )
{
  SD_LOCAL_DEVICE_MODEL  *SdController;
  UINT32                 *Block32;
  UINT32                 BitMask;

  SdController = (SD_LOCAL_DEVICE_MODEL*) Context;

  BitMask = ByteEnableToBitMask (ByteEnable);

  switch (Address) {
    case SD_MMC_HC_BUF_DAT_PORT:
      if (SdController->PioTransferStart && (SdController->CurrentPioIndex < SD_CONTROLLER_MODE_BLOCK_SIZE / sizeof (UINT32))) {
        Block32 = (UINT32*) SdController->Block[0];
        *Value = Block32[SdController->CurrentPioIndex];
        SdController->CurrentPioIndex++;
        if (SdController->CurrentPioIndex >= SD_CONTROLLER_MODE_BLOCK_SIZE / sizeof (UINT32)) {
          SdController->NormalErrorInterruptStatus |= BIT1;
        }
      }
      break;
    case SD_MMC_HC_NOR_INT_STS:
      *Value = (SdController->NormalErrorInterruptStatus & BitMask);
      break;
    case SD_MMC_HC_TRANS_MOD:
    case SD_MMC_HC_PRESENT_STATE:
    case SD_MMC_HC_HOST_CTRL1:
    case SD_MMC_HC_SDMA_ADDR:
    case SD_MMC_HC_BLK_SIZE:
    case SD_MMC_HC_ARG1:
    case SD_MMC_HC_RESPONSE:
    case SD_MMC_HC_RESPONSE + 4:
    case SD_MMC_HC_RESPONSE + 8:
    case SD_MMC_HC_RESPONSE + 12:
      *Value = 0;
      break;
    default:
      DEBUG ((DEBUG_ERROR, "Unmapped read to SD host controller %X\n", Address));
      *Value = 0xFFFFFFFF;
      break;
  }
}

VOID
SdMmcBarLocalModelWrite (
  IN VOID                  *Context,
  IN UINT64                Address,
  IN UINT32                ByteEnable,
  IN UINT32                Value
  )
{
  SD_LOCAL_DEVICE_MODEL  *SdController;
  UINT32                 BitMask;

  SdController = (SD_LOCAL_DEVICE_MODEL*) Context;

  BitMask = ByteEnableToBitMask (ByteEnable);

  switch (Address) {
    case SD_MMC_HC_SDMA_ADDR:
      SdController->SdmaAddress = Value;
      break;
    case SD_MMC_HC_TRANS_MOD:
      if (ByteEnable & 0x3) { // Transfer mode register
        SdController->TransferMode &= (UINT16)BitMask;
        SdController->TransferMode |= (UINT16)Value;
      }

      if (ByteEnable & 0xC) { // Command register
        if (SdController->TransferMode & BIT0) {
          DEBUG ((DEBUG_INFO, "DMA transfer\n"));
          if (SdController->SdmaAddress == 0x20) { // TODO: DMA implementation
            CopyMem (MemoryBlock, SdController->Block[0], SD_CONTROLLER_MODE_BLOCK_SIZE);
            SdController->NormalErrorInterruptStatus |= (BIT0 | BIT1);
          }
        } else {
          DEBUG ((DEBUG_INFO, "PIO transfer\n"));
          SdController->PioTransferStart = TRUE;
          SdController->CurrentPioIndex = 0;
          SdController->NormalErrorInterruptStatus |= (BIT5 | BIT0);
        }
      }
      break;
    case SD_MMC_HC_NOR_INT_STS:
      SdController->NormalErrorInterruptStatus = SdController->NormalErrorInterruptStatus & (~Value);
      break;
    case SD_MMC_HC_HOST_CTRL1:
      if (Value & BIT0) {
        SdController->LedWasEnabled = TRUE;
      }
      break;
    default:
      DEBUG ((DEBUG_ERROR, "Unmapped write to SD host controller %X\n", Address));
      break;
  }
}

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
  OUT SD_LOCAL_DEVICE_MODEL   **Device
  )
{
  MOCK_PCI_DEVICE  *MockPciDevice;
  EFI_PCI_IO_PROTOCOL  *MockPciIo;
  REGISTER_SPACE_MOCK  *SdBar;
  UINTN                     Index;

  *Private = AllocateCopyPool (sizeof (SD_MMC_HC_PRIVATE_DATA), &gSdMmcPciHcTemplate);
  (*Device) = AllocateZeroPool (sizeof (SD_LOCAL_DEVICE_MODEL));

  for (Index = 0; Index < SD_CONTROLLER_MODEL_NUM_OF_BLOCKS; Index++) {
    CopyMem ((*Device)->Block[Index], gTestBlock, SD_CONTROLLER_MODE_BLOCK_SIZE);
  }


  InitializeListHead (&(*Private)->Queue);

  MockPciDeviceInitialize (&SdControllerPciSpace, &MockPciDevice);

  LocalRegisterSpaceCreate (L"SD BAR", SdMmcBarLocalModelWrite, SdMmcBarLocalModelRead, *Device, &SdBar);

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
  OUT SD_LOCAL_DEVICE_MODEL   **Device
  )
{
  MOCK_PCI_DEVICE  *MockPciDevice;
  EFI_PCI_IO_PROTOCOL  *MockPciIo;
  REGISTER_SPACE_MOCK  *SdBar;
  UINTN                     Index;

  *Private = AllocateCopyPool (sizeof (SD_MMC_HC_PRIVATE_DATA), &gSdMmcPciHcTemplate);
  (*Device) = AllocateZeroPool (sizeof (SD_LOCAL_DEVICE_MODEL));

  for (Index = 0; Index < SD_CONTROLLER_MODEL_NUM_OF_BLOCKS; Index++) {
    CopyMem ((*Device)->Block[Index], gTestBlock, SD_CONTROLLER_MODE_BLOCK_SIZE);
  }

  InitializeListHead (&(*Private)->Queue);

  LocalRegisterSpaceCreate (L"SD BAR", SdMmcBarLocalModelWrite, SdMmcBarLocalModelRead, *Device, &SdBar);

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
  UT_ASSERT_EQUAL (TestContext->Device->LedWasEnabled, TRUE);
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

UNIT_TEST_STATUS
EFIAPI
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

  TestContext->Device->LedWasEnabled = FALSE;
  MemoryBlock = AllocateZeroPool (512);
  SdMmcCreateSingleBlockTransferPacket (MemoryBlock, 512, &Packet, &CommandBlock, &StatusBlock);
  Status = TestContext->Private->PassThru.PassThru (&TestContext->Private->PassThru, 0, &Packet, NULL);

  UT_ASSERT_EQUAL (Status, EFI_SUCCESS);
  UT_ASSERT_EQUAL (TestContext->Device->LedWasEnabled, TRUE);
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
  TEST_CONTEXT            SdmaTestContext;
  TEST_CONTEXT            PioTestContext;

  InitializeBootServices ();

  SdMmcBuildControllerReadyForPioTransfer (&PioTestContext.Private, &PioTestContext.Device);
  SdMmcPrivateDataBuildControllerReadyToTransfer (&SdmaTestContext.Private, &SdmaTestContext.Device);

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
