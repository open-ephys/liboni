#include "circbuffer.h"

int circBufferInit(circ_buffer_t* buffer)
{
	if (buffer->size == 0) return 0;
	buffer->buffer = malloc(buffer->size);
	if (buffer->buffer == NULL) return 0;
	buffer->read = 0;
	buffer->write = 0;
	return 1;
}

void circBufferRelease(circ_buffer_t* buffer)
{
	if (buffer->buffer != NULL) free(buffer->buffer);
}
int circBufferCanWrite(circ_buffer_t* buffer, size_t size)
{
	size_t rpointer = buffer->read;
	size_t wpointer = buffer->write;
	size_t rem = (wpointer < rpointer) ? rpointer - wpointer : buffer->size - (wpointer - rpointer);
	return (rem > size) ? 1 : 0;
}
int circBufferCanRead(circ_buffer_t* buffer, size_t size)
{
	size_t wpointer = buffer->write;
	size_t rpointer = buffer->read;
	size_t num = (wpointer >= rpointer) ? wpointer - rpointer : buffer->size - (rpointer - wpointer);
	return (num >= size) ? 1 : 0;
}
//These do not check. Check before calling them
void circBufferWrite(circ_buffer_t* buffer, uint8_t* src, size_t size)
{
	size_t wpointer = buffer->write;
	size_t toWrite = (size + wpointer) > buffer->size ? buffer->size - wpointer : size;
	memcpy(buffer->buffer + wpointer, src, toWrite);
	if (toWrite < size)
	{
		memcpy(buffer->buffer, src + toWrite, size - toWrite);
	}
	wpointer = (wpointer + size) % buffer->size;
	buffer->write = wpointer;
}
void circBufferRead(circ_buffer_t* buffer, uint8_t* dst, size_t size)
{
	size_t rpointer = buffer->read;
	size_t toRead = (size + rpointer) > buffer->size ? buffer->size - rpointer : size;
	memcpy(dst, buffer->buffer + rpointer, toRead);
	if (toRead < size)
	{
		memcpy(dst + toRead, buffer->buffer, size - toRead);
	}
	rpointer = (rpointer + size) % buffer->size;
	buffer->read = rpointer;
}
