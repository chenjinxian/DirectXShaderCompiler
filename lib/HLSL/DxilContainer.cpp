///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// DxilContainer.cpp                                                         //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
// Provides support for manipulating DXIL container structures.              //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "dxc/HLSL/DxilContainer.h"
#include <algorithm>

namespace hlsl {

DxilPartIterator begin(const DxilContainerHeader *pHeader) {
  return{ pHeader, 0 };
}

DxilPartIterator end(const DxilContainerHeader *pHeader) {
  return{ pHeader, pHeader->PartCount };
}

void InitDxilContainer(_Out_ DxilContainerHeader *pHeader, uint32_t partCount,
                       uint32_t containerSizeInBytes) {
  memset(pHeader, 0, sizeof(*pHeader));
  pHeader->HeaderFourCC = DFCC_Container;
  pHeader->Version.Major = DxilContainerVersionMajor;
  pHeader->Version.Minor = DxilContainerVersionMinor;
  pHeader->PartCount = partCount;
  pHeader->ContainerSizeInBytes = containerSizeInBytes;
}

const DxilContainerHeader *IsDxilContainerLike(const void *ptr, size_t length) {
  if (ptr == nullptr || length < 4)
    return nullptr;
  if (DFCC_Container != *reinterpret_cast<const uint32_t *>(ptr))
    return nullptr;
  return reinterpret_cast<const DxilContainerHeader *>(ptr);
}

bool IsValidDxilContainer(const DxilContainerHeader *pHeader, size_t length) {
  // Validate that the header is where it's supposed to be.
  if (pHeader == nullptr) return false;
  if (length < sizeof(DxilContainerHeader)) return false;

  // Validate the header values.
  if (pHeader->HeaderFourCC != DFCC_Container) return false;
  if (pHeader->Version.Major != DxilContainerVersionMajor) return false;
  if (pHeader->ContainerSizeInBytes > length) return false;
  if (pHeader->ContainerSizeInBytes > DxilContainerMaxSize) return false;

  // Make sure that the count of offsets fits.
  size_t partOffsetTableBytes = sizeof(uint32_t) * pHeader->PartCount;
  if (partOffsetTableBytes + sizeof(DxilContainerHeader) >
      pHeader->ContainerSizeInBytes)
    return false;

  // Make sure that each part is within the bounds.
  const uint8_t *pLinearContainer = reinterpret_cast<const uint8_t *>(pHeader);
  const uint32_t *pPartOffsetTable =
      reinterpret_cast<const uint32_t *>(pHeader + 1);
  for (uint32_t i = 0; i < pHeader->PartCount; ++i) {
    // The part header should fit.
    if (pPartOffsetTable[i] >
        (pHeader->ContainerSizeInBytes - sizeof(DxilPartHeader)))
      return false;

    // The contents of the part should fit.
    const DxilPartHeader *pPartHeader =
        reinterpret_cast<const DxilPartHeader *>(pLinearContainer +
                                                 pPartOffsetTable[i]);
    if (pPartOffsetTable[i] + sizeof(DxilPartHeader) + pPartHeader->PartSize >
        pHeader->ContainerSizeInBytes) {
      return false;
    }
  }

  // Note: the container parts may overlap and there may be holes
  // based on this validation

  return true;
}

const DxilPartHeader *GetDxilPartByType(const DxilContainerHeader *pHeader, DxilFourCC fourCC) {
  if (!IsDxilContainerLike(pHeader, pHeader->ContainerSizeInBytes)) {
    return nullptr;
  }
  const DxilPartIterator partIter =
      find_if(begin(pHeader), end(pHeader), DxilPartIsType(fourCC));
  if (partIter == end(pHeader)) {
    return nullptr;
  }
  return *partIter;
}

DxilPartHeader *GetDxilPartByType(DxilContainerHeader *pHeader,
                                  DxilFourCC fourCC) {
  return const_cast<DxilPartHeader *>(GetDxilPartByType(
      static_cast<const DxilContainerHeader *>(pHeader), fourCC));
}

const DxilProgramHeader *GetDxilProgramHeader(const DxilContainerHeader *pHeader, DxilFourCC fourCC) {
  if (!IsDxilContainerLike(pHeader, pHeader->ContainerSizeInBytes)) {
    return nullptr;
  }
  const DxilPartHeader *PartHeader = GetDxilPartByType(pHeader, fourCC);
  if (!PartHeader) {
    return nullptr;
  }
  const DxilProgramHeader *ProgramHeader =
      reinterpret_cast<const DxilProgramHeader *>(GetDxilPartData(PartHeader));
  return IsValidDxilProgramHeader(ProgramHeader,
                                  ProgramHeader->SizeInUint32 * 4)
             ? ProgramHeader
             : nullptr;
}

DxilProgramHeader *GetDxilProgramHeader(DxilContainerHeader *pHeader, DxilFourCC fourCC) {
  return const_cast<DxilProgramHeader *>(
      GetDxilProgramHeader(static_cast<const DxilContainerHeader *>(pHeader), fourCC));
}

} // namespace hlsl
