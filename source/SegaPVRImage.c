//
//  SegaPVRImage.c
//  SEGAPVR
//
//  Created by Yevgeniy Logachev on 4/13/14.
//  Copyright (c) 2014 yev. All rights reserved.
//

#include "SegaPVRImage.h"
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <math.h>
#include <stdint.h>

// Twiddle
#define kTwiddleTableSize 1024
unsigned long int gTwiddledTable[kTwiddleTableSize];

unsigned long int GetUntwiddledTexelPosition(unsigned long int x, unsigned long int y);
uint32_t get_untwiddled_index(uint16_t w, uint16_t h, uint32_t p);

int MipMapsCountFromWidth(unsigned long int width);

// RGBA Utils
void TexelToRGBA(unsigned short int srcTexel, enum TextureFormatMasks srcFormat, uint8_t* rgba);

unsigned int ToUint16(unsigned char* value)
{
	return (value[0] | (value[1] << 8));
}


bool LoadPVRFromFile(const char* filename, unsigned char** image, unsigned long int* imageSize, struct PVRTHeader* outPvrtHeader)
{
	FILE* pFile = fopen(filename, "rb");
	if (pFile == 0) {
		return false;
	}
	
	fseek(pFile, 0, SEEK_END); // seek to end of file
	size_t fsize = ftell(pFile); // get current file pointer
	fseek(pFile, 0, SEEK_SET);
	
	unsigned char* buff = (unsigned char*)malloc(fsize);
	fread(buff, fsize, 1, pFile);
	fclose(pFile);
	
	unsigned char* srcPtr = buff;
	
	struct PVRTHeader pvrtHeader;
	unsigned int offset = ReadPVRHeader(srcPtr, &pvrtHeader);
	if (offset == 0)
	{
		free(buff);
		return false;
	}

	if (outPvrtHeader)
	{
		*outPvrtHeader = pvrtHeader;
	}
	
	printf("Image size: (%d, %d)\n", pvrtHeader.width, pvrtHeader.height);
	
	srcPtr += offset;
	
	*imageSize = pvrtHeader.width * pvrtHeader.height * 4; // RGBA8888
	*image = (unsigned char *)malloc(*imageSize);
	memset(*image, 0, *imageSize);
	
	DecodePVR(srcPtr, &pvrtHeader, *image);

	free(buff);
	
	return true;
}

unsigned int ReadPVRHeader(unsigned char* srcData, struct PVRTHeader* pvrtHeader)
{
	unsigned int offset = 0;
	struct GBIXHeader gbixHeader;
	if (strncmp((char*)srcData, "GBIX", 4) == 0)
	{
		memcpy(&gbixHeader, srcData, sizeof(struct GBIXHeader));
		offset += sizeof(struct GBIXHeader);
	}
	
	if (strncmp((char*)srcData + offset, "PVRT", 4) == 0)
	{
		memcpy(pvrtHeader, srcData + offset, sizeof(struct PVRTHeader));
		offset += sizeof(struct PVRTHeader);
	}
	
	return offset;
}

bool DecodePVR(unsigned char* srcData, const struct PVRTHeader* pvrtHeader, unsigned char* dstData)
{
	const unsigned int kSrcStride = sizeof(unsigned short int);
	const unsigned int kDstStride = sizeof(unsigned int); // RGBA8888
	
	enum TextureFormatMasks srcFormat = (enum TextureFormatMasks)(pvrtHeader->textureAttributes & 0xFF);
	
	// Unpack data
	bool isTwiddled = false;
	bool isMipMaps = false;
	bool isVQCompressed = false;
	unsigned int codeBookSize = 0;
	
	switch((pvrtHeader->textureAttributes >> 8) & 0xFF)
	{
		case TTM_TwiddledMipMaps:
			isMipMaps = true;
		case TTM_Twiddled:
		case TTM_TwiddledNonSquare:
			isTwiddled = true;
			break;
			
		case TTM_VectorQuantizedMipMaps:
			isMipMaps = true;
		case TTM_VectorQuantized:
			isVQCompressed = true;
			codeBookSize = 256;
			break;
			
		case TTM_VectorQuantizedCustomCodeBookMipMaps:
			isMipMaps = true;
		case TTM_VectorQuantizedCustomCodeBook:
			isVQCompressed = true;
			codeBookSize = pvrtHeader->width;
			if(codeBookSize < 16)
			{
				codeBookSize = 16;
			}
			else if(codeBookSize == 64)
			{
				codeBookSize = 128;
			}
			else
			{
				codeBookSize = 256;
			}
			break;
			
		case TTM_Raw:
		case TTM_RawNonSquare:
			break;
			
		default:
			return false;
			break;
	}
	
	const unsigned int numCodedComponents = 4;
	unsigned char* srcVQ = 0;
	if (isVQCompressed)
	{
		srcVQ = srcData;
		srcData += numCodedComponents * kSrcStride * codeBookSize;
	}
	
	unsigned int mipWidth = 0;
	unsigned int mipHeight = 0;
	unsigned int mipSize = 0;
	// skip mipmaps
	int mipMapCount = (isMipMaps) ? MipMapsCountFromWidth(pvrtHeader->width) : 1;
	while (mipMapCount)
	{
		mipWidth = (pvrtHeader->width >> (mipMapCount - 1));
		mipHeight = (pvrtHeader->height >> (mipMapCount - 1));
		mipSize = mipWidth * mipHeight;
		
		if (--mipMapCount > 0)
		{
			if (isVQCompressed)
			{
				srcData += mipSize / 4;
			}
			else
			{
				srcData += kSrcStride * mipSize;
			}
		}
		else if (isMipMaps)
		{
			srcData += (isVQCompressed) ? 1 : kSrcStride;  // skip 1x1 mip
		}
	}
	
	// Compressed textures processes only half-size
	if (isVQCompressed)
	{
		mipWidth /= 2;
		mipHeight /= 2;
		mipSize = mipWidth * mipHeight;
	}
	
	puts("extracting image data");
	//extract image data
	unsigned long int x = 0;
	unsigned long int y = 0;
	
	unsigned int processed = 0;
	while(processed < mipSize)
	{
		unsigned long int srcPos = 0;
		unsigned long int dstPos = 0;
		unsigned short int srcTexel = 0;
		
		if (isVQCompressed)
		{
			unsigned long int vqIndex = srcData[GetUntwiddledTexelPosition(x, y)] * numCodedComponents; // Index of codebook * numbers of 2x2 block components
			// Bypass elements in 2x2 block
			for (unsigned int yoffset = 0; yoffset < 2; ++yoffset)
			{
				for (unsigned int xoffset = 0; xoffset < 2; ++xoffset)
				{   
					srcPos = (vqIndex + (xoffset * 2 + yoffset)) * kSrcStride;
					srcTexel = ToUint16(&srcVQ[srcPos]);
					
					dstPos = ((y * 2 + yoffset) * 2 * mipWidth + (x * 2 + xoffset)) * kDstStride;
					TexelToRGBA(srcTexel, srcFormat, &dstData[dstPos]);
				}
			}
			
			if (++x >= mipWidth)
			{
				x = 0;
				++y;
			}
		}
		else
		{
			x = processed % mipWidth;
			y = processed / mipWidth;
			
			srcPos = processed * kSrcStride;
			srcTexel = ToUint16(&srcData[srcPos]);
			
			if(isTwiddled) {
				dstPos = get_untwiddled_index(pvrtHeader->width,pvrtHeader->height,processed) * kDstStride;
			} else {
				dstPos = processed * kDstStride;
			}
			TexelToRGBA(srcTexel, srcFormat, &dstData[dstPos]);
		}
		
		++processed;
	}
	
	return true;
}

int MipMapsCountFromWidth(unsigned long int width)
{
	unsigned int mipMapsCount = 0;
	while( width )
	{
		++mipMapsCount;
		width /= 2;
	}
	
	return mipMapsCount;
}

// Twiddle
unsigned long int UntwiddleValue(unsigned long int value)
{
	unsigned long int untwiddled = 0;
	
	for (size_t i = 0; i < 10; i++)
	{
		unsigned long int shift = pow(2, i);
		if (value & shift) untwiddled |= (shift << i);
	}
	
	return untwiddled;
}

void BuildTwiddleTable()
{
	for( unsigned long int i = 0; i < kTwiddleTableSize; i++ )
	{
		gTwiddledTable[i] = UntwiddleValue( i );
	}
}

unsigned long int GetUntwiddledTexelPosition(unsigned long int x, unsigned long int y)
{
	unsigned long int pos = 0;
	
	if(x >= kTwiddleTableSize || y >= kTwiddleTableSize)
	{
		pos = UntwiddleValue(y)  |  UntwiddleValue(x) << 1;
	}
	else
	{
		pos = gTwiddledTable[y]  |  gTwiddledTable[x] << 1;
	}
	
	return pos;
}

uint32_t get_untwiddled_index(uint16_t w, uint16_t h, uint32_t p) {
	uint32_t ddx = 1, ddy = w;
	uint32_t q = 0;

	for(int i = 0; i < 16; i++){
		if(h >>= 1){
			if(p & 1){q |= ddy;}
			p >>= 1;
		}
		ddy <<= 1;
		if(w >>= 1){
			if(p & 1){q |= ddx;}
			p >>= 1;
		}
		ddx <<= 1;
	}

	return q;
}

void TexelToRGBA(const uint16_t srcTexel, enum TextureFormatMasks srcFormat, uint8_t* rgba) {
	uint8_t* r = &rgba[0];
	uint8_t* g = &rgba[1];
	uint8_t* b = &rgba[2];
	uint8_t* a = &rgba[3];

	switch( srcFormat ) {
		case TFM_RGB565:
			*a = 0xFF;
			*r = (0x1F&(srcTexel>>11)) << 3;
			*g = (0x3F&(srcTexel>>5)) << 2;
			*b = (0x1F&(srcTexel>>0)) << 3;
			break;
			
		case TFM_ARGB1555:
			*a = (srcTexel & 0x8000) ? 0xFF : 0x00;
			*r = (srcTexel & 0x7C00) >> 7;
			*g = (srcTexel & 0x03E0) >> 2;
			*b = (srcTexel & 0x001F) << 3;
			break;
			
		case TFM_ARGB4444:
			*a = (srcTexel & 0xF000) >> 8;
			*r = (srcTexel & 0x0F00) >> 4;
			*g = (srcTexel & 0x00F0);
			*b = (srcTexel & 0x000F) << 4;
			break;
			
		case TFM_YUV422: // wip
			*a = 0xFF;
			*r = 0xFF;
			*g = 0xFF;
			*b = 0xFF;
			break;
		default:
			puts("format not supported ^^");
			break;
	}
}
