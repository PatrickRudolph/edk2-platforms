/** @file

  Fsp SerialIo Uart Debug Property Library

@copyright
  INTEL CONFIDENTIAL
  Copyright 2019 Intel Corporation.

  The source code contained or described herein and all documents related to the
  source code ("Material") are owned by Intel Corporation or its suppliers or
  licensors. Title to the Material remains with Intel Corporation or its suppliers
  and licensors. The Material may contain trade secrets and proprietary and
  confidential information of Intel Corporation and its suppliers and licensors,
  and is protected by worldwide copyright and trade secret laws and treaty
  provisions. No part of the Material may be used, copied, reproduced, modified,
  published, uploaded, posted, transmitted, distributed, or disclosed in any way
  without Intel's prior express written permission.

  No license under any patent, copyright, trade secret or other intellectual
  property right is granted to or conferred upon you by disclosure or delivery
  of the Materials, either expressly, by implication, inducement, estoppel or
  otherwise. Any license under such intellectual property rights must be
  express and approved by Intel in writing.

  Unless otherwise agreed by Intel in writing, you may not remove or alter
  this notice or any other notice embedded in Materials by Intel or
  Intel's suppliers or licensors in any way.

  This file contains an 'Intel Peripheral Driver' and is uniquely identified as
  "Intel Reference Module" and is licensed for Intel CPUs and chipsets under
  the terms of your license agreement with Intel or your vendor. This file may
  be modified by the user, subject to additional terms of the license agreement.

@par Specification Reference:
**/

#include <Base.h>
#include <Uefi/UefiBaseType.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/HobLib.h>
#include <Library/FvLib.h>

#include <StandaloneMmCpu.h>

#include <Guid/FspHeaderFile.h>

/**
  Find FSP header pointer.

  @param[in] FV Firmware volume where to locate the FSP header.

  @return FSP header pointer.
**/
STATIC
FSP_INFO_HEADER *
EFIAPI
FspFindFspHeader (
  IN EFI_FIRMWARE_VOLUME_HEADER  *FV
  )
{
  FSP_INFO_HEADER             *FspInfoHeader;
  EFI_STATUS                  Status;
  EFI_FFS_FILE_HEADER         *FileHeader;
  VOID                        *SectionData;
  UINTN                       SectionDataSize;

  if (FV->Signature != EFI_FVH_SIGNATURE) {
    return NULL;
  }
  if (FV->FvLength < (sizeof(EFI_FIRMWARE_VOLUME_HEADER) + sizeof(FSP_INFO_HEADER))) {
    return NULL;
  }

  //
  // Locate first file in FV
  //
  FileHeader = NULL;

  Status = FfsFindNextFile (
             EFI_FV_FILETYPE_ALL,
             FV,
             &FileHeader
             );
  if (EFI_ERROR (Status)) {
    return NULL;
  }

  //
  // First file is the FSP header
  //
  if (!CompareGuid(&FileHeader->Name, &gFspHeaderFileGuid)) {
    return NULL;
  }

  //
  // Get RAW section
  //
  Status = FfsFindSectionData (
             EFI_SECTION_RAW,
             FileHeader,
             &SectionData,
             &SectionDataSize
             );
  if (EFI_ERROR (Status)) {
    return NULL;
  }

  if (SectionDataSize < sizeof (FSP_INFO_HEADER)) {
    return NULL;
  }

  FspInfoHeader = SectionData;

  //
  // Validate FSP_INFO_HEADER
  //
  if ((FspInfoHeader->Signature != FSP_INFO_HEADER_SIGNATURE) ||
      (FspInfoHeader->ImageBase != (UINTN)FV) ||
      ((FspInfoHeader->SpecVersion & 0xf0) != 0x20) ||
      (FspInfoHeader->HeaderRevision < 7)) {
    return NULL;
  }

  return FspInfoHeader;
}


/**
  Updates the the FSP header with 

  @param  ImageHandle   ImageHandle of the loaded driver.
  @param  SystemTable   Pointer to the EFI System Table.

  @retval  EFI_SUCCESS          PcdPasswordCleared is got successfully.

**/
EFI_STATUS
EFIAPI
FspHeaderUpdateMmInitialize (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_MM_SYSTEM_TABLE  *mMmst
  )
{
  FSP_INFO_HEADER              *mFspInfoHeader;
  EFI_HOB_GUID_TYPE            *Hob;
  MM_CPU_DRIVER_EP_DESCRIPTOR  *CpuDriverEntryPointDesc;
  EFI_CONFIGURATION_TABLE      *ConfigurationTable;
  UINTN                        Index;
  VOID                         *HobStart;
  EFI_HOB_FIRMWARE_VOLUME      *BfvHob;

  ASSERT (mMmst != NULL);

  // Retrieve the Hoblist from the MMST to extract the details of the NS
  // communication buffer that has been reserved for StandaloneMmPkg
  ConfigurationTable = mMmst->MmConfigurationTable;
  for (Index = 0; Index < mMmst->NumberOfTableEntries; Index++) {
    if (CompareGuid (&gEfiHobListGuid, &(ConfigurationTable[Index].VendorGuid))) {
      break;
    }
  }

  // Bail out if the Hoblist could not be found
  if (Index >= mMmst->NumberOfTableEntries) {
    DEBUG ((DEBUG_ERROR, "Hoblist not found - 0x%x\n", Index));
    return EFI_OUT_OF_RESOURCES;
  }

  HobStart = ConfigurationTable[Index].VendorTable;

  //
  // Get Boot Firmware Volume address from the BFV Hob
  //
  BfvHob = HobStart;
  do {
    BfvHob = GetNextHob (EFI_HOB_TYPE_FV, BfvHob);
    if (BfvHob == NULL) {
      return EFI_UNSUPPORTED;
    }

    mFspInfoHeader = FspFindFspHeader((VOID *)(UINTN)BfvHob->BaseAddress);
  } while (!mFspInfoHeader);

  //
  // Locate the HOB with the buffer to populate the entry point of this driver
  //
  Hob = GetNextGuidHob (&gEfiMmCpuDriverEpDescriptorGuid, HobStart);
  if (Hob == NULL) {
    DEBUG ((DEBUG_ERROR, "HOB with Guid %g not found\n", &gEfiMmCpuDriverEpDescriptorGuid));
    return EFI_NOT_FOUND;
  }

  CpuDriverEntryPointDesc = (MM_CPU_DRIVER_EP_DESCRIPTOR *)GET_GUID_HOB_DATA (Hob);
  if (CpuDriverEntryPointDesc == NULL) {
    return EFI_NOT_FOUND;
  }

  //
  // Tell bootloader about new function entry offset
  //
  mFspInfoHeader->FspMultiPhaseSiInitEntryOffset = (UINTN)*CpuDriverEntryPointDesc->MmCpuDriverEpPtr - (UINTN)BfvHob->BaseAddress;

  //
  // Tell bootloader not to use main entry offset any more
  //
  mFspInfoHeader->TempRamInitEntryOffset = 0;

  DEBUG ((DEBUG_INFO, "FspHeaderUpdateMm: Updated FSP header\n"));

  return EFI_SUCCESS;
}

