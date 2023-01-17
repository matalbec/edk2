
#include <PiPei.h>
#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/BaseMemoryLib.h>

EFI_STATUS
EFIAPI
Stall (
  IN UINTN  Microseconds
)
{
  return EFI_SUCCESS;
}

EFI_TPL
EFIAPI
RaiseTpl (
  IN EFI_TPL      NewTpl
  )
{
  return NewTpl;
}

VOID
EFIAPI
RestoreTpl (
  IN EFI_TPL      OldTpl
  )
{
}

#define MAX_PROTOCOLS_PER_HANDLE 10

typedef struct {
  EFI_GUID  Guid;
  VOID      *Interface;
} PROTOCOL_INFO;

typedef struct {
  UINT32  NoOfProtocols;
  PROTOCOL_INFO  Protocols[MAX_PROTOCOLS_PER_HANDLE];
} HANDLE_INFO;

#define MAX_HANDLES 5

HANDLE_INFO mHandleDatabase[MAX_HANDLES];

UINT32 mFirstFreeHandle = 0;

EFI_STATUS
EFIAPI
OpenProtocol (
  IN  EFI_HANDLE                Handle,
  IN  EFI_GUID                  *Protocol,
  OUT VOID                      **Interface  OPTIONAL,
  IN  EFI_HANDLE                AgentHandle,
  IN  EFI_HANDLE                ControllerHandle,
  IN  UINT32                    Attributes
  )
{
  UINT32 ProtocolIndex;
  HANDLE_INFO  *HandleInfo;
  PROTOCOL_INFO *ProtocolInfo;

  if (Interface == NULL) {
    return EFI_SUCCESS;
  }

  HandleInfo = (HANDLE_INFO*) Handle;

  for (ProtocolIndex = 0; ProtocolIndex < HandleInfo->NoOfProtocols; ProtocolIndex++) {
    ProtocolInfo = &HandleInfo->Protocols[ProtocolIndex];
    if (CompareMem (&ProtocolInfo->Guid, Protocol, sizeof (EFI_GUID)) == 0) {
      *Interface = ProtocolInfo->Interface;
      return EFI_SUCCESS;
    }
  }
  return EFI_NOT_FOUND;
}

EFI_STATUS
EFIAPI
LocateProtocol (
  IN  EFI_GUID  *Protocol,
  IN  VOID      *Registration  OPTIONAL,
  OUT VOID      **Interface
  )
{
  UINT32 ProtocolIndex;
  UINT32 HandleIndex;
  HANDLE_INFO *Handle;
  PROTOCOL_INFO *ProtocolInfo;

  for (HandleIndex = 0; HandleIndex < mFirstFreeHandle; HandleIndex++) {
    Handle = &mHandleDatabase[HandleIndex];
    for (ProtocolIndex = 0; ProtocolIndex < Handle->NoOfProtocols; ProtocolIndex++) {
      ProtocolInfo = &Handle->Protocols[ProtocolIndex];
      if (CompareMem (&ProtocolInfo->Guid, Protocol, sizeof (EFI_GUID)) == 0) {
        *Interface = ProtocolInfo->Interface;
        return EFI_SUCCESS;
      }
    }
  }
  return EFI_NOT_FOUND;
}

EFI_STATUS
EFIAPI
InstallProtocolInterface (
  IN OUT EFI_HANDLE               *Handle,
  IN     EFI_GUID                 *Protocol,
  IN     EFI_INTERFACE_TYPE       InterfaceType,
  IN     VOID                     *Interface
  )
{
  PROTOCOL_INFO  *HandleProtocols;
  UINT32 ProtocolsInHandle;

  if (Handle == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  if (*Handle == NULL) {
    if (mFirstFreeHandle >= MAX_HANDLES) {
      return EFI_OUT_OF_RESOURCES;
    }
    *Handle = (EFI_HANDLE) &mHandleDatabase[mFirstFreeHandle];
    ZeroMem (*Handle, sizeof (HANDLE_INFO));
    mFirstFreeHandle++;
  }

  HandleProtocols = ((HANDLE_INFO*)(*Handle))->Protocols;
  ProtocolsInHandle = ((HANDLE_INFO*)(*Handle))->NoOfProtocols;

  if (ProtocolsInHandle >= MAX_PROTOCOLS_PER_HANDLE) {
    return EFI_OUT_OF_RESOURCES;
  }

  CopyMem (&HandleProtocols[ProtocolsInHandle].Guid, Protocol, sizeof (EFI_GUID));
  HandleProtocols[ProtocolsInHandle].Interface = Interface;
  ((HANDLE_INFO*)(*Handle))->NoOfProtocols++;
  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
CreateEvent (
  IN  UINT32                       Type,
  IN  EFI_TPL                      NotifyTpl,
  IN  EFI_EVENT_NOTIFY             NotifyFunction,
  IN  VOID                         *NotifyContext,
  OUT EFI_EVENT                    *Event
  )
{
    return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
SetTimer (
  IN  EFI_EVENT                Event,
  IN  EFI_TIMER_DELAY          Type,
  IN  UINT64                   TriggerTime
  )
{
  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
CloseProtocol (
  IN EFI_HANDLE               Handle,
  IN EFI_GUID                 *Protocol,
  IN EFI_HANDLE               AgentHandle,
  IN EFI_HANDLE               ControllerHandle
  )
{
  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
CloseEvent (
  IN EFI_EVENT                Event
  )
{
  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
InstallMultipleProtocolInterface (
  IN OUT EFI_HANDLE           *Handle,
  ...
  )
{
  VA_LIST                   Args;
  UINTN                     Index;
  EFI_STATUS                Status;
  EFI_GUID                  *Protocol;
  VOID                      *Interface;

  if (Handle == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  VA_START (Args, Handle);
  for (Index = 0, Status = EFI_SUCCESS; !EFI_ERROR (Status); Index++) {
    //
    // If protocol is NULL, then it's the end of the list
    //
    Protocol = VA_ARG (Args, EFI_GUID *);
    if (Protocol == NULL) {
      break;
    }

    Interface = VA_ARG (Args, VOID *);
    Status = InstallProtocolInterface (Handle, Protocol, EFI_NATIVE_INTERFACE, Interface);
  }
  VA_END (Args);

  return Status;
}

VOID
InitializeBootServices (
    VOID
  )
{
  gBS = AllocateZeroPool (sizeof (EFI_BOOT_SERVICES));
  gBS->Stall = Stall;
  gBS->RaiseTPL = RaiseTpl;
  gBS->RestoreTPL = RestoreTpl;
  gBS->OpenProtocol = OpenProtocol;
  gBS->LocateProtocol = LocateProtocol;
  gBS->InstallProtocolInterface = InstallProtocolInterface;
  gBS->SetTimer = SetTimer;
  gBS->CreateEvent = CreateEvent;
  gBS->CloseProtocol = CloseProtocol;
  gBS->CloseEvent = CloseEvent;
  gBS->InstallMultipleProtocolInterfaces = InstallMultipleProtocolInterface;
}
