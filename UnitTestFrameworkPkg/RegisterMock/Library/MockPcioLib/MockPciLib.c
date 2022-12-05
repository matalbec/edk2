#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <MockPciLib.h>


EFIAPI
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

EFIAPI
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

EFIAPI
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

EFIAPI
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
  UINT32  *Uint32Buffer;
  UINT32  Index;

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
      PciDev->Bar[BarIndex]->Write (PciDev->Bar[BarIndex], Offset, 4, Uint32Buffer[Index]);
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
      return EFI_UNSUPPORTED;
  }

  return PciDev->Bar[BarIndex]->Write (PciDev->Bar[BarIndex], Offset, Size, *(UINT64*)Buffer);
}

EFIAPI
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

EFIAPI
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

EFIAPI
EFI_STATUS
MockPciIoConfigRead (
  IN EFI_PCI_IO_PROTOCOL              *This,
  IN     EFI_PCI_IO_PROTOCOL_WIDTH    Width,
  IN     UINT32                       Offset,
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

  if (PciDev->ConfigSpace == NULL) {
    DEBUG ((DEBUG_INFO, "NULL Bar\n"));
    return EFI_UNSUPPORTED;
  }

  if (Width == EfiPciIoWidthFifoUint32) {
    Uint32Buffer = (UINT32*) Buffer;
    for (Index = 0; Index < Count; Index++) {
      PciDev->ConfigSpace->Read (PciDev->ConfigSpace, Offset, 4, &Val);
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

  return PciDev->ConfigSpace->Read (PciDev->ConfigSpace, Offset, Size, (UINT64*)Buffer);
}

EFIAPI
EFI_STATUS
MockPciIoConfigWrite (
  IN EFI_PCI_IO_PROTOCOL              *This,
  IN     EFI_PCI_IO_PROTOCOL_WIDTH    Width,
  IN     UINT32                       Offset,
  IN     UINTN                        Count,
  IN OUT VOID                         *Buffer
  )
{
  MOCK_PCI_IO  *PciIo;
  MOCK_PCI_DEVICE  *PciDev;
  UINT32  Size;

  PciIo = (MOCK_PCI_IO*) This;
  PciDev = PciIo->MockPci;

  if (PciDev->ConfigSpace == NULL) {
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

  return PciDev->ConfigSpace->Write (PciDev->ConfigSpace, Offset, Size, *(UINT64*)Buffer);
}

EFIAPI
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

EFIAPI
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

EFIAPI
EFI_STATUS
MockPciIoUnmap (
  IN EFI_PCI_IO_PROTOCOL           *This,
  IN  VOID                         *Mapping
  )
{
  return EFI_UNSUPPORTED;
}

EFIAPI
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

EFIAPI
EFI_STATUS
MockPciIoFreeBuffer (
  IN EFI_PCI_IO_PROTOCOL           *This,
  IN  UINTN                        Pages,
  IN  VOID                         *HostAddress
  )
{
  return EFI_UNSUPPORTED;
}

EFIAPI
EFI_STATUS
MockPciIoFlush (
  IN EFI_PCI_IO_PROTOCOL  *This
  )
{
  return EFI_UNSUPPORTED;
}

EFIAPI
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

EFIAPI
EFI_STATUS
MockPciIoProtocolAttributes (
  IN EFI_PCI_IO_PROTOCOL                       *This,
  IN  EFI_PCI_IO_PROTOCOL_ATTRIBUTE_OPERATION  Operation,
  IN  UINT64                                   Attributes,
  OUT UINT64                                   *Result OPTIONAL
  )
{
  return EFI_SUCCESS;
}

EFIAPI
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

EFIAPI
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
MockPciIoCreate (
  IN MOCK_PCI_DEVICE  *MockPci,
  OUT EFI_PCI_IO_PROTOCOL  **PciIo
  )
{
  MOCK_PCI_IO  *MockPciIo;

  MockPciIo = AllocateZeroPool (sizeof (MOCK_PCI_IO));

  MockPciIo->MockPci = MockPci;

  MockPciIo->PciIo.Pci.Read = MockPciIoConfigRead;
  MockPciIo->PciIo.Pci.Write = MockPciIoConfigWrite;
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

EFI_STATUS
MockPciDeviceInitialize (
  IN REGISTER_SPACE_MOCK       *ConfigSpace,
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
  IN REGISTER_SPACE_MOCK   *BarRegisterSpace,
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
