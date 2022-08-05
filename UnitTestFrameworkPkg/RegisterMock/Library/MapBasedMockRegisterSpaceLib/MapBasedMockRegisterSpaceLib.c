#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <MapBasedRegisterSpaceLib.h>

EFI_STATUS
MapBasedRegisterMockRead (
  IN REGISTER_SPACE_MOCK  *RegisterSpace,
  IN UINT64          Address,
  IN UINT32          Size,
  OUT UINT64         *Value
  )
{
  UINT32  Index;
  MAP_BASED_REGISTER_SPACE  *SimpleRegisterSpace;

  SimpleRegisterSpace = (MAP_BASED_REGISTER_SPACE*) RegisterSpace;

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
MapBasedRegisterMockWrite (
  IN REGISTER_SPACE_MOCK  *RegisterSpace,
  IN UINT64          Address,
  IN UINT32          Size,
  IN UINT64          Value
  )
{
  UINT32  Index;
  MAP_BASED_REGISTER_SPACE  *SimpleRegisterSpace;

  SimpleRegisterSpace = (MAP_BASED_REGISTER_SPACE*) RegisterSpace;

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

EFI_STATUS
MapBasedRegisterSpaceCreate (
  IN CHAR16        *RegisterSpaceDescription,
  IN REGISTER_MAP  *RegisterMapTemplate,
  IN UINT32        RegisterMapSize,
  IN REGISTER_PRE_WRITE_CALLBACK  PreWrite,
  IN VOID                         *PreWriteContext,
  IN REGISTER_POST_READ_CALLBACK  PostRead,
  IN VOID                         *PostReadContext,
  OUT MAP_BASED_REGISTER_SPACE       **SimpleRegisterSpace
)
{
  *SimpleRegisterSpace = AllocateZeroPool (sizeof (MAP_BASED_REGISTER_SPACE));
  (*SimpleRegisterSpace)->RegisterSpace.Name = RegisterSpaceDescription;
  (*SimpleRegisterSpace)->RegisterSpace.Read = MapBasedRegisterMockRead;
  (*SimpleRegisterSpace)->RegisterSpace.Write = MapBasedRegisterMockWrite;
  (*SimpleRegisterSpace)->PostRead = PostRead;
  (*SimpleRegisterSpace)->PostReadContext = PostReadContext;
  (*SimpleRegisterSpace)->PreWrite = PreWrite;
  (*SimpleRegisterSpace)->PreWriteContext = PreWriteContext;
  (*SimpleRegisterSpace)->MapSize = RegisterMapSize;
  (*SimpleRegisterSpace)->Map = AllocateCopyPool (RegisterMapSize * sizeof (REGISTER_MAP), RegisterMapTemplate);

  return EFI_SUCCESS;
}