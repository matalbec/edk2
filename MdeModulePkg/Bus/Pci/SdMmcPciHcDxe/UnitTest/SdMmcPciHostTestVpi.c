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

#include "../SdMmcPciHcDxe.h"
#include "../SdMmcPciHci.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define UNIT_TEST_NAME     "SdMmcPciHc driver unit tests"
#define UNIT_TEST_VERSION  "0.1"

typedef struct {
  REGISTER_SPACE_MOCK RegisterSpace;
  BOOLEAN mem_type;
  int socket;
} VPI_SIM_REGISTER_SPACE;

EFI_STATUS
VpiPciRegisterSpaceRead (
  IN REGISTER_SPACE_MOCK  *RegisterSpace,
  IN UINT64               Address,
  IN UINT32               Size,
  OUT UINT64              *Value
  )
{
  VPI_SIM_REGISTER_SPACE  *VpiSpace;
  char msg[256];
  UINT64 Val;

  VpiSpace = (VPI_SIM_REGISTER_SPACE*) RegisterSpace;

  if (VpiSpace->mem_type) {
    sprintf (msg, "read mem %d %d", Address, Size);
  } else {
    sprintf (msg, "read pci %d %d", Address, Size);
  }

  send (VpiSpace->socket, msg, strlen(msg), 0);
  recv (VpiSpace->socket, msg, 255, 0);
  DEBUG ((DEBUG_INFO, "Read response %a\n", msg));
  Val = atoi (msg);
  switch (Size) {
    case 1:
      *(UINT8*)Value = Val;
      break;
    case 2:
      *(UINT16*)Value = Val;
      break;
    case 4:
      *(UINT32*)Value = Val;
      break;
    default:
      DEBUG ((DEBUG_ERROR, "Incorrect data width\n"));
      return EFI_DEVICE_ERROR;
  }
  DEBUG ((DEBUG_INFO, "Read returned %X\n", Val));
  return EFI_SUCCESS;
}

EFI_STATUS
VpiPciRegisterSpaceWrite (
  IN REGISTER_SPACE_MOCK  *RegisterSpace,
  IN UINT64               Address,
  IN UINT32               Size,
  IN UINT64               Value
  )
{
  VPI_SIM_REGISTER_SPACE *VpiSpace;
  char msg[256];

  VpiSpace = (VPI_SIM_REGISTER_SPACE*) RegisterSpace;

  if (VpiSpace->mem_type) {
    sprintf (msg, "write mem %d %d %d", Address, Size, Value);
  } else {
    sprintf (msg, "write pci %d %d %d", Address, Size, Value);
  }

  send (VpiSpace->socket, msg, strlen(msg), 0);
  recv (VpiSpace->socket, msg, 255, 0);
  return EFI_SUCCESS;
}

VOID
VpiRegisterSpaceInit (
  IN CHAR16                    *RegisterSpaceName,
  IN int                       sock,
  IN BOOLEAN                   mem_type,
  OUT REGISTER_SPACE_MOCK      **RegisterSpaceMock
  )
{
  VPI_SIM_REGISTER_SPACE *VpiRegSpace;

  VpiRegSpace = (VPI_SIM_REGISTER_SPACE*) AllocateZeroPool (sizeof (VPI_SIM_REGISTER_SPACE));
  VpiRegSpace->RegisterSpace.Name = RegisterSpaceName;
  VpiRegSpace->RegisterSpace.Read = VpiPciRegisterSpaceRead;
  VpiRegSpace->RegisterSpace.Write = VpiPciRegisterSpaceWrite;
  VpiRegSpace->socket = sock;
  VpiRegSpace->mem_type = mem_type;

  *RegisterSpaceMock = (REGISTER_SPACE_MOCK*) VpiRegSpace;
}

int
InitializeConnectionToSim (
  VOID
  )
{
  int sockfd, portno;
  struct sockaddr_in serv_addr;
  struct hostent *server;

  sockfd = socket (AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0) {
    return -1;
  }

  server = gethostbyname("localhost");
  if (server == NULL) {
    DEBUG ((DEBUG_INFO, "Failed to get server\n"));
    return -1;
  }

  ZeroMem (&serv_addr, sizeof (serv_addr));
  serv_addr.sin_family = AF_INET;
  CopyMem (&serv_addr.sin_addr.s_addr, server->h_addr, server->h_length);
  portno = 5001;
  serv_addr.sin_port = htons(portno);

  if (connect(sockfd, (struct sockaddr*)&serv_addr, sizeof (serv_addr)) < 0) {
    DEBUG ((DEBUG_INFO, "Failed to connect to sim\n"));
    return -1;
  } else {
    DEBUG ((DEBUG_INFO, "Connected to sim server\n"));
  }

  return sockfd;
}

EFI_STATUS
SdMmcVpiBuildControllerReadyForPioTransfer (
  OUT EFI_PCI_IO_PROTOCOL  **PciIo,
  OUT int *sock
  )
{
  MOCK_PCI_DEVICE *MockPciDevice;
  EFI_PCI_IO_PROTOCOL  *MockPciIo;
  REGISTER_SPACE_MOCK  *PciRegisterSpace;
  REGISTER_SPACE_MOCK  *MemRegisterSpace;

  *sock = InitializeConnectionToSim();
  if (sock < 0) {
    return EFI_DEVICE_ERROR;
  }

  VpiRegisterSpaceInit(L"SD PCI config VPI space", *sock, FALSE, &PciRegisterSpace);
  VpiRegisterSpaceInit(L"SD mem VPI space", *sock, TRUE, &MemRegisterSpace);

  MockPciDeviceInitialize (PciRegisterSpace, &MockPciDevice);

  MockPciDeviceRegisterBar (MockPciDevice, MemRegisterSpace, 0);

  MockPciIoCreate (MockPciDevice, &MockPciIo);

  *PciIo = MockPciIo;

  return EFI_SUCCESS;
}

EFI_STATUS
CloseVpiConnection (
  IN int sock
  )
{
  char* msg;

  msg = "done";
  send(sock, msg, strlen(msg), 0);
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

extern SD_MMC_HC_PRIVATE_DATA  gSdMmcPciHcTemplate;

EFI_STATUS
SdMmcBuildPassThru (
  IN EFI_PCI_IO_PROTOCOL *PciIo,
  OUT EFI_SD_MMC_PASS_THRU_PROTOCOL **SdMmc
  )
{
  SD_MMC_HC_PRIVATE_DATA  *Private;

  Private = AllocateCopyPool (sizeof (SD_MMC_HC_PRIVATE_DATA), &gSdMmcPciHcTemplate);
  InitializeListHead (&Private->Queue);
  Private->Slot[0].Enable = TRUE;
  Private->Slot[0].MediaPresent = TRUE;
  Private->Slot[0].Initialized = TRUE;
  Private->Slot[0].CardType = SdCardType;
  Private->Capability[0].Adma2 = FALSE;
  Private->Capability[0].Sdma = FALSE;
  Private->ControllerVersion[0] = SD_MMC_HC_CTRL_VER_300;
  Private->PciIo = PciIo;

  *SdMmc = &Private->PassThru;

  return EFI_SUCCESS;
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
  EFI_PCI_IO_PROTOCOL         *PciIo;
  EFI_SD_MMC_PASS_THRU_PROTOCOL  *SdMmcPassThru;
  EFI_HANDLE                  Controller = NULL;
  int                         sock;

  InitializeBootServices ();
  Status = SdMmcVpiBuildControllerReadyForPioTransfer (&PciIo, &sock);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_INFO, "Failed to init connection\n"));
    return Status;
  }

  Status = SdMmcBuildPassThru (PciIo, &SdMmcPassThru);

  gBS->InstallProtocolInterface (&Controller, &gEfiPciIoProtocolGuid, EFI_NATIVE_INTERFACE, (VOID*) PciIo);
  gBS->InstallProtocolInterface (&Controller, &gEfiSdMmcPassThruProtocolGuid, EFI_NATIVE_INTERFACE, (VOID*) SdMmcPassThru);

  Framework = NULL;

  DEBUG ((DEBUG_INFO, "%a v%a\n", UNIT_TEST_NAME, UNIT_TEST_VERSION));

  Status = InitUnitTestFramework (&Framework, UNIT_TEST_NAME, gEfiCallerBaseName, UNIT_TEST_VERSION);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Failed in InitUnitTestFramework. Status = %r\n", Status));
    return Status;
  }

  Status = CreateUnitTestSuite (&SdMmcPassThruTest, Framework, "SdMmcPassThruTestsWithDesignSim", "SdMmc.PassThru", NULL, NULL);
  if (EFI_ERROR (Status)) {
    return Status;
  }
  AddTestCase (SdMmcPassThruTest, "BlockPio", "Write", SdMmcSingleBlockWriteShouldSucceed, NULL, NULL, NULL);
  AddTestCase (SdMmcPassThruTest, "BlockPio", "Read", SdMmcSingleBlockReadShouldReturnDataBlock, NULL, NULL, NULL);

  Status = RunAllTestSuites (Framework);

  CloseVpiConnection(sock);

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
