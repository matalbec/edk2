## @file
# MdePkg DSC file used to build host-based unit tests.
#
# Copyright (c) 2022, Intel Corporation. All rights reserved.<BR>
# SPDX-License-Identifier: BSD-2-Clause-Patent
#
##

[Defines]
  PLATFORM_NAME           = SdMmcPciHcUnitTest
  PLATFORM_GUID           = 50652B4C-88CB-4481-96E8-37F2D0034440
  PLATFORM_VERSION        = 0.1
  DSC_SPECIFICATION       = 0x00010005
  OUTPUT_DIRECTORY        = Build/MdeModulePkg/HostTest
  SUPPORTED_ARCHITECTURES = IA32|X64
  BUILD_TARGETS           = NOOPT
  SKUID_IDENTIFIER        = DEFAULT

!include UnitTestFrameworkPkg/UnitTestFrameworkPkgHost.dsc.inc

[LibraryClasses]
  PcdLib|MdePkg/Library/BasePcdLibNull/BasePcdLibNull.inf
  DevicePathLib|MdePkg/Library/UefiDevicePathLib/UefiDevicePathLib.inf
  UefiLib|MdePkg/Library/UefiLib/UefiLib.inf
  UefiRuntimeServicesTableLib|MdePkg/Library/UefiRuntimeServicesTableLib/UefiRuntimeServicesTableLib.inf
  MockPciLib|UnitTestFrameworkPkg/RegisterMock/Library/MockPcioLib/MockPciLib.inf
  MapBasedMockRegisterSpaceLib|UnitTestFrameworkPkg/RegisterMock/Library/MapBasedMockRegisterSpaceLib/MapBasedMockRegisterSpaceLib.inf

[Components]
  #MdeModulePkg/Bus/Pci/SdMmcPciHcDxe/UnitTest/SdMmcPciHcHostTest.inf
  MdeModulePkg/Bus/Pci/SdMmcPciHcDxe/UnitTest/SdMmcPciHostTestVpi.inf
