#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <assert.h>
#include <stdarg.h>

#include <glib.h>
#include <qemu/compiler.h>
#include <libqos/libqos-pc.h>
#include <libqos/pci-pc.h>

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
QmpPciIoFromQPciDev (
  IN QPCIDevice *QPciDevice,
  OUT 
)

EFI_STATUS
QmpFindPciDeviceByClassCode (
  IN QOSState     *Qs,
  IN UINT8        BaseClassCode,
  IN UINT8        SubClassCode,
  IN UINT8        ProgrammingInterface,
  OUT QPCIDevice  **QPciDevice
  )
{
  UINTN      Device;
  UINTN      Function;
  QPCIBus    *QPciBus;
  QPCIDevice *Dev;
  
  QPciBus = qpci_new_pc (Qs->qts, NULL);
  if (QPciBus == NULL) {
    DEBUG (DEBUG_ERROR, "%a failed to get pci bus\n", __FUNCTION__);
    return EFI_DEVICE_ERROR;
  }

  QPciDevice = NULL;
  for (Device = 0; Device < 32; Device++) {
    for (Function = 0; Function < 8; Function++) {
      Dev = qpci_device_find (QPciBus, QPCI_DEVFN(Device, Function));
      if (Dev == NULL) {
        continue;
      }
      if (qpci_config_readb(Dev, 0xA) == ProgrammingInterface &&
          qpci_config_readb(Dev, 0xB) == SubClassCode &&
          qpci_config_readb(Dev, 0xC) == BaseClassCode) {
        QPciDevice = Dev;
        break;
      } else {
        g_free (Dev);
      }
    }
  }

  g_free (QPciBus);
  return (QPciDevice == NULL) ? EFI_NOT_FOUND : EFI_SUCCESS;
}

EFI_STATUS
QmpBuildPciIoByClassCode (
  IN QOSState             *Qs,
  IN UINT8                 BaseClassCode,
  IN UINT8                 SubClassCode,
  IN UINT8                 ProgrammingInterface,
  OUT EFI_PCI_IO_PROTOCOL  **PciIo
  )
{
  MOCK_PCI_DEVICE      *MockPciDevice;
  EFI_PCI_IO_PROTOCOL  *MockPciIo;
  REGISTER_SPACE_MOCK  *PciConfigRegisterSpace;
  REGISTER_SPACE_MOCK  *BarRegisterSpace;
  QPCIDevice *QPciDev;

  QmpFindPciDeviceByClassCode (Qs, BaseClassCode, SubClassCode, ProgrammingInterface, &QPciDev);



  QemuRegisterSpaceInit (L"SD PCI config", QemuPciCfg, 0, Qs, &PciConfigRegisterSpace);
  QemuRegisterSpaceInit (L"SD MEM", QemuBar, 0, Qs, &BarRegisterSpace);

  MockPciDeviceInitialize (&SdControllerPciSpace, &MockPciDevice);

  MockPciDeviceRegisterBar (MockPciDevice, (REGISTER_SPACE_MOCK*) BarRegisterSpace, 0);

  MockPciIoCreate (MockPciDevice, &MockPciIo);

  *PciIo = MockPciIo;

  return EFI_SUCCESS;
}

EFI_STATUS
QmpInitSimAndCreatePciIoByClassCode (
  IN CONST CHAR           *Cli,
  IN UINT8                 BaseClassCode,
  IN UINT8                 SubClassCode,
  IN UINT8                 ProgrammingInterface, 
  OUT EFI_PCI_IO_PROTOCOL  **PciIo
  )
{
  QOSState *qs;

  qs = qtest_pc_boot (Cli);
  QmpBuildPciIoByClassCode (qs, BaseClassCode, SubClassCode, ProgrammingInterface, PciIo);

  return EFI_SUCCESS;
}