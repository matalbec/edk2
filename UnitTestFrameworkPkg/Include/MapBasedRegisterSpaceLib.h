#ifndef _MAP_BASED_REGISTER_SPACE_LIB_H_
#define _MAP_BASED_REGISTER_SPACE_LIB_H_

#include <Base.h>
#include <RegisterSpaceMock.h>

typedef struct _MAP_BASED_REGISTER_SPACE MAP_BASED_REGISTER_SPACE;

typedef struct {
  UINT64                       Offset;
  CHAR16*                      Name;
  UINT32                       SizeInBytes;
  UINT64                       Value;
} REGISTER_MAP;

typedef
VOID
(*REGISTER_POST_READ_CALLBACK) (
  IN MAP_BASED_REGISTER_SPACE  *RegisterSpace,
  IN UINT64        Address,
  IN UINT32        Size,
  IN OUT UINT64    *Value,
  IN VOID          *Context
  );

typedef
VOID
(*REGISTER_PRE_WRITE_CALLBACK) (
  IN MAP_BASED_REGISTER_SPACE  *RegisterSpace,
  IN UINT64        Address,
  IN UINT32        Size,
  IN OUT UINT64    *Value,
  IN VOID          *Context
  );

struct _MAP_BASED_REGISTER_SPACE {
  REGISTER_SPACE_MOCK          RegisterSpace;
  REGISTER_POST_READ_CALLBACK  PostRead;
  VOID                         *PostReadContext;
  REGISTER_PRE_WRITE_CALLBACK  PreWrite;
  VOID                         *PreWriteContext;
  UINTN                        MapSize;
  REGISTER_MAP                 *Map;
};

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
);

#endif