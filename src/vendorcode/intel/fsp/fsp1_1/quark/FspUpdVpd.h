/** @file

Copyright (c) 2015-2016, Intel Corporation. All rights reserved.<BR>

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

* Redistributions of source code must retain the above copyright notice, this
  list of conditions and the following disclaimer.
* Redistributions in binary form must reproduce the above copyright notice, this
  list of conditions and the following disclaimer in the documentation and/or
  other materials provided with the distribution.
* Neither the name of Intel Corporation nor the names of its contributors may
  be used to endorse or promote products derived from this software without
  specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
  THE POSSIBILITY OF SUCH DAMAGE.

  This file is automatically generated. Please do NOT modify !!!

**/

#ifndef __FSPUPDVPD_H__
#define __FSPUPDVPD_H__

#pragma pack(push, 1)

#define MAX_CHANNELS_NUM       2
#define MAX_DIMMS_NUM          2

typedef struct {
  UINT8         DimmId;
  UINT32        SizeInMb;
  UINT16        MfgId;
  UINT8         ModulePartNum[20];/* Module part number for DDR3 is 18 bytes however for DRR4 20 bytes as per JEDEC Spec, so reserving 20 bytes */
} DIMM_INFO;

typedef struct {
  UINT8         ChannelId;
  UINT8         DimmCount;
  DIMM_INFO     DimmInfo[MAX_DIMMS_NUM];
} CHANNEL_INFO;

typedef struct {
  UINT8         Revision;
  UINT16        DataWidth;
  /** As defined in SMBIOS 3.0 spec
    Section 7.18.2 and Table 75
  **/
  UINT8         MemoryType;
  UINT16        MemoryFrequencyInMHz;
  /** As defined in SMBIOS 3.0 spec
    Section 7.17.3 and Table 72
  **/
  UINT8         ErrorCorrectionType;
  UINT8         ChannelCount;
  CHANNEL_INFO  ChannelInfo[MAX_CHANNELS_NUM];
} FSP_SMBIOS_MEMORY_INFO;

/** UPD data structure for FspMemoryInitApi
**/
typedef struct {

/** Offset 0x0020
**/
  UINT64                      Signature;

/** Offset 0x0028 - Revision
  Revision version of the MemoryInitUpd Region
**/
  UINT8                       Revision;
} MEMORY_INIT_UPD;

/** UPD data structure for FspSiliconInitApi
**/
typedef struct {

/** Offset 0x0200
**/
  UINT64                      Signature;

/** Offset 0x0208 - Revision
  Revision version of the SiliconInitUpd Region
**/
  UINT8                       Revision;
} SILICON_INIT_UPD;

#define FSP_UPD_SIGNATURE                0x244450554B525124        /* '$QRKUPD$' */
#define FSP_MEMORY_INIT_UPD_SIGNATURE    0x244450554D454D24        /* '$MEMUPD$' */
#define FSP_SILICON_INIT_UPD_SIGNATURE   0x244450555F495324        /* '$SI_UPD$' */

/** UPD data structure. The UPD_DATA_REGION may contain some reserved or unused fields in the data structure. These fields are required to use the default values provided in the FSP binary. Intel always recommends copying the whole UPD_DATA_REGION from the flash to a local structure in the stack before overriding any field.
**/
typedef struct {

/** Offset 0x0000
**/
  UINT64                      Signature;

/** Offset 0x0008 - This field is not an option and is a Revision of the UPD_DATA_REGION. It can be used by the boot loader to validate the UPD region. If the value in this field is changed for an FSP release, the boot loader should not assume the same layout for the UPD_DATA_REGION data structure. Instead it should use the new FspUpdVpd.h from the FSP release package.
  Size of SMRAM memory reserved. 0x400000 for Release build and 0x1000000 for Debug build
**/
  UINT8                       Revision;

/** Offset 0x0009
**/
  UINT8                       ReservedUpd0[7];

/** Offset 0x0010 - MemoryInitUpdOffset
  This field contains the offset of the MemoryInitUpd structure relative to UPD_DATA_REGION
**/
  UINT32                      MemoryInitUpdOffset;

/** Offset 0x0014 - SiliconInitUpdOffset
  This field contains the offset of the SiliconInitUpd structure relative to UPD_DATA_REGION
**/
  UINT32                      SiliconInitUpdOffset;

/** Offset 0x0018
**/
  UINT64                      ReservedUpd1;

/** Offset 0x0020
**/
  MEMORY_INIT_UPD             MemoryInitUpd;

/** Offset 0x0200
**/
  SILICON_INIT_UPD            SiliconInitUpd;

/** Offset 0x03FA - RegionTerminator
  This field is not an option and is a termination field at the end of the data structure. This field is will have a value 0x55AA indicating the end of UPD data.The boot loader should never override this field.
**/
  UINT16                      RegionTerminator;
} UPD_DATA_REGION;

#define FSP_IMAGE_ID    0x305053462D4B5551	/* 'QUK-FSP0' */
#define FSP_IMAGE_REV   0x00000000		/* 0.0 */

/** VPD data structure
**/
typedef struct {

/** Offset 0x0000
**/
  UINT64                      PcdVpdRegionSign;

/** Offset 0x0008 - PcdImageRevision
  This field is not an option and is a revision ID for the FSP release. It can be used by the boot loader to validate the VPD/UPD region. If the value in this field is changed for an FSP release, the boot loader should not assume the same layout for the UPD_DATA_REGION/VPD_DATA_REGION data structure. Instead it should use the new FspUpdVpd.h from the FSP release package.  This should match the ImageRevision in FSP_INFO_HEADER.
**/
  UINT32                      PcdImageRevision;

/** Offset 0x000C - PcdUpdRegionOffset
  This field is not an option and contains the offset of the UPD data region within the FSP release image. The boot loader can use it to find the location of UPD_DATA_REGION.
**/
  UINT32                      PcdUpdRegionOffset;
} VPD_DATA_REGION;

#pragma pack(pop)

#endif