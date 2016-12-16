
#include <Uefi.h>
#include <Library/DebugLib.h>
#include <IndustryStandard/Pci.h>
#include <Library/BaseMemoryLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/UefiDriverEntryPoint.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/BaseLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiLib.h>
#include <Library/IoLib.h>
#include <Protocol/PciIo.h>
#include <Protocol/IdeControllerInit.h>
#include <Protocol/ComponentName.h>
#include <Protocol/ComponentName2.h>
#include <Protocol/DriverBinding.h>
#include <Protocol/DevicePath.h>
#include <PlatformDefinition.h>
#include "../CommonLib.h"
#include <SetupVariable.h>

#define AHCI_MULTI_MAX_DEVICES              16
#define AHCI_MAX_PORTS                      4
#define IDE_MAX_CHANNELS                    2
#define IDE_MAX_DEVICES                     2
#define SATA_CONTROLLER_SIGNATURE           SIGNATURE_32('S', 'A', 'T', 'A')

typedef struct _EFI_SATA_CONTROLLER_PRIVATE_DATA {
  UINT32                            Signature;
  EFI_IDE_CONTROLLER_INIT_PROTOCOL  IdeInit;
  EFI_PCI_IO_PROTOCOL               *PciIo;
  EFI_ATA_COLLECTIVE_MODE           DisqulifiedModes[AHCI_MAX_PORTS][AHCI_MULTI_MAX_DEVICES];
  EFI_IDENTIFY_DATA                 IdentifyData[AHCI_MAX_PORTS][AHCI_MULTI_MAX_DEVICES];
  BOOLEAN                           IdentifyValid[AHCI_MAX_PORTS][AHCI_MULTI_MAX_DEVICES];
} EFI_SATA_CONTROLLER_PRIVATE_DATA;

#define SATA_CONTROLLER_PRIVATE_DATA_FROM_THIS(a) \
  CR ( \
  a, \
  EFI_SATA_CONTROLLER_PRIVATE_DATA, \
  IdeInit, \
  SATA_CONTROLLER_SIGNATURE \
  )

STATIC EFI_GUID gSataControllerDriverGuid = {0x65aba490, 0xb147, 0x4cd0, { 0xb6, 0xf3, 0x59, 0xf3, 0x21, 0x91, 0x4, 0x72 }};





STATIC
EFI_STATUS
IdeInitGetChannelInfo (
  IN   EFI_IDE_CONTROLLER_INIT_PROTOCOL    *This,
  IN   UINT8                               Channel,
  OUT  BOOLEAN                             *Enabled,
  OUT  UINT8                               *MaxDevices
  )
{
  EFI_SATA_CONTROLLER_PRIVATE_DATA  *SataPrivateData;

  SataPrivateData = SATA_CONTROLLER_PRIVATE_DATA_FROM_THIS(This);
  ASSERT (SataPrivateData);

  if (Channel < This->ChannelCount) {
    *Enabled = TRUE;
    *MaxDevices = IDE_MAX_DEVICES;
    return EFI_SUCCESS;
  } else {
    *Enabled = FALSE;
    return EFI_INVALID_PARAMETER;
  }
}

STATIC
EFI_STATUS
IdeInitNotifyPhase (
  IN  EFI_IDE_CONTROLLER_INIT_PROTOCOL  *This,
  IN  EFI_IDE_CONTROLLER_ENUM_PHASE     Phase,
  IN  UINT8                             Channel
  )
{
  if (Channel >= This->ChannelCount) {
    return EFI_INVALID_PARAMETER;
  }

  switch (Phase) {
  case EfiIdeBeforeChannelEnumeration:
  case EfiIdeAfterChannelEnumeration:
  case EfiIdeBeforeChannelReset:
  case EfiIdeAfterChannelReset:
  case EfiIdeBusBeforeDevicePresenceDetection:
  case EfiIdeBusAfterDevicePresenceDetection:
  case EfiIdeResetMode:
    break;

  default:
    return EFI_UNSUPPORTED;
    break;
  }

  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
IdeInitSubmitData (
  IN  EFI_IDE_CONTROLLER_INIT_PROTOCOL    *This,
  IN  UINT8                               Channel,
  IN  UINT8                               Device,
  IN  EFI_IDENTIFY_DATA                   *IdentifyData
  )
{
  EFI_SATA_CONTROLLER_PRIVATE_DATA  *SataPrivateData;
  
  SataPrivateData = SATA_CONTROLLER_PRIVATE_DATA_FROM_THIS(This);
  ASSERT (SataPrivateData);

  if ((Channel >= This->ChannelCount) || (Device >= IDE_MAX_DEVICES)) {
    return EFI_INVALID_PARAMETER;
  }

  if (IdentifyData != NULL) {
    CopyMem (
      &(SataPrivateData->IdentifyData[Channel][Device]),
      IdentifyData,
      sizeof(EFI_IDENTIFY_DATA)
      );
    SataPrivateData->IdentifyValid[Channel][Device] = TRUE;
  } else {
    SataPrivateData->IdentifyValid[Channel][Device] = FALSE;
  }

  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
IdeInitDisqualifyMode (
  IN  EFI_IDE_CONTROLLER_INIT_PROTOCOL    *This,
  IN  UINT8                               Channel,
  IN  UINT8                               Device,
  IN  EFI_ATA_COLLECTIVE_MODE             *BadModes
  )
{
  EFI_SATA_CONTROLLER_PRIVATE_DATA  *SataPrivateData;
  
  SataPrivateData = SATA_CONTROLLER_PRIVATE_DATA_FROM_THIS(This);
  ASSERT(SataPrivateData);

  if ((Channel >= This->ChannelCount) || (BadModes == NULL) || (Device >= IDE_MAX_DEVICES)) {
    return EFI_INVALID_PARAMETER;
  }

  CopyMem (
    &(SataPrivateData->DisqulifiedModes[Channel][Device]),
    BadModes,
    sizeof (EFI_ATA_COLLECTIVE_MODE)
    );
  //
  DEBUG ((EFI_D_ERROR, "Zoey:1\n"));
  //

  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
CalculateBestUdmaMode (
  IN  EFI_IDENTIFY_DATA    *IdentifyData,
  IN  UINT16               *DisUDmaMode OPTIONAL,
  OUT UINT16               *SelectedMode
  )
{
  UINT16  TempMode;
  UINT16  DeviceUDmaMode;

  DeviceUDmaMode = 0;
  //Zoey:When field_validity bit2 is 1, ultra_dma_mode(word 88)is valid
  if ((IdentifyData->AtaData.field_validity & 0x04) == 0x00) {
    return EFI_UNSUPPORTED;
  }

  DeviceUDmaMode = IdentifyData->AtaData.ultra_dma_mode;
  DeviceUDmaMode &= 0x3f;//ultra_dma_mode(word 88) bit5-0 is valid
  TempMode = 0;

  while ((DeviceUDmaMode >>= 1) != 0) {
    TempMode++;
  }

  if (DisUDmaMode != NULL) {
    if (*DisUDmaMode == 0) {
      *SelectedMode = 0;
      return EFI_UNSUPPORTED;
    }

    if (TempMode >= *DisUDmaMode) {
      TempMode = (UINT16) (*DisUDmaMode - 1);
    }
  }

  *SelectedMode = TempMode;
  //
  DEBUG ((EFI_D_ERROR, "Zoey:2\n"));
  //

  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
CalculateBestPioMode (
  IN  EFI_IDENTIFY_DATA    * IdentifyData,
  IN  UINT16               *DisPioMode OPTIONAL,
  OUT UINT16               *SelectedMode
  )
{
  UINT16  PioMode;
  UINT16  AdvancedPioMode;
  UINT16  Temp;
  UINT16  Index;
  UINT16  MinimumPioCycleTime;

  Temp    = 0xff;

  PioMode = (UINT8) (IdentifyData->AtaData.obsolete_51_52[0] >> 8);

  //
  // see whether Identify Data word 64 - 70 are valid
  //
  if ((IdentifyData->AtaData.field_validity & 0x02) == 0x02) {

    AdvancedPioMode = IdentifyData->AtaData.advanced_pio_modes;

    for (Index = 0; Index < 8; Index++) {
      if ((AdvancedPioMode & 0x01) != 0) {
        Temp = Index;
      }

      AdvancedPioMode >>= 1;
    }
    //
    // if Temp is modified, meant the advanced_pio_modes is not zero;
    // if Temp is not modified, meant the no advanced PIO Mode is supported,
    // the best PIO Mode is the value in pio_cycle_timing.
    //
    if (Temp != 0xff) {
      AdvancedPioMode = (UINT16) (Temp + 3);
    } else {
      AdvancedPioMode = PioMode;
    }
    //
    // Limit the PIO mode to at most PIO4.
    //
    PioMode             = (UINT16) (AdvancedPioMode < 4 ? AdvancedPioMode : 4);

    MinimumPioCycleTime = IdentifyData->AtaData.min_pio_cycle_time_with_flow_control;

    if (MinimumPioCycleTime <= 120) {
      PioMode = (UINT16) (4 < PioMode ? 4 : PioMode);
    } else if (MinimumPioCycleTime <= 180) {
      PioMode = (UINT16) (3 < PioMode ? 3 : PioMode);
    } else if (MinimumPioCycleTime <= 240) {
      PioMode = (UINT16) (2 < PioMode ? 2 : PioMode);
    } else {
      PioMode = 0;
    }
    //
    // Degrade the PIO mode if the mode has been disqualified
    //
    if (DisPioMode != NULL) {

      if (*DisPioMode < 2) {
        return EFI_UNSUPPORTED;
        //
        // no mode below ATA_PIO_MODE_BELOW_2
        //
      }

      if (PioMode >= *DisPioMode) {
        PioMode = (UINT16) (*DisPioMode - 1);
      }
    }

    if (PioMode < 2) {
      *SelectedMode = 1;
      //
      // ATA_PIO_MODE_BELOW_2;
      //
    } else {
      *SelectedMode = PioMode;
      //
      // ATA_PIO_MODE_2 to ATA_PIO_MODE_4;
      //
    }

  } else {
    //
    // Identify Data word 64 - 70 are not valid
    // Degrade the PIO mode if the mode has been disqualified
    //
    if (DisPioMode != NULL) {

      if (*DisPioMode < 2) {
        return EFI_UNSUPPORTED;
        //
        // no mode below ATA_PIO_MODE_BELOW_2
        //
      }

      if (PioMode == *DisPioMode) {
        PioMode--;
      }
    }

    if (PioMode < 2) {
      *SelectedMode = 1;
      //
      // ATA_PIO_MODE_BELOW_2;
      //
    } else {
      *SelectedMode = 2;
      //
      // ATA_PIO_MODE_2;
      //
    }

  }
  //
  DEBUG ((EFI_D_ERROR, "Zoey:3\n"));
  //

  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
IdeInitCalculateMode (
  IN  EFI_IDE_CONTROLLER_INIT_PROTOCOL       *This,
  IN  UINT8                                  Channel,
  IN  UINT8                                  Device,
  IN OUT EFI_ATA_COLLECTIVE_MODE             **SupportedModes
  )
{
  EFI_SATA_CONTROLLER_PRIVATE_DATA  *SataPrivateData;
  EFI_IDENTIFY_DATA                 *IdentifyData;
  BOOLEAN                           IdentifyValid;
  EFI_ATA_COLLECTIVE_MODE           *DisqulifiedModes;
  UINT16                            SelectedMode;
  EFI_STATUS                        Status;

  SataPrivateData = SATA_CONTROLLER_PRIVATE_DATA_FROM_THIS(This);
  ASSERT (SataPrivateData);

  if ((Channel >= This->ChannelCount) || (SupportedModes == NULL) || (Device >= IDE_MAX_DEVICES)) {
    return EFI_INVALID_PARAMETER;
  }

  *SupportedModes   = AllocateZeroPool(sizeof(EFI_ATA_COLLECTIVE_MODE));
  if ( *SupportedModes == NULL) {
      ASSERT (FALSE);
      return EFI_OUT_OF_RESOURCES;
  }

  IdentifyData      = &(SataPrivateData->IdentifyData[Channel][Device]);
  DisqulifiedModes  = &(SataPrivateData->DisqulifiedModes[Channel][Device]);
  IdentifyValid     = SataPrivateData->IdentifyValid[Channel][Device];

  //
  // Make sure we've got the valid identify data of the device from SubmitData()
  //
  if (!IdentifyValid) {
    return EFI_NOT_READY;
  }

  Status = CalculateBestPioMode (
            IdentifyData,
            (DisqulifiedModes->PioMode.Valid ? ((UINT16 *) &(DisqulifiedModes->PioMode.Mode)) : NULL),
            &SelectedMode
            );
  if (!EFI_ERROR (Status)) {
    (*SupportedModes)->PioMode.Valid  = TRUE;
    (*SupportedModes)->PioMode.Mode   = SelectedMode;

  } else {
    (*SupportedModes)->PioMode.Valid = FALSE;
  }

  Status = CalculateBestUdmaMode (
            IdentifyData,
            (DisqulifiedModes->UdmaMode.Valid ? ((UINT16 *) &(DisqulifiedModes->UdmaMode.Mode)) : NULL),
            &SelectedMode
            );

  if (!EFI_ERROR (Status)) {
    (*SupportedModes)->UdmaMode.Valid = TRUE;
    (*SupportedModes)->UdmaMode.Mode  = SelectedMode;

  } else {
    (*SupportedModes)->UdmaMode.Valid = FALSE;
  }

  DEBUG((EFI_D_INFO, "Pio:%d, Udma:%d, MWDma:%d, ExtModeCount:%d\n", \
                     (*SupportedModes)->PioMode.Valid?(*SupportedModes)->PioMode.Mode:-1,  \
                     (*SupportedModes)->UdmaMode.Valid?(*SupportedModes)->UdmaMode.Mode:-1, \
                     (*SupportedModes)->MultiWordDmaMode.Valid?(*SupportedModes)->MultiWordDmaMode.Mode:-1, \
                     (*SupportedModes)->ExtModeCount));

  //
  DEBUG ((EFI_D_ERROR, "Zoey:4\n"));
  //

  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
IdeInitSetTiming (
  IN  EFI_IDE_CONTROLLER_INIT_PROTOCOL    *This,
  IN  UINT8                               Channel,
  IN  UINT8                               Device,
  IN  EFI_ATA_COLLECTIVE_MODE             *Modes
  )
{
  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
SataControllerSupported (
  IN EFI_DRIVER_BINDING_PROTOCOL *This,
  IN EFI_HANDLE                  Controller,
  IN EFI_DEVICE_PATH_PROTOCOL    *RemainingDevicePath
  )
{
  EFI_STATUS                Status;
  EFI_DEVICE_PATH_PROTOCOL  *ParentDevicePath;
  EFI_PCI_IO_PROTOCOL       *PciIo;
  PCI_TYPE00                PciData;


  Status = gBS->OpenProtocol (
                  Controller,
                  &gEfiDevicePathProtocolGuid,
                  (VOID*)&ParentDevicePath,
                  This->DriverBindingHandle,
                  Controller,
                  EFI_OPEN_PROTOCOL_BY_DRIVER
                  );
  if (EFI_ERROR (Status)) {
    return Status;
  }
  gBS->CloseProtocol (
        Controller,
        &gEfiDevicePathProtocolGuid,
        This->DriverBindingHandle,
        Controller
        );

  Status = gBS->OpenProtocol (
                  Controller,
                  &gEfiPciIoProtocolGuid,
                  (VOID**)&PciIo,
                  This->DriverBindingHandle,
                  Controller,
                  EFI_OPEN_PROTOCOL_BY_DRIVER
                  );
  if (EFI_ERROR (Status)) {
    return Status;
  }
  Status = PciIo->Pci.Read (
                        PciIo,
                        EfiPciIoWidthUint8,
                        0,
                        sizeof(PciData),
                        &PciData
                        );
  gBS->CloseProtocol (
         Controller,
         &gEfiPciIoProtocolGuid,
         This->DriverBindingHandle,
         Controller
         );
    //
  DEBUG ((EFI_D_ERROR, "Zoey:5\n"));
  //
  if (EFI_ERROR (Status)) {
    return EFI_UNSUPPORTED;
  }

 /* if (PciData.Hdr.VendorId != CHIPSET_VENDOR_ID) {
    return EFI_UNSUPPORTED;    
  }*/
  if(PciData.Hdr.ClassCode[2] != PCI_CLASS_MASS_STORAGE){
    return EFI_UNSUPPORTED; 
  }		
  if (PciData.Hdr.ClassCode[1] == PCI_CLASS_MASS_STORAGE_IDE ||
      PciData.Hdr.ClassCode[1] == PCI_CLASS_MASS_STORAGE_AHCI){
    return EFI_SUCCESS;
  } else {
    return EFI_UNSUPPORTED; 
  }

}



EFI_STATUS
EFIAPI
SataControllerStart (
  IN EFI_DRIVER_BINDING_PROTOCOL    *This,
  IN EFI_HANDLE                     Controller,
  IN EFI_DEVICE_PATH_PROTOCOL       *RemainingDevicePath
  )
{
  EFI_STATUS                        Status;
  EFI_PCI_IO_PROTOCOL               *PciIo;
  EFI_SATA_CONTROLLER_PRIVATE_DATA  *SataPrivateData;
  PCI_TYPE00                        PciData;
  UINTN                             SegNum;
  UINTN                             BusNum;
  UINTN                             DevNum;
  UINTN                             FuncNum;
  UINT64                            CommandVal;


  SataPrivateData = NULL;

  Status = gBS->OpenProtocol (
                  Controller,
                  &gEfiPciIoProtocolGuid,
                  (VOID**)&PciIo,
                  This->DriverBindingHandle,
                  Controller,
                  EFI_OPEN_PROTOCOL_BY_DRIVER
                  );
  if (EFI_ERROR (Status)) {
    return Status;
  }

//Zoey:为SataPrivateData分配内存，并将所有元素设0
  SataPrivateData = AllocateZeroPool(sizeof(EFI_SATA_CONTROLLER_PRIVATE_DATA));
  if (SataPrivateData == NULL) {
    DEBUG ((EFI_D_ERROR, "malloc error!\n"));
    Status = EFI_OUT_OF_RESOURCES;
    goto Done;
  }
  SataPrivateData->Signature              = SATA_CONTROLLER_SIGNATURE;
  SataPrivateData->PciIo                  = PciIo;
  SataPrivateData->IdeInit.GetChannelInfo = IdeInitGetChannelInfo;//Zoey:获取指定Channel信息
  SataPrivateData->IdeInit.NotifyPhase    = IdeInitNotifyPhase;//Zoey:在执行特定操作前，IdeBus driver调用该函数,允许IDE Controller Init为其他操作做准备
  SataPrivateData->IdeInit.SubmitData     = IdeInitSubmitData;//Zoey:This function is called by IdeBus driver to submit EFI_IDENTIFY_DATA data structure obtained from IDE deivce. This structure is used to set IDE timing
  SataPrivateData->IdeInit.DisqualifyMode = IdeInitDisqualifyMode;//Zoey:This function is called by IdeBus driver to disqualify unsupported operation mode on specfic IDE device
  SataPrivateData->IdeInit.CalculateMode  = IdeInitCalculateMode;//Zoey:This function is called by IdeBus driver to calculate the best operation mode supported by specific IDE device
  SataPrivateData->IdeInit.SetTiming      = IdeInitSetTiming;//Zoey:This function is called by IdeBus driver to set appropriate timing on IDE controller according supported operation mode.
  SataPrivateData->IdeInit.EnumAll        = FALSE;//Zoey:Set to FALSE if an enumeration group consists of only one channel.  

  Status = PciIo->GetLocation (
                    PciIo,
                    &SegNum,
                    &BusNum,
                    &DevNum,
                    &FuncNum
                    );
  ASSERT_EFI_ERROR(Status);

  Status = PciIo->Pci.Read (
                        PciIo,
                        EfiPciIoWidthUint8,
                        0,
                        sizeof(PciData),
                        &PciData
                        );
  ASSERT_EFI_ERROR(Status);

  Status = PciIo->Attributes (
                     PciIo,
                     EfiPciIoAttributeOperationSupported,
                     0,
                     &CommandVal
                     );     
  ASSERT_EFI_ERROR(Status);
  Status = PciIo->Attributes (
                    PciIo,
                    EfiPciIoAttributeOperationEnable,
                    CommandVal & EFI_PCI_DEVICE_ENABLE,
                    NULL
                    );
  ASSERT_EFI_ERROR(Status);
	
  if (PciData.Hdr.ClassCode[1] == PCI_CLASS_MASS_STORAGE_IDE) {
    SataPrivateData->IdeInit.ChannelCount = WaitIdeDeviceReady(BusNum, DevNum, FuncNum);
  } else if (PciData.Hdr.ClassCode[1] == PCI_CLASS_MASS_STORAGE_AHCI) {
    SataPrivateData->IdeInit.ChannelCount = AHCI_MAX_PORTS;
  }

  Status = gBS->InstallMultipleProtocolInterfaces (
                  &Controller,
                  &gSataControllerDriverGuid,
                  SataPrivateData,
                  &gEfiIdeControllerInitProtocolGuid,
                  &(SataPrivateData->IdeInit),
                  NULL
                  );

Done:
  if (EFI_ERROR (Status)) {
    gBS->CloseProtocol (
          Controller,
          &gEfiPciIoProtocolGuid,
          This->DriverBindingHandle,
          Controller
          );
    if (SataPrivateData != NULL) {
      FreePool (SataPrivateData);
    }
  }
  //
  DEBUG ((EFI_D_ERROR, "Zoey:6\n"));
  //

  return Status;
}



EFI_STATUS
EFIAPI
SataControllerStop (
  IN  EFI_DRIVER_BINDING_PROTOCOL     *This,
  IN  EFI_HANDLE                      Controller,
  IN  UINTN                           NumberOfChildren,
  IN  EFI_HANDLE                      *ChildHandleBuffer
  )
{
  EFI_STATUS                        Status;
  EFI_SATA_CONTROLLER_PRIVATE_DATA  *SataPrivateData;

  Status = gBS->OpenProtocol (
                  Controller,
                  &gSataControllerDriverGuid,
                  (VOID **) &SataPrivateData,
                  This->DriverBindingHandle,
                  Controller,
                  EFI_OPEN_PROTOCOL_GET_PROTOCOL
                  );

  if (!EFI_ERROR (Status)) {
    gBS->UninstallMultipleProtocolInterfaces (
          Controller,
          &gSataControllerDriverGuid,
          SataPrivateData,
          &gEfiIdeControllerInitProtocolGuid,
          &(SataPrivateData->IdeInit),
          NULL
          );
  }

  gBS->CloseProtocol (
        Controller,
        &gEfiPciIoProtocolGuid,
        This->DriverBindingHandle,
        Controller
        );

  if (SataPrivateData != NULL) {
    FreePool(SataPrivateData);
  }
  //
  DEBUG ((EFI_D_ERROR, "Zoey:7\n"));
  //

  return EFI_SUCCESS;
}

STATIC EFI_DRIVER_BINDING_PROTOCOL gSataControllerDriverBinding = {
  SataControllerSupported,
  SataControllerStart,
  SataControllerStop,
  0x10,
  NULL,
  NULL
};


STATIC
EFI_STATUS
EFIAPI
SataControllerGetDriverName (
  IN  EFI_COMPONENT_NAME_PROTOCOL  *This,
  IN  CHAR8                        *Language,
  OUT CHAR16                       **DriverName
  );
STATIC
EFI_STATUS
EFIAPI
SataControllerGetControllerName (
  IN  EFI_COMPONENT_NAME_PROTOCOL   *This,
  IN  EFI_HANDLE                    ControllerHandle,
  IN  EFI_HANDLE                    ChildHandle  OPTIONAL,
  IN  CHAR8                         *Language,
  OUT CHAR16                        **ControllerName
  );

STATIC EFI_COMPONENT_NAME_PROTOCOL gSataControllerComponentName = {
  SataControllerGetDriverName,
  SataControllerGetControllerName,
  "eng"
};

STATIC EFI_COMPONENT_NAME2_PROTOCOL gSataControllerComponentName2 = {
  (EFI_COMPONENT_NAME2_GET_DRIVER_NAME)SataControllerGetDriverName,
  (EFI_COMPONENT_NAME2_GET_CONTROLLER_NAME)SataControllerGetControllerName,
  "en"
};

STATIC EFI_UNICODE_STRING_TABLE gSataControllerDriverNameTable[] = {
  {
    "eng",
    L"Onboard Serial ATA Controller Initialization Driver"
  },
  {
    NULL,
    NULL
  }
};

STATIC EFI_UNICODE_STRING_TABLE gSataControllerControllerNameTable[] = {
  {
    "eng",
    L"Onboard Serial ATA Controller"
  },
  {
    NULL,
    NULL
  }
};


STATIC
EFI_STATUS
SataControllerGetDriverName (
  IN  EFI_COMPONENT_NAME_PROTOCOL   *This,
  IN  CHAR8                         *Language,
  OUT CHAR16                        **DriverName
  )
{
	//
	DEBUG ((EFI_D_ERROR, "Zoey:8\n"));
	//

  return LookupUnicodeString2 (
           Language,
           This->SupportedLanguages,
           gSataControllerDriverNameTable,
           DriverName,
           (BOOLEAN)(This == &gSataControllerComponentName)
           );
}

STATIC
EFI_STATUS
SataControllerGetControllerName (
  IN  EFI_COMPONENT_NAME_PROTOCOL   *This,
  IN  EFI_HANDLE                    ControllerHandle,
  IN  EFI_HANDLE                    ChildHandle        OPTIONAL,
  IN  CHAR8                         *Language,
  OUT CHAR16                        **ControllerName
  )
{
	//
	DEBUG ((EFI_D_ERROR, "Zoey:9\n"));
	//

  return LookupUnicodeString2 (
           Language,
           This->SupportedLanguages,
           gSataControllerControllerNameTable,
           ControllerName,
           (BOOLEAN)(This == &gSataControllerComponentName)
           );
}




EFI_STATUS
EFIAPI
SataControllerInstall (
  IN EFI_HANDLE           ImageHandle,
  IN EFI_SYSTEM_TABLE     *SystemTable
  )
{
  EFI_STATUS  Status;

  Status = EfiLibInstallDriverBindingComponentName2 (
             ImageHandle,
             SystemTable,
             &gSataControllerDriverBinding,
             ImageHandle,
             &gSataControllerComponentName,
             &gSataControllerComponentName2
             );
  ASSERT_EFI_ERROR (Status);
    //
  DEBUG ((EFI_D_ERROR, "Zoey:10\n"));
  //
  return Status;
}













