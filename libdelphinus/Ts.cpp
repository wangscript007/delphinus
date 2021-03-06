/*
 *  Ts.cpp - declaration of types related to transport stream header
 *  and the fields within as specified in the ISO 13818-1 document
 *
 *  This file is part of delphinus.
 *
 *  Copyright (C) 2012 Ash (Tuxdude) <tuxdude.github@gmail.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as
 *  published by the Free Software Foundation; either version 3 of the
 *  License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this program.  If not, see
 *  <http://www.gnu.org/licenses/>.
 */

#include "Ts.h"
#include <cstdio>

using namespace MpegConstants;

//#define DEBUG

#define MODULE_TS 1
#define CURRENT_MODULE MODULE_TS

#ifdef DEBUG
#define MSG(fmt, ...); LogOutput(CURRENT_MODULE, DelphinusUtils::LOG_INFO, " " fmt " \n", ##__VA_ARGS__);
#else
#define MSG(fmt, ...);
#endif

#define ERR(fmt, ...); LogOutput(CURRENT_MODULE, DelphinusUtils::LOG_ERROR, " " fmt " \n", ##__VA_ARGS__);

TsPacket::TsPacket()
    :   start(NULL),
        startOffset(0),
        packetSize(0),
        adaptationFieldOffset(0),
        payloadOffset(0),
        isMemoryAllocated(false),
        isValid(false)
{

}

TsPacket::~TsPacket()
{
    if (isMemoryAllocated && start)
    {
        delete[] start;
    }
}

bool TsPacket::parse(uint8_t* data, uint64_t size)
{
    if (isMemoryAllocated && start)
    {
        delete start;
        isMemoryAllocated = false;
    }
    start = data;
    startOffset = 0;
    packetSize = 0;
    adaptationFieldOffset = 0;
    payloadOffset = 0;
    isValid = false;

    if (size < PACKET_SIZE_TS)
    {
        // We need at least one whole TS packet
        isValid = false;
    }
    else
    {
        if (getSyncByte() == 0x47)
        {
            packetSize = PACKET_SIZE_TS;
            isValid = true;
        }
        else
        {
            // Check if it is TTS
            startOffset = 4;
            if (getSyncByte() != 0x47)
            {
                ERR("Unable to find the sync byte 0x47!");
                isValid = false;
            }
            else
            {
                packetSize = PACKET_SIZE_TTS;
                isValid = true;
            }
        }
    }

    if (isValid)
    {
        uint8_t adaptationFieldLength = 0;
        if (hasAdaptationField())
        {
            adaptationFieldOffset = startOffset + 4;
            adaptationFieldLength = *(start + adaptationFieldOffset);
        }
        else
        {
            adaptationFieldOffset = 0;
            adaptationFieldLength = 0;
        }
        if (hasPayload())
        {
            if (adaptationFieldLength == 0)
            {
                payloadOffset = startOffset + 4;
            }
            else
            {
                payloadOffset = adaptationFieldOffset + adaptationFieldLength + 1;
            }
        }

        MSG("First 4 bytes of the TS: %02x %02x %02x %02x",
            start[0], start[1], start[2], start[3]);
    }
    return isValid;
}

TsPacket* TsPacket::copy()
{
    if (start && isValid)
    {
        TsPacket* tsPacket = new TsPacket();
        uint8_t* copiedData = new uint8_t[packetSize];
        tsPacket->parse(copiedData, packetSize);
        tsPacket->isMemoryAllocated = true;
        return tsPacket;
    }
    else
    {
        return NULL;
    }
}

