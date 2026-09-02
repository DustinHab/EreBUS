/*
 * efi.h -- the parts of the UEFI interface the loader needs.
 *
 * Written by hand rather than pulling in gnu-efi: the loader is part of
 * the system, not our code living inside somebody else's library. For a
 * security-focused system that also keeps the trusted base small.
 *
 * The order of the function pointers in these tables is ABI, not taste.
 * It follows UEFI specification 2.10 exactly. Entries we do not use are
 * declared as void* so that the offsets still line up.
 */
#ifndef EREBUS_EFI_H
#define EREBUS_EFI_H

typedef unsigned char      UINT8;
typedef unsigned short     UINT16;
typedef unsigned int       UINT32;
typedef unsigned long long UINT64;
typedef signed short       INT16;
typedef signed long long   INT64;
typedef UINT64             UINTN;
typedef INT64              INTN;
typedef UINT16             CHAR16;
typedef unsigned char      BOOLEAN;
typedef void               VOID;
typedef VOID              *EFI_HANDLE;
typedef VOID              *EFI_EVENT;
typedef UINTN              EFI_STATUS;
typedef UINT64             EFI_PHYSICAL_ADDRESS;
typedef UINT64             EFI_VIRTUAL_ADDRESS;
typedef UINT64             EFI_TPL;

#define EFIAPI __attribute__((ms_abi))
#define NULL ((void *)0)
#define TRUE  1
#define FALSE 0

/* Status codes: the top bit marks an error. */
#define EFI_ERROR_BIT           0x8000000000000000ULL
#define EFI_SUCCESS             0
#define EFI_LOAD_ERROR          (EFI_ERROR_BIT | 1)
#define EFI_INVALID_PARAMETER   (EFI_ERROR_BIT | 2)
#define EFI_UNSUPPORTED         (EFI_ERROR_BIT | 3)
#define EFI_BAD_BUFFER_SIZE     (EFI_ERROR_BIT | 4)
#define EFI_BUFFER_TOO_SMALL    (EFI_ERROR_BIT | 5)
#define EFI_NOT_READY           (EFI_ERROR_BIT | 6)
#define EFI_DEVICE_ERROR        (EFI_ERROR_BIT | 7)
#define EFI_OUT_OF_RESOURCES    (EFI_ERROR_BIT | 9)
#define EFI_NOT_FOUND           (EFI_ERROR_BIT | 14)
#define EFI_ERROR(s) (((UINTN)(s) & EFI_ERROR_BIT) != 0)

typedef struct {
    UINT32 Data1;
    UINT16 Data2;
    UINT16 Data3;
    UINT8  Data4[8];
} EFI_GUID;

typedef struct {
    UINT64 Signature;
    UINT32 Revision;
    UINT32 HeaderSize;
    UINT32 CRC32;
    UINT32 Reserved;
} EFI_TABLE_HEADER;

/* ------------------------------------------------------------------ */
/* Memory                                                              */
/* ------------------------------------------------------------------ */

typedef enum {
    AllocateAnyPages,
    AllocateMaxAddress,
    AllocateAddress
} EFI_ALLOCATE_TYPE;

typedef enum {
    EfiReservedMemoryType,
    EfiLoaderCode,
    EfiLoaderData,
    EfiBootServicesCode,
    EfiBootServicesData,
    EfiRuntimeServicesCode,
    EfiRuntimeServicesData,
    EfiConventionalMemory,
    EfiUnusableMemory,
    EfiACPIReclaimMemory,
    EfiACPIMemoryNVS,
    EfiMemoryMappedIO,
    EfiMemoryMappedIOPortSpace,
    EfiPalCode,
    EfiPersistentMemory,
    EfiUnacceptedMemoryType,
    EfiMaxMemoryType
} EFI_MEMORY_TYPE;

/* Careful: the firmware is allowed to hand back a LARGER descriptor
 * than declared here. When walking the map never use sizeof() -- always
 * the DescriptorSize returned by GetMemoryMap. */
typedef struct {
    UINT32               Type;
    UINT32               Pad;
    EFI_PHYSICAL_ADDRESS PhysicalStart;
    EFI_VIRTUAL_ADDRESS  VirtualStart;
    UINT64               NumberOfPages;
    UINT64               Attribute;
} EFI_MEMORY_DESCRIPTOR;

/* ------------------------------------------------------------------ */
/* Text output                                                         */
/* ------------------------------------------------------------------ */

struct _EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL;

typedef struct _EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL {
    VOID       *Reset;
    EFI_STATUS (EFIAPI *OutputString)(struct _EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This,
                                      CHAR16 *String);
    VOID       *TestString;
    VOID       *QueryMode;
    EFI_STATUS (EFIAPI *SetMode)(struct _EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This,
                                 UINTN ModeNumber);
    EFI_STATUS (EFIAPI *SetAttribute)(struct _EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This,
                                      UINTN Attribute);
    EFI_STATUS (EFIAPI *ClearScreen)(struct _EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This);
    VOID       *SetCursorPosition;
    VOID       *EnableCursor;
    VOID       *Mode;
} EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL;

typedef struct {
    UINT16 ScanCode;
    CHAR16 UnicodeChar;
} EFI_INPUT_KEY;

typedef struct _EFI_SIMPLE_TEXT_INPUT_PROTOCOL {
    VOID       *Reset;
    EFI_STATUS (EFIAPI *ReadKeyStroke)(struct _EFI_SIMPLE_TEXT_INPUT_PROTOCOL *This,
                                       EFI_INPUT_KEY *Key);
    EFI_EVENT   WaitForKey;
} EFI_SIMPLE_TEXT_INPUT_PROTOCOL;

/* ------------------------------------------------------------------ */
/* Graphics Output Protocol                                            */
/* ------------------------------------------------------------------ */

#define EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID \
    { 0x9042a9de, 0x23dc, 0x4a38, { 0x96, 0xfb, 0x7a, 0xde, 0xd0, 0x80, 0x51, 0x6a } }

typedef enum {
    PixelRedGreenBlueReserved8BitPerColor,
    PixelBlueGreenRedReserved8BitPerColor,
    PixelBitMask,
    PixelBltOnly,
    PixelFormatMax
} EFI_GRAPHICS_PIXEL_FORMAT;

typedef struct {
    UINT32 RedMask;
    UINT32 GreenMask;
    UINT32 BlueMask;
    UINT32 ReservedMask;
} EFI_PIXEL_BITMASK;

typedef struct {
    UINT32                    Version;
    UINT32                    HorizontalResolution;
    UINT32                    VerticalResolution;
    EFI_GRAPHICS_PIXEL_FORMAT PixelFormat;
    EFI_PIXEL_BITMASK         PixelInformation;
    UINT32                    PixelsPerScanLine;
} EFI_GRAPHICS_OUTPUT_MODE_INFORMATION;

typedef struct {
    UINT32                                MaxMode;
    UINT32                                Mode;
    EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *Info;
    UINTN                                 SizeOfInfo;
    EFI_PHYSICAL_ADDRESS                  FrameBufferBase;
    UINTN                                 FrameBufferSize;
} EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE;

typedef struct _EFI_GRAPHICS_OUTPUT_PROTOCOL {
    EFI_STATUS (EFIAPI *QueryMode)(struct _EFI_GRAPHICS_OUTPUT_PROTOCOL *This,
                                   UINT32 ModeNumber, UINTN *SizeOfInfo,
                                   EFI_GRAPHICS_OUTPUT_MODE_INFORMATION **Info);
    EFI_STATUS (EFIAPI *SetMode)(struct _EFI_GRAPHICS_OUTPUT_PROTOCOL *This,
                                 UINT32 ModeNumber);
    VOID       *Blt;
    EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE *Mode;
} EFI_GRAPHICS_OUTPUT_PROTOCOL;

/* ------------------------------------------------------------------ */
/* Loaded image and file system                                        */
/* ------------------------------------------------------------------ */

#define EFI_LOADED_IMAGE_PROTOCOL_GUID \
    { 0x5b1b31a1, 0x9562, 0x11d2, { 0x8e, 0x3f, 0x00, 0xa0, 0xc9, 0x69, 0x72, 0x3b } }

typedef struct {
    UINT32       Revision;
    EFI_HANDLE   ParentHandle;
    VOID        *SystemTable;
    EFI_HANDLE   DeviceHandle;   /* <- where our file system comes from */
    VOID        *FilePath;
    VOID        *Reserved;
    UINT32       LoadOptionsSize;
    VOID        *LoadOptions;
    VOID        *ImageBase;
    UINT64       ImageSize;
    EFI_MEMORY_TYPE ImageCodeType;
    EFI_MEMORY_TYPE ImageDataType;
    VOID        *Unload;
} EFI_LOADED_IMAGE_PROTOCOL;

#define EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID \
    { 0x964e5b22, 0x6459, 0x11d2, { 0x8e, 0x39, 0x00, 0xa0, 0xc9, 0x69, 0x72, 0x3b } }

#define EFI_FILE_INFO_GUID \
    { 0x09576e92, 0x6d3f, 0x11d2, { 0x8e, 0x39, 0x00, 0xa0, 0xc9, 0x69, 0x72, 0x3b } }

#define EFI_FILE_MODE_READ   0x0000000000000001ULL
#define EFI_FILE_MODE_WRITE  0x0000000000000002ULL
#define EFI_FILE_MODE_CREATE 0x8000000000000000ULL

struct _EFI_FILE_PROTOCOL;

typedef struct _EFI_FILE_PROTOCOL {
    UINT64     Revision;
    EFI_STATUS (EFIAPI *Open)(struct _EFI_FILE_PROTOCOL *This,
                              struct _EFI_FILE_PROTOCOL **NewHandle,
                              CHAR16 *FileName, UINT64 OpenMode,
                              UINT64 Attributes);
    EFI_STATUS (EFIAPI *Close)(struct _EFI_FILE_PROTOCOL *This);
    EFI_STATUS (EFIAPI *Delete)(struct _EFI_FILE_PROTOCOL *This);
    EFI_STATUS (EFIAPI *Read)(struct _EFI_FILE_PROTOCOL *This,
                              UINTN *BufferSize, VOID *Buffer);
    EFI_STATUS (EFIAPI *Write)(struct _EFI_FILE_PROTOCOL *This,
                               UINTN *BufferSize, VOID *Buffer);
    VOID       *GetPosition;
    EFI_STATUS (EFIAPI *SetPosition)(struct _EFI_FILE_PROTOCOL *This,
                                     UINT64 Position);
    EFI_STATUS (EFIAPI *GetInfo)(struct _EFI_FILE_PROTOCOL *This,
                                 EFI_GUID *InformationType,
                                 UINTN *BufferSize, VOID *Buffer);
    VOID       *SetInfo;
    VOID       *Flush;
} EFI_FILE_PROTOCOL;

typedef struct _EFI_SIMPLE_FILE_SYSTEM_PROTOCOL {
    UINT64     Revision;
    EFI_STATUS (EFIAPI *OpenVolume)(struct _EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *This,
                                    EFI_FILE_PROTOCOL **Root);
} EFI_SIMPLE_FILE_SYSTEM_PROTOCOL;

typedef struct {
    UINT16 Year;
    UINT8  Month, Day, Hour, Minute, Second, Pad1;
    UINT32 Nanosecond;
    INT16  TimeZone;
    UINT8  Daylight, Pad2;
} EFI_TIME;

typedef struct {
    UINT64   Size;
    UINT64   FileSize;
    UINT64   PhysicalSize;
    EFI_TIME CreateTime;
    EFI_TIME LastAccessTime;
    EFI_TIME ModificationTime;
    UINT64   Attribute;
    CHAR16   FileName[1];
} EFI_FILE_INFO;

/* ------------------------------------------------------------------ */
/* Boot services -- the order of these fields is ABI                   */
/* ------------------------------------------------------------------ */

typedef struct {
    EFI_TABLE_HEADER Hdr;

    /* Task priority */
    VOID *RaiseTPL;
    VOID *RestoreTPL;

    /* Memory */
    EFI_STATUS (EFIAPI *AllocatePages)(EFI_ALLOCATE_TYPE Type,
                                       EFI_MEMORY_TYPE MemoryType,
                                       UINTN Pages,
                                       EFI_PHYSICAL_ADDRESS *Memory);
    EFI_STATUS (EFIAPI *FreePages)(EFI_PHYSICAL_ADDRESS Memory, UINTN Pages);
    EFI_STATUS (EFIAPI *GetMemoryMap)(UINTN *MemoryMapSize,
                                      EFI_MEMORY_DESCRIPTOR *MemoryMap,
                                      UINTN *MapKey,
                                      UINTN *DescriptorSize,
                                      UINT32 *DescriptorVersion);
    EFI_STATUS (EFIAPI *AllocatePool)(EFI_MEMORY_TYPE PoolType, UINTN Size,
                                      VOID **Buffer);
    EFI_STATUS (EFIAPI *FreePool)(VOID *Buffer);

    /* Events and timers */
    VOID *CreateEvent;
    VOID *SetTimer;
    VOID *WaitForEvent;
    VOID *SignalEvent;
    VOID *CloseEvent;
    VOID *CheckEvent;

    /* Protocol handling */
    VOID *InstallProtocolInterface;
    VOID *ReinstallProtocolInterface;
    VOID *UninstallProtocolInterface;
    EFI_STATUS (EFIAPI *HandleProtocol)(EFI_HANDLE Handle, EFI_GUID *Protocol,
                                        VOID **Interface);
    VOID *Reserved;
    VOID *RegisterProtocolNotify;
    VOID *LocateHandle;
    VOID *LocateDevicePath;
    VOID *InstallConfigurationTable;

    /* Images */
    VOID *LoadImage;
    VOID *StartImage;
    VOID *Exit;
    VOID *UnloadImage;
    EFI_STATUS (EFIAPI *ExitBootServices)(EFI_HANDLE ImageHandle, UINTN MapKey);

    /* Miscellaneous */
    VOID *GetNextMonotonicCount;
    EFI_STATUS (EFIAPI *Stall)(UINTN Microseconds);
    EFI_STATUS (EFIAPI *SetWatchdogTimer)(UINTN Timeout, UINT64 WatchdogCode,
                                          UINTN DataSize, CHAR16 *WatchdogData);

    /* Driver support */
    VOID *ConnectController;
    VOID *DisconnectController;

    /* Open and close protocol */
    VOID *OpenProtocol;
    VOID *CloseProtocol;
    VOID *OpenProtocolInformation;

    /* Library */
    VOID *ProtocolsPerHandle;
    VOID *LocateHandleBuffer;
    EFI_STATUS (EFIAPI *LocateProtocol)(EFI_GUID *Protocol, VOID *Registration,
                                        VOID **Interface);
    VOID *InstallMultipleProtocolInterfaces;
    VOID *UninstallMultipleProtocolInterfaces;

    /* 32-bit CRC */
    VOID *CalculateCrc32;

    /* Miscellaneous, part two */
    VOID (EFIAPI *CopyMem)(VOID *Destination, VOID *Source, UINTN Length);
    VOID (EFIAPI *SetMem)(VOID *Buffer, UINTN Size, UINT8 Value);
    VOID *CreateEventEx;
} EFI_BOOT_SERVICES;

typedef struct {
    EFI_GUID VendorGuid;
    VOID    *VendorTable;
} EFI_CONFIGURATION_TABLE;

typedef struct {
    EFI_TABLE_HEADER                 Hdr;
    CHAR16                          *FirmwareVendor;
    UINT32                           FirmwareRevision;
    EFI_HANDLE                       ConsoleInHandle;
    EFI_SIMPLE_TEXT_INPUT_PROTOCOL  *ConIn;
    EFI_HANDLE                       ConsoleOutHandle;
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *ConOut;
    EFI_HANDLE                       StandardErrorHandle;
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *StdErr;
    VOID                            *RuntimeServices;
    EFI_BOOT_SERVICES               *BootServices;
    UINTN                            NumberOfTableEntries;
    EFI_CONFIGURATION_TABLE         *ConfigurationTable;
} EFI_SYSTEM_TABLE;

/* ACPI root pointer, found in the firmware configuration table. */
#define EFI_ACPI_20_TABLE_GUID \
    { 0x8868e871, 0xe4f1, 0x11d3, { 0xbc, 0x22, 0x00, 0x80, 0xc7, 0x3c, 0x88, 0x81 } }
#define EFI_ACPI_10_TABLE_GUID \
    { 0xeb9d2d30, 0x2d88, 0x11d3, { 0x9a, 0x16, 0x00, 0x90, 0x27, 0x3f, 0xc1, 0x4d } }

#endif /* EREBUS_EFI_H */
