// SPDX-License-Identifier: GPL-2.0-or-later
/* uefi.h - lo mínimo de la especificación UEFI que usa el cargador de SaguiOS (D264).
 *
 * Escrito a mano a partir de la especificación pública (UEFI 2.x): solo los tipos,
 * tablas, protocolos y GUID que hacen falta, con los campos que no se usan como
 * punteros genéricos para que el orden y el tamaño de cada tabla sean los de la
 * especificación. Sin gnu-efi ni cabeceras de EDK2.
 *
 * Vale para las dos arquitecturas: en x64 los servicios del firmware usan la
 * convención de llamada de Microsoft (EFIAPI = ms_abi); en IA32, la cdecl normal.
 * UINTN es del tamaño de un puntero (8 o 4 bytes). */
#ifndef SAGUI_UEFI_H
#define SAGUI_UEFI_H

typedef unsigned char      UINT8;
typedef unsigned short     UINT16;
typedef unsigned int       UINT32;
typedef unsigned long long UINT64;
typedef signed int         INT32;
typedef unsigned long      UINTN;     /* gcc: long tiene el tamaño del puntero en x64 y en i386 */
typedef UINT8              BOOLEAN;
typedef UINT16             CHAR16;    /* compilado con -fshort-wchar: L"..." da CHAR16 */
typedef void              *EFI_HANDLE;
typedef void              *EFI_EVENT;
typedef UINTN              EFI_STATUS;
typedef UINT64             EFI_PHYSICAL_ADDRESS;

#if defined(__x86_64__)
#define EFIAPI __attribute__((ms_abi))
#else
#define EFIAPI
#endif

/* Códigos de estado: los errores llevan el bit más alto. */
#define EFI_ERRO_BIT          ((UINTN)1 << (sizeof(UINTN) * 8 - 1))
#define EFI_SUCCESS           0
#define EFI_ERROR(s)          (((UINTN)(s) & EFI_ERRO_BIT) != 0)
#define EFI_INVALID_PARAMETER (EFI_ERRO_BIT | 2)
#define EFI_BUFFER_TOO_SMALL  (EFI_ERRO_BIT | 5)
#define EFI_NOT_READY         (EFI_ERRO_BIT | 6)
#define EFI_NOT_FOUND         (EFI_ERRO_BIT | 14)

typedef struct {
    UINT32 d1;
    UINT16 d2, d3;
    UINT8  d4[8];
} EFI_GUID;

typedef struct {
    UINT64 Signature;
    UINT32 Revision;
    UINT32 HeaderSize;
    UINT32 CRC32;
    UINT32 Reserved;
} EFI_TABLE_HEADER;

/* ------------------------------------------------------------ texto */

typedef struct {
    UINT16 ScanCode;
    CHAR16 UnicodeChar;
} EFI_INPUT_KEY;

typedef struct EFI_SIMPLE_TEXT_INPUT_PROTOCOL {
    EFI_STATUS (EFIAPI *Reset)(struct EFI_SIMPLE_TEXT_INPUT_PROTOCOL *, BOOLEAN);
    EFI_STATUS (EFIAPI *ReadKeyStroke)(struct EFI_SIMPLE_TEXT_INPUT_PROTOCOL *, EFI_INPUT_KEY *);
    EFI_EVENT WaitForKey;
} EFI_SIMPLE_TEXT_INPUT_PROTOCOL;

/* Simple Text Input Ex: da el estado de Ctrl/Alt/Mayús con la tecla. */
typedef struct {
    UINT32 KeyShiftState;
    UINT8  KeyToggleState;
} EFI_KEY_STATE;

typedef struct {
    EFI_INPUT_KEY Key;
    EFI_KEY_STATE KeyState;
} EFI_KEY_DATA;

#define EFI_SHIFT_STATE_VALID     0x80000000u
#define EFI_RIGHT_SHIFT_PRESSED   0x00000001u
#define EFI_LEFT_SHIFT_PRESSED    0x00000002u
#define EFI_RIGHT_CONTROL_PRESSED 0x00000004u
#define EFI_LEFT_CONTROL_PRESSED  0x00000008u

typedef struct EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL {
    void *Reset;
    EFI_STATUS (EFIAPI *ReadKeyStrokeEx)(struct EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL *, EFI_KEY_DATA *);
    EFI_EVENT WaitForKeyEx;
    void *SetState;
    void *RegisterKeyNotify;
    void *UnregisterKeyNotify;
} EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL;

typedef struct EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL {
    void *Reset;
    EFI_STATUS (EFIAPI *OutputString)(struct EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *, CHAR16 *);
    void *TestString;
    void *QueryMode;
    void *SetMode;
    EFI_STATUS (EFIAPI *SetAttribute)(struct EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *, UINTN);
    EFI_STATUS (EFIAPI *ClearScreen)(struct EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *);
    void *SetCursorPosition;
    void *EnableCursor;
    void *Mode;
} EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL;

/* ------------------------------------------------------------ memoria */

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
    EfiMaxMemoryType
} EFI_MEMORY_TYPE;

typedef enum {
    AllocateAnyPages,
    AllocateMaxAddress,
    AllocateAddress
} EFI_ALLOCATE_TYPE;

/* El tamaño de cada descriptor lo da GetMemoryMap (puede ser mayor que este). */
typedef struct {
    UINT32 Type;
    UINT32 Pad;
    UINT64 PhysicalStart;
    UINT64 VirtualStart;
    UINT64 NumberOfPages;
    UINT64 Attribute;
} EFI_MEMORY_DESCRIPTOR;

/* ------------------------------------------------------------ servicios de arranque */

typedef enum {
    AllHandles,
    ByRegisterNotify,
    ByProtocol
} EFI_LOCATE_SEARCH_TYPE;

typedef struct EFI_DEVICE_PATH_PROTOCOL {
    UINT8 Type;
    UINT8 SubType;
    UINT8 Length[2];
} EFI_DEVICE_PATH_PROTOCOL;

typedef struct {
    EFI_TABLE_HEADER Hdr;
    void *RaiseTPL;
    void *RestoreTPL;
    EFI_STATUS (EFIAPI *AllocatePages)(EFI_ALLOCATE_TYPE, EFI_MEMORY_TYPE, UINTN, EFI_PHYSICAL_ADDRESS *);
    EFI_STATUS (EFIAPI *FreePages)(EFI_PHYSICAL_ADDRESS, UINTN);
    EFI_STATUS (EFIAPI *GetMemoryMap)(UINTN *, EFI_MEMORY_DESCRIPTOR *, UINTN *, UINTN *, UINT32 *);
    EFI_STATUS (EFIAPI *AllocatePool)(EFI_MEMORY_TYPE, UINTN, void **);
    EFI_STATUS (EFIAPI *FreePool)(void *);
    void *CreateEvent;
    void *SetTimer;
    void *WaitForEvent;
    void *SignalEvent;
    void *CloseEvent;
    void *CheckEvent;
    void *InstallProtocolInterface;
    void *ReinstallProtocolInterface;
    void *UninstallProtocolInterface;
    EFI_STATUS (EFIAPI *HandleProtocol)(EFI_HANDLE, EFI_GUID *, void **);
    void *Reserved;
    void *RegisterProtocolNotify;
    void *LocateHandle;
    EFI_STATUS (EFIAPI *LocateDevicePath)(EFI_GUID *, EFI_DEVICE_PATH_PROTOCOL **, EFI_HANDLE *);
    void *InstallConfigurationTable;
    void *LoadImage;
    void *StartImage;
    void *Exit;
    void *UnloadImage;
    EFI_STATUS (EFIAPI *ExitBootServices)(EFI_HANDLE, UINTN);
    void *GetNextMonotonicCount;
    EFI_STATUS (EFIAPI *Stall)(UINTN);
    EFI_STATUS (EFIAPI *SetWatchdogTimer)(UINTN, UINT64, UINTN, CHAR16 *);
    void *ConnectController;
    void *DisconnectController;
    void *OpenProtocol;
    void *CloseProtocol;
    void *OpenProtocolInformation;
    void *ProtocolsPerHandle;
    EFI_STATUS (EFIAPI *LocateHandleBuffer)(EFI_LOCATE_SEARCH_TYPE, EFI_GUID *, void *, UINTN *, EFI_HANDLE **);
    EFI_STATUS (EFIAPI *LocateProtocol)(EFI_GUID *, void *, void **);
    void *InstallMultipleProtocolInterfaces;
    void *UninstallMultipleProtocolInterfaces;
    void *CalculateCrc32;
    void *CopyMem;
    void *SetMem;
    void *CreateEventEx;
} EFI_BOOT_SERVICES;

typedef struct {
    EFI_GUID VendorGuid;
    void    *VendorTable;
} EFI_CONFIGURATION_TABLE;

typedef struct {
    EFI_TABLE_HEADER                    Hdr;
    CHAR16                             *FirmwareVendor;
    UINT32                              FirmwareRevision;
    EFI_HANDLE                          ConsoleInHandle;
    EFI_SIMPLE_TEXT_INPUT_PROTOCOL     *ConIn;
    EFI_HANDLE                          ConsoleOutHandle;
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL    *ConOut;
    EFI_HANDLE                          StandardErrorHandle;
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL    *StdErr;
    void                               *RuntimeServices;
    EFI_BOOT_SERVICES                  *BootServices;
    UINTN                               NumberOfTableEntries;
    EFI_CONFIGURATION_TABLE            *ConfigurationTable;
} EFI_SYSTEM_TABLE;

/* ------------------------------------------------------------ protocolos */

typedef struct {
    UINT32            Revision;
    EFI_HANDLE        ParentHandle;
    EFI_SYSTEM_TABLE *SystemTable;
    EFI_HANDLE        DeviceHandle;
    EFI_DEVICE_PATH_PROTOCOL *FilePath;
    void             *Reserved;
    UINT32            LoadOptionsSize;
    void             *LoadOptions;
    void             *ImageBase;
    UINT64            ImageSize;
    EFI_MEMORY_TYPE   ImageCodeType;
    EFI_MEMORY_TYPE   ImageDataType;
    void             *Unload;
} EFI_LOADED_IMAGE_PROTOCOL;

typedef struct {
    UINT16 Year;
    UINT8  Month, Day, Hour, Minute, Second, Pad1;
    UINT32 Nanosecond;
    UINT16 TimeZone;
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

#define EFI_FILE_MODE_READ 1ull

typedef struct EFI_FILE_PROTOCOL {
    UINT64 Revision;
    EFI_STATUS (EFIAPI *Open)(struct EFI_FILE_PROTOCOL *, struct EFI_FILE_PROTOCOL **, CHAR16 *, UINT64, UINT64);
    EFI_STATUS (EFIAPI *Close)(struct EFI_FILE_PROTOCOL *);
    void *Delete;
    EFI_STATUS (EFIAPI *Read)(struct EFI_FILE_PROTOCOL *, UINTN *, void *);
    void *Write;
    void *GetPosition;
    void *SetPosition;
    EFI_STATUS (EFIAPI *GetInfo)(struct EFI_FILE_PROTOCOL *, EFI_GUID *, UINTN *, void *);
    void *SetInfo;
    void *Flush;
} EFI_FILE_PROTOCOL;

typedef struct EFI_SIMPLE_FILE_SYSTEM_PROTOCOL {
    UINT64 Revision;
    EFI_STATUS (EFIAPI *OpenVolume)(struct EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *, EFI_FILE_PROTOCOL **);
} EFI_SIMPLE_FILE_SYSTEM_PROTOCOL;

typedef struct {
    UINT32  MediaId;
    BOOLEAN RemovableMedia;
    BOOLEAN MediaPresent;
    BOOLEAN LogicalPartition;
    BOOLEAN ReadOnly;
    BOOLEAN WriteCaching;
    UINT32  BlockSize;
    UINT32  IoAlign;
    UINT64  LastBlock;
} EFI_BLOCK_IO_MEDIA;

typedef struct EFI_BLOCK_IO_PROTOCOL {
    UINT64              Revision;
    EFI_BLOCK_IO_MEDIA *Media;
    void *Reset;
    EFI_STATUS (EFIAPI *ReadBlocks)(struct EFI_BLOCK_IO_PROTOCOL *, UINT32, UINT64, UINTN, void *);
    /* D439: el cargador escribe un sector, la cabecera, para borrar la marca del kernel de prueba */
    EFI_STATUS (EFIAPI *WriteBlocks)(struct EFI_BLOCK_IO_PROTOCOL *, UINT32, UINT64, UINTN, void *);
    EFI_STATUS (EFIAPI *FlushBlocks)(struct EFI_BLOCK_IO_PROTOCOL *);
} EFI_BLOCK_IO_PROTOCOL;

/* D270: Disk I/O, lectura por bytes sobre el Block I/O (cualquier tamaño de bloque y
 * cualquier alineación del búfer); el firmware lo pone en cada disco con Block I/O. */
typedef struct EFI_DISK_IO_PROTOCOL {
    UINT64 Revision;
    EFI_STATUS (EFIAPI *ReadDisk)(struct EFI_DISK_IO_PROTOCOL *, UINT32, UINT64, UINTN, void *);
    void *WriteDisk;
} EFI_DISK_IO_PROTOCOL;

/* Graphics Output Protocol (GOP), el sustituto de VBE. */
typedef enum {
    PixelRedGreenBlueReserved8BitPerColor,
    PixelBlueGreenRedReserved8BitPerColor,
    PixelBitMask,
    PixelBltOnly,
    PixelFormatMax
} EFI_GRAPHICS_PIXEL_FORMAT;

typedef struct {
    UINT32 RedMask, GreenMask, BlueMask, ReservedMask;
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

typedef struct EFI_GRAPHICS_OUTPUT_PROTOCOL {
    EFI_STATUS (EFIAPI *QueryMode)(struct EFI_GRAPHICS_OUTPUT_PROTOCOL *, UINT32, UINTN *,
                                   EFI_GRAPHICS_OUTPUT_MODE_INFORMATION **);
    EFI_STATUS (EFIAPI *SetMode)(struct EFI_GRAPHICS_OUTPUT_PROTOCOL *, UINT32);
    void *Blt;
    EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE *Mode;
} EFI_GRAPHICS_OUTPUT_PROTOCOL;

/* EDID de la pantalla conectada (para la resolución nativa del panel). */
typedef struct {
    UINT32 SizeOfEdid;
    UINT8 *Edid;
} EFI_EDID_PROTOCOL;

/* ------------------------------------------------------------ GUID (de la especificación) */

#define GUID_LOADED_IMAGE      { 0x5B1B31A1, 0x9562, 0x11D2, { 0x8E, 0x3F, 0x00, 0xA0, 0xC9, 0x69, 0x72, 0x3B } }
#define GUID_SIMPLE_FS         { 0x964E5B22, 0x6459, 0x11D2, { 0x8E, 0x39, 0x00, 0xA0, 0xC9, 0x69, 0x72, 0x3B } }
#define GUID_FILE_INFO         { 0x09576E92, 0x6D3F, 0x11D2, { 0x8E, 0x39, 0x00, 0xA0, 0xC9, 0x69, 0x72, 0x3B } }
#define GUID_BLOCK_IO          { 0x964E5B21, 0x6459, 0x11D2, { 0x8E, 0x39, 0x00, 0xA0, 0xC9, 0x69, 0x72, 0x3B } }
#define GUID_DISK_IO           { 0xCE345171, 0xBA0B, 0x11D2, { 0x8E, 0x4F, 0x00, 0xA0, 0xC9, 0x69, 0x72, 0x3B } }
#define GUID_DEVICE_PATH       { 0x09576E91, 0x6D3F, 0x11D2, { 0x8E, 0x39, 0x00, 0xA0, 0xC9, 0x69, 0x72, 0x3B } }
#define GUID_GOP               { 0x9042A9DE, 0x23DC, 0x4A38, { 0x96, 0xFB, 0x7A, 0xDE, 0xD0, 0x80, 0x51, 0x6A } }
#define GUID_EDID_ACTIVE       { 0xBD8C1056, 0x9F36, 0x44EC, { 0x92, 0xA8, 0xA6, 0x33, 0x7F, 0x81, 0x79, 0x86 } }
#define GUID_EDID_DISCOVERED   { 0x1C0C34F6, 0xD380, 0x41FA, { 0xA0, 0x49, 0x8A, 0xD0, 0x6C, 0x1A, 0x66, 0xAA } }
#define GUID_TEXT_INPUT_EX     { 0xDD9E7534, 0x7762, 0x4698, { 0x8C, 0x14, 0xF5, 0x85, 0x17, 0xA6, 0x25, 0xAA } }
#define GUID_ACPI_20           { 0x8868E871, 0xE4F1, 0x11D3, { 0xBC, 0x22, 0x00, 0x80, 0xC7, 0x3C, 0x88, 0x81 } }
#define GUID_ACPI_10           { 0xEB9D2D30, 0x2D88, 0x11D3, { 0x9A, 0x16, 0x00, 0x90, 0x27, 0x3F, 0xC1, 0x4D } }
#define GUID_SMBIOS            { 0xEB9D2D31, 0x2D88, 0x11D3, { 0x9A, 0x16, 0x00, 0x90, 0x27, 0x3F, 0xC1, 0x4D } }
#define GUID_SMBIOS3           { 0xF2FD1544, 0x9794, 0x4A2C, { 0x99, 0x2E, 0xE5, 0xBB, 0xCF, 0x20, 0xE3, 0x94 } }

#endif
