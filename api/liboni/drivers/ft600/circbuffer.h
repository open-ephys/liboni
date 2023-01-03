#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

//This buffer and its functions should be thread safe as long as only one thread
//writes and only one thread reads
typedef struct {
	size_t read;
	size_t write;
	size_t size;
	uint8_t* buffer;
} circ_buffer_t;

int circBufferInit(circ_buffer_t* buffer);
void circBufferRelease(circ_buffer_t* buffer);
int circBufferCanWrite(circ_buffer_t* buffer, size_t size);
int circBufferCanRead(circ_buffer_t* buffer, size_t size);

//These do not check. Check before calling them
void circBufferWrite(circ_buffer_t* buffer, uint8_t* src, size_t size);
void circBufferRead(circ_buffer_t* buffer, uint8_t* dst, size_t size);