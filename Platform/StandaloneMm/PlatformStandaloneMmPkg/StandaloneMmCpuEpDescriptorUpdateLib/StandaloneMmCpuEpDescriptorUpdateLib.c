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

#include <PiMm.h>

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/HobLib.h>
#include <StandaloneMmCpu.h>

STATIC   PI_MM_CPU_DRIVER_ENTRYPOINT  MmCpuDriverEpPtr;

/**
  Updates the gEfiMmCpuDriverEpDescriptorGuid HOB and point MmCpuDriverEpPtr
  to a temporary buffer within MM.

  @param  ImageHandle   ImageHandle of the loaded driver.
  @param  SystemTable   Pointer to the EFI System Table.

  @retval  EFI_SUCCESS          gEfiMmCpuDriverEpDescriptorGuid HOB was successfully updated.

**/
EFI_STATUS
EFIAPI
StandaloneMmCpuEpDescriptorUpdateLibConstructor (
  IN EFI_HANDLE           ImageHandle,
  IN EFI_MM_SYSTEM_TABLE  *mMmst
  )
{
  EFI_HOB_GUID_TYPE            *Hob;
  MM_CPU_DRIVER_EP_DESCRIPTOR  *CpuDriverEntryPointDesc;
  EFI_CONFIGURATION_TABLE      *ConfigurationTable;
  UINTN                        Index;
  VOID* HobStart;

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
  // Locate the HOB with the buffer to populate the entry point of this driver
  //
  Hob = GetNextGuidHob (&gEfiMmCpuDriverEpDescriptorGuid, HobStart);
  if (Hob == NULL) {
    DEBUG ((DEBUG_ERROR, "Hob with Guid %g not found\n", &gEfiMmCpuDriverEpDescriptorGuid));
    return EFI_NOT_FOUND;
  }

  CpuDriverEntryPointDesc = (MM_CPU_DRIVER_EP_DESCRIPTOR *)GET_GUID_HOB_DATA (Hob);
  if (CpuDriverEntryPointDesc == NULL) {
    return EFI_NOT_FOUND;
  }

  //
  // Point to a temporary buffer in MM
  //
  CpuDriverEntryPointDesc->MmCpuDriverEpPtr = &MmCpuDriverEpPtr;

  return EFI_SUCCESS;
}
