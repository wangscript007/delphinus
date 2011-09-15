/*
 *  TsFile.cpp - An abstraction for handling the raw Transport Stream
 *
 *  This file is part of delphinus - a stream analyzer for various multimedia
 *  stream formats and containers.
 *
 *  Copyright (C) 2011 Ashwin (Tuxdude)
 *  tuxdude@users.sourceforge.net
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "TsFile.h"
#include <cassert>

//#define DEBUG

#ifdef DEBUG
#define MSG(x, ...); ::fprintf(stderr, " " x " \n", ##__VA_ARGS__);
#else
#define MSG(x, ...); 
#endif

#define ERR(x, ...); ::fprintf(stderr, " " x " \n", ##__VA_ARGS__);

uint64_t readFile(uint8_t* buffer, FILE* fileHandle, uint64_t size);

uint64_t readFile(uint8_t* buffer, FILE* fileHandle, uint64_t size)
{
    uint64_t readSize = fread(buffer, size, 1, fileHandle);
    MSG("Only read: %lu", readSize);
    if (readSize == 1)
    {
        return size;
    }
    else if (!feof(fileHandle))
    {
        MSG("Doing partial file read");
        readSize = fread(buffer, 1, size, fileHandle);
        MSG("Now read: %lu", readSize);
        return readSize;
    }
    else
    {
        return 0;
    }
}

void TsFile::readFromOffset(uint64_t offset)
{
    MSG("Gonna read from offset: %lu", offset);
    if (!fseeko(fileHandle, offset, SEEK_SET))
    {
        validBufferSize = readFile(buffer, fileHandle, BUFFER_SIZE);
        currentFileOffset = offset;
        //FIXME: Handle the EOF case
        assert(validBufferSize > 0);
    }
    else
    {
        assert(false);
    }
}

void TsFile::validate()
{
    readFromOffset(0);
    TsPacket* tsPacket = new TsPacket();
    uint64_t bufferOffset = 0;
    packetSize = 0;
    isTsFile = false;

    if (!tsPacket->parse(buffer + bufferOffset, validBufferSize - bufferOffset))
    {
        delete tsPacket;
        return;
    }

    packetSize = tsPacket->getPacketSize();
    uint64_t maxValidBufferOffset = VALID_PACKETS * packetSize;
    assert (maxValidBufferOffset <= BUFFER_SIZE);
    bufferOffset += packetSize;

    while (bufferOffset < maxValidBufferOffset)
    {
        if (!tsPacket->parse(buffer + bufferOffset, validBufferSize - bufferOffset))
        {
            delete tsPacket;
            return;
        }
        bufferOffset += packetSize;
    }
    isTsFile = true;
    delete tsPacket;
}

TsFile::TsFile()
    :   
        buffer(NULL),
        fileHandle(NULL),
        viewPacket(NULL),
        fileSize(0),
        validBufferSize(0),
        currentFileOffset(0),
        lastPacketOffset((uint64_t) - 1),
        packetSize(0),
        isTsFile(false)
{
    buffer = new uint8_t[BUFFER_SIZE];
    assert(buffer != NULL);
    viewPacket = new TsPacket();
    assert(viewPacket != NULL);
}

TsFile::~TsFile()
{
    close();
    if (buffer)
    {
        delete buffer;
        buffer = NULL;
    }
    if (viewPacket)
    {
        delete viewPacket;
        viewPacket = NULL;
    }
}

bool TsFile::open(const char* fileName)
{
    close();
    fileHandle = fopen(fileName, "rb");
    if (fileHandle == NULL)
    {
        return false;
    }
    fseeko(fileHandle, 0, SEEK_END);
    fileSize = ftello(fileHandle);
    fseeko(fileHandle, 0, SEEK_SET);
    lastPacketOffset = fileSize - packetSize;

    validate();

    return true;
}

void TsFile::close()
{
    if (fileHandle)
    {
        fclose(fileHandle);
        fileHandle = NULL;
        fileSize = 0;
    }
}

bool TsFile::isValid()
{
    return isTsFile; 
}

TsPacket* TsFile::viewPacketByNumber(uint64_t packetNumber)
{
    uint64_t packetOffset = packetNumber * packetSize;
    uint64_t bufferOffset = packetOffset % BUFFER_SIZE;
    if (packetOffset >= fileSize)
    {
        // invalid packet number - exceeds file size
        return NULL;
    }
    if (currentFileOffset > packetOffset ||
        currentFileOffset + BUFFER_SIZE < packetOffset)
    {
        readFromOffset(packetOffset - bufferOffset);
    }

    MSG("Returning packet %lu from buffer offset: %lu", packetNumber, bufferOffset);
    viewPacket->parse(buffer + bufferOffset, validBufferSize - bufferOffset);
    lastPacketOffset = packetOffset;
    return viewPacket;
}

TsPacket* TsFile::viewNextPacket()
{
    uint64_t packetOffset = (uint64_t) - 1;
    if (lastPacketOffset == (uint64_t) - 1)
    {
        packetOffset = 0;
    }
    else if (lastPacketOffset + packetSize >= fileSize)
    {
        // reached EOF
        return NULL;
    }
    
    packetOffset = lastPacketOffset + packetSize;
    uint64_t bufferOffset = packetOffset % BUFFER_SIZE;
    if (bufferOffset == 0)
    {
        // We do not have this packet in the buffer yet
        readFromOffset(packetOffset);
    }
    MSG("Returning packet from buffer offset: %lu", bufferOffset);
    viewPacket->parse(buffer + bufferOffset, validBufferSize - bufferOffset);
    lastPacketOffset = packetOffset;
    return viewPacket;
}

TsPacket* TsFile::viewPreviousPacket()
{
    uint64_t packetOffset = (uint64_t) - 1;
    if (lastPacketOffset == (uint64_t) - 1)
    {
        packetOffset = 0;
    }
    else if (lastPacketOffset - packetSize >= fileSize)
    {
        // This case wont be hit - if we do then detonate
        assert(false);
        return NULL;
    }
    
    packetOffset = lastPacketOffset - packetSize;
    uint64_t bufferOffset = packetOffset % BUFFER_SIZE;
    if (bufferOffset == (uint64_t)(BUFFER_SIZE - packetSize))
    {
        // We do not have this packet in the buffer yet
        readFromOffset(packetOffset - bufferOffset);
    }
    viewPacket->parse(buffer + bufferOffset, validBufferSize - bufferOffset);
    lastPacketOffset = packetOffset;
    return viewPacket;
}

