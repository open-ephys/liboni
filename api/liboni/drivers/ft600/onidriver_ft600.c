#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "../../onidriver.h"
#include "circbuffer.h"

#ifdef _WIN32
#include<Windows.h>
#define DEFAULT_OVERLAPPED 4
#define SERIAL_LEN 16
#define FTD3XX_STATIC
#else
#include <unistd.h>
#include <pthread.h>
#define Sleep(x) usleep((x)*1000)
#define TRUE true
#define FALSE false
#define SERIAL_LEN 32
#endif
#include <FTD3XX.h>

#define DEFAULT_AUXSIZE 8192
#define DEFAULT_BLOCKREAD 4096
#define DEFAULT_BLOCKWRITE 4096
#define DEFAULT_SIGNALSIZE 8192
#define DEFAULT_REGSIZE 512
#define DEFAULT_PRIO 0x0A0A0000

#define UNUSED(x) (void)(x)
#define CMD_WRITEREG 0x01
#define CMD_READREG 0x02
#define CMD_SETPRIO 0x03
#define SIGRES_SIGNAL 0x01
#define SIGRES_REG 0x02

const char usbdesc[] = "Open Ephys FT600 USB board";

// NB: To save some repetition
#define CTX_CAST const oni_ft600_ctx ctx = (oni_ft600_ctx)driver_ctx
#define MIN(a,b) ((a<b) ? a : b)
#define CHECK_FTERR(exp) {if(exp!=FT_OK){oni_ft600_free_ctx(ctx);return ONI_EINIT;}}
#define CHECK_NULL(exp) {if(exp==NULL){oni_ft600_free_ctx(ctx);return ONI_EBADALLOC;}}
#define CHECK_ERR(exp) {if(exp==0){oni_ft600_free_ctx(ctx);return ONI_EINIT;}}

typedef enum 
{
	STATE_NOINIT = 0,
	STATE_INIT,
	STATE_RUNNING
} oni_ft600_state;

typedef enum
{
	SIG_CMD = 0,
	SIG_SIGNAL,
	SIG_REG
} oni_ft600_sigstate;

const oni_driver_info_t driverInfo
    = {.name = "ft600", .major = 1, .minor = 0, .patch = 0, .pre_relase = NULL};

struct oni_ft600_ctx_impl {
	oni_size_t inBlockSize;
	oni_size_t outBlockSize;
	FT_HANDLE ftHandle;
	circ_buffer_t signalBuffer;
	circ_buffer_t regBuffer;
	uint8_t* auxBuffer;
	size_t auxSize;
	uint32_t prioValue;
	oni_ft600_state state;
	oni_ft600_sigstate sigState;
	unsigned short sigOffset;
	unsigned short sigError;
	unsigned int nextReadIndex;
	size_t lastReadOffset;
	size_t lastReadRead;
#ifdef _WIN32
	uint8_t* inBuffer;
	unsigned int numInOverlapped;
	OVERLAPPED cmdOverlapped;
	OVERLAPPED sigOverlapped;
	OVERLAPPED outOverlapped;
	OVERLAPPED* inOverlapped;
	ULONG* inTransferred;
#else
	pthread_mutex_t controlMutex;
#endif
	struct {
		oni_dev_idx_t dev_idx;
		oni_reg_addr_t dev_addr;
		oni_reg_val_t value;
		oni_reg_val_t rw;
	} regOperation;
};

typedef struct oni_ft600_ctx_impl* oni_ft600_ctx;

// Configuration file offsets
typedef enum oni_conf_reg_off {
	// Register R/W interface
	CONFDEVIDXOFFSET = 0, // Configuration device index register byte offset
	CONFADDROFFSET = 1, // Configuration register address register byte offset
	CONFVALUEOFFSET = 2, // Configuration register value register byte offset
	CONFRWOFFSET = 3, // Configuration register read/write register byte offset
	CONFTRIGOFFSET = 4, // Configuration read/write trigger register byte offset

	// Global configuration
	CONFRUNNINGOFFSET = 5, // Configuration run register byte offset
	CONFRESETOFFSET = 6, // Configuration reset register byte offset
	CONFSYSCLKHZOFFSET = 7, // Configuration base system clock frequency register byte offset
	CONFACQCLKHZOFFSET = 8, // Configuration frame counter clock frequency register byte offset
	CONFRESETACQCOUNTER = 9, // Configuration frame counter clock reset register byte offset
	CONFHWADDRESS = 10 // Configuration hardware address register byte offset

} oni_conf_off_t;

enum ft600_pipes
{
	pipe_in = 0x82,
	pipe_out = 0x02,
	pipe_sig = 0x83,
	pipe_cmd = 0x03
};

#ifndef _WIN32
enum ft600_pipeid
{
	pipeid_data = 0,
	pipeid_control = 1
};
#endif

inline void fill_control_buffers(oni_ft600_ctx ctx, ULONG transferred)
{
	size_t index = 0;
	size_t lastIndex = 0;
	size_t toWrite = 0;
	do {
		switch (ctx->sigState)
		{
		case SIG_CMD:
			if (ctx->auxBuffer[index] == SIGRES_SIGNAL)
				ctx->sigState = SIG_SIGNAL;
			else if (ctx->auxBuffer[index] == SIGRES_REG)
				ctx->sigState = SIG_REG;
			index++;
			break;
		case SIG_SIGNAL:
			lastIndex = index;
			while (index < transferred && ctx->auxBuffer[index] != 0) { /*printf("ii %ld t %d\n",index,transferred);*/ index++; }
			if (index >= transferred) index--;
			while (!circBufferCanWrite(&ctx->signalBuffer, index - lastIndex + 1));
			circBufferWrite(&ctx->signalBuffer, ctx->auxBuffer + lastIndex, index - lastIndex + 1);
			if (index < transferred && ctx->auxBuffer[index] == 0) ctx->sigState = SIG_CMD;
			index++;
			break;
		case SIG_REG:
			toWrite = MIN(sizeof(oni_reg_val_t), transferred - index);
			while (!circBufferCanWrite(&ctx->regBuffer, toWrite));
			circBufferWrite(&ctx->regBuffer, ctx->auxBuffer + index, toWrite);
			ctx->sigOffset = (ctx->sigOffset + toWrite) % sizeof(oni_reg_val_t);
			index += toWrite;
			if (ctx->sigOffset == 0) ctx->sigState = SIG_CMD;
			break;
		}

	} while (index < transferred);
}

#ifdef _WIN32
void oni_ft600_usb_callback(PVOID context, E_FT_NOTIFICATION_CALLBACK_TYPE type, PVOID cinfo)
{
	FT_STATUS ftStatus;
	if (type != E_FT_NOTIFICATION_CALLBACK_TYPE_DATA) return;
	FT_NOTIFICATION_CALLBACK_INFO_DATA* info = (FT_NOTIFICATION_CALLBACK_INFO_DATA*)cinfo;
	if (!info) return;
	oni_ft600_ctx ctx = (oni_ft600_ctx)context;
	ULONG transferred;
	
	

	FT_ReadPipe(ctx->ftHandle, info->ucEndpointNo, ctx->auxBuffer, info->ulRecvNotificationLength, &transferred, &ctx->sigOverlapped);
	ftStatus = FT_GetOverlappedResult(ctx->ftHandle, &ctx->sigOverlapped, &transferred, TRUE);
	if (ftStatus != FT_OK)
	{
		ctx->sigError = 1;
		return;
	}

	if (transferred > 0)
	{
		fill_control_buffers(ctx, transferred);
	}

}
#else
void oni_ft600_update_control(oni_ft600_ctx ctx)
{
	FT_STATUS ftStatus;
	DWORD toread = ctx->auxSize;
	ULONG transferred;
	if (pthread_mutex_trylock(&ctx->controlMutex) == 0)
	{
		ftStatus = FT_GetReadQueueStatus(ctx->ftHandle, pipeid_control, &toread);
		if (ftStatus != FT_OK)
		{
			ctx->sigError = 1;
            pthread_mutex_unlock(&ctx->controlMutex);
			return;
		}
        toread = MIN(toread,ctx->auxSize);
        if (toread > 0)
        {
            ftStatus = FT_ReadPipeEx(ctx->ftHandle, 1, ctx->auxBuffer, toread, &transferred, 5000);
            if (ftStatus != FT_OK && ftStatus != FT_TIMEOUT && ftStatus != FT_IO_PENDING)
            {
                ctx->sigError = 1;
                pthread_mutex_unlock(&ctx->controlMutex);
                return;
            }
            if (ftStatus == FT_OK && transferred > 0)
            {
                fill_control_buffers(ctx, transferred);
            }
        }
		pthread_mutex_unlock(&ctx->controlMutex);
	}
}

void flushPipe(oni_ft600_ctx ctx, int pipe)
{
    FT_STATUS ftStatus;
    unsigned char nullbuf[10];
    ULONG transferred;
    do 
    {
        ftStatus = FT_ReadPipeEx(ctx->ftHandle, pipe, nullbuf, 10, &transferred, 100);
        if (ftStatus != FT_OK && ftStatus != FT_TIMEOUT && ftStatus != FT_IO_PENDING) break;
    } while(transferred > 0);
}

#endif

static inline oni_conf_off_t _oni_register_offset(oni_config_t reg);

inline void oni_ft600_restart_acq(oni_ft600_ctx ctx)
{
	ctx->nextReadIndex = 0;
	ctx->lastReadOffset = 0;
	ctx->lastReadRead = 0;
	ctx->state = STATE_INIT;
}

inline void oni_ft600_reset_ctx(oni_ft600_ctx ctx)
{
	ctx->sigState = SIG_CMD;
	ctx->sigOffset = 0;
	ctx->sigError = 0;
	oni_ft600_restart_acq(ctx);
}

inline void oni_ft600_reset_acq(oni_ft600_ctx ctx, int hard)
{
	if (hard) {
		FT_WriteGPIO(ctx->ftHandle, 0x01, 0x01);
		Sleep(10);
	}
	FT_AbortPipe(ctx->ftHandle, pipe_in);
	FT_AbortPipe(ctx->ftHandle, pipe_out);
	FT_AbortPipe(ctx->ftHandle, pipe_cmd);
	FT_AbortPipe(ctx->ftHandle, pipe_sig);
#ifndef _WIN32
//TODO: fix with proper resets once we have the board
//    FT_FlushPipe(ctx->ftHandle, pipe_in);
//    FT_FlushPipe(ctx->ftHandle, pipe_out);
//    FT_FlushPipe(ctx->ftHandle, pipe_cmd);
//    FT_FlushPipe(ctx->ftHandle, pipe_sig);
#endif
	oni_ft600_reset_ctx(ctx);
	FT_WriteGPIO(ctx->ftHandle, 0x01, 0x00);
	Sleep(1);
}

inline void oni_ft600_stop_acq(oni_ft600_ctx ctx)
{
	Sleep(10);
#ifdef _WIN32
    FT_ClearStreamPipe(ctx->ftHandle, FALSE, FALSE, pipe_in);
    FT_AbortPipe(ctx->ftHandle, pipe_in);
#else
     FT_AbortPipe(ctx->ftHandle, pipe_in);
     flushPipe(ctx, pipeid_data);
#endif
	oni_ft600_restart_acq(ctx);
}

inline int oni_ft600_sendcmd(oni_ft600_ctx ctx, uint8_t* buffer, size_t size)
{
	ULONG transferred = 0;
	ULONG total = 0;
	FT_STATUS ftStatus;
	do
	{
#ifdef _WIN32
		FT_WritePipe(ctx->ftHandle, pipe_cmd, buffer + total, size - total, &transferred, &ctx->cmdOverlapped);
		ftStatus = FT_GetOverlappedResult(ctx->ftHandle, &ctx->cmdOverlapped, &transferred, TRUE);
		if (ftStatus != FT_OK) return ONI_ESEEKFAILURE;
#else
		ftStatus = FT_WritePipeEx(ctx->ftHandle, pipeid_control, buffer + total, size - total, &transferred, 1000);
		if (ftStatus != FT_OK && ftStatus != FT_TIMEOUT && ftStatus != FT_IO_PENDING) return ONI_ESEEKFAILURE;
#endif
		if (ftStatus == FT_OK) 
			total += transferred;
	} while (total < size);
	return ONI_ESUCCESS;
}

oni_driver_ctx oni_driver_create_ctx()
{
	oni_ft600_ctx ctx;
	ctx = calloc(1, sizeof(struct oni_ft600_ctx_impl));
	if (ctx == NULL)
		return NULL;
	ctx->ftHandle = NULL;
	oni_ft600_reset_ctx(ctx);
	ctx->state = STATE_NOINIT;
	ctx->auxSize = DEFAULT_AUXSIZE;
	ctx->inBlockSize = DEFAULT_BLOCKREAD;
	ctx->outBlockSize = DEFAULT_BLOCKWRITE;
	ctx->regBuffer.size = DEFAULT_REGSIZE;
	ctx->signalBuffer.size = DEFAULT_SIGNALSIZE;
	
#if _WIN32
	ctx->numInOverlapped = DEFAULT_OVERLAPPED;
#endif
	return ctx;
}

void oni_ft600_free_ctx(oni_ft600_ctx ctx)
{
#ifdef _WIN32
	if (ctx->inBuffer != NULL) free(ctx->inBuffer);
#endif
	if (ctx->auxBuffer != NULL) free(ctx->auxBuffer);
	circBufferRelease(&ctx->signalBuffer);
	circBufferRelease(&ctx->regBuffer);
	if (ctx->ftHandle != NULL)
	{
		FT_ClearNotificationCallback(ctx->ftHandle);
		FT_AbortPipe(ctx->ftHandle, pipe_in);
		FT_AbortPipe(ctx->ftHandle, pipe_out);
		FT_AbortPipe(ctx->ftHandle, pipe_cmd);
		FT_AbortPipe(ctx->ftHandle, pipe_sig);
#ifdef _WIN32
		FT_ReleaseOverlapped(ctx->ftHandle, &ctx->outOverlapped);
		FT_ReleaseOverlapped(ctx->ftHandle, &ctx->sigOverlapped);
		FT_ReleaseOverlapped(ctx->ftHandle, &ctx->cmdOverlapped);
		if (ctx->inOverlapped != NULL) {
			for (int i = 0; i < ctx->numInOverlapped; i++)
				FT_ReleaseOverlapped(ctx->ftHandle, &ctx->inOverlapped[i]);
			free(ctx->inOverlapped);
		}
		if (ctx->inTransferred != NULL) free(ctx->inTransferred);
#else
		pthread_mutex_destroy(&ctx->controlMutex);
#endif
		FT_Close(ctx->ftHandle);
		ctx->ftHandle = NULL;
	}
}

int oni_driver_init(oni_driver_ctx driver_ctx, int host_idx)
{
	CTX_CAST;
	DWORD numDevs = 0;
	FT_STATUS ftStatus;
	char serialNumber[SERIAL_LEN];
	int devIdx = 0;

	serialNumber[0] = 0;
	ftStatus = FT_CreateDeviceInfoList(&numDevs);
	if (ftStatus != FT_OK || numDevs == 0) return ONI_EDEVID;
	FT_DEVICE_LIST_INFO_NODE* devInfo = (FT_DEVICE_LIST_INFO_NODE*)malloc(sizeof(FT_DEVICE_LIST_INFO_NODE) * numDevs);
	if (devInfo == NULL) return ONI_EINIT;
	ftStatus = FT_GetDeviceInfoList(devInfo, &numDevs);
	if (ftStatus != FT_OK)
	{
		free(devInfo);
		return ONI_EINIT;
	}

	for (DWORD dev = 0; dev < numDevs; dev++)
	{
		if (strncmp(devInfo[dev].Description, usbdesc, 32) == 0)
		{
			if (!(devInfo[dev].Flags & FT_FLAGS_OPENED))
			{
				if (host_idx < 0 || devIdx == host_idx)
				{
					strncpy(serialNumber, devInfo[dev].SerialNumber, SERIAL_LEN);
					break;
				}
			}
			devIdx++;
		}
	}
	free(devInfo);
	if (serialNumber[0] == 0) return ONI_EDEVIDX;
#ifndef _WIN32
   FT_TRANSFER_CONF ftTransfer;
	memset(&ftTransfer, 0, sizeof(FT_TRANSFER_CONF));
	ftTransfer.wStructSize = sizeof(FT_TRANSFER_CONF);
	//The spec defines that only one simultaneous call to each pipe must be done, so disable built-in thred safety for performance
	ftTransfer.pipe[FT_PIPE_DIR_IN].fNonThreadSafeTransfer = FALSE;
	ftTransfer.pipe[FT_PIPE_DIR_OUT].fNonThreadSafeTransfer = FALSE;
	CHECK_FTERR(FT_SetTransferParams(&ftTransfer, pipeid_data));
	//And control is mutexed in the onidriver
	CHECK_FTERR(FT_SetTransferParams(&ftTransfer, pipeid_control));

	//unused pipes
	memset(&ftTransfer, 0, sizeof(FT_TRANSFER_CONF));
	ftTransfer.wStructSize = sizeof(FT_TRANSFER_CONF);
	ftTransfer.pipe[FT_PIPE_DIR_IN].fPipeNotUsed = TRUE;
	ftTransfer.pipe[FT_PIPE_DIR_OUT].fPipeNotUsed = TRUE;
	CHECK_FTERR(FT_SetTransferParams(&ftTransfer, 2));
	CHECK_FTERR(FT_SetTransferParams(&ftTransfer, 3));
#endif
    
	ftStatus = FT_Create(serialNumber, FT_OPEN_BY_SERIAL_NUMBER, &ctx->ftHandle);
	if (ftStatus != FT_OK)
	{
		ctx->ftHandle = NULL;
		return ONI_EINIT;
	}
#ifdef _WIN32
	FT_InitializeOverlapped(ctx->ftHandle, &ctx->outOverlapped);
	FT_InitializeOverlapped(ctx->ftHandle, &ctx->sigOverlapped);
	FT_InitializeOverlapped(ctx->ftHandle, &ctx->cmdOverlapped);
	ctx->inOverlapped = malloc(ctx->numInOverlapped * sizeof(OVERLAPPED));
	CHECK_NULL(ctx->inOverlapped);
	for (int i = 0; i < ctx->numInOverlapped; i++)
		FT_InitializeOverlapped(ctx->ftHandle, &ctx->inOverlapped[i]);
	ctx->inTransferred = malloc(ctx->numInOverlapped * sizeof(ULONG));
	CHECK_NULL(ctx->inTransferred);
	ctx->inBuffer = malloc(2 * (size_t)ctx->numInOverlapped * ctx->inBlockSize);
	CHECK_NULL(ctx->inBuffer);
#else
	if (pthread_mutex_init(&ctx->controlMutex, NULL) != 0)
	{
		oni_ft600_free_ctx(ctx);
		return ONI_EINIT;
	}
#endif
	CHECK_ERR(circBufferInit(&ctx->signalBuffer));
	CHECK_ERR(circBufferInit(&ctx->regBuffer));
	ctx->auxBuffer = malloc(ctx->auxSize);
	CHECK_NULL(ctx->auxBuffer);
    
#ifdef _WIN32
	CHECK_FTERR(FT_SetNotificationCallback(ctx->ftHandle, oni_ft600_usb_callback, ctx));
	//CHECK_FTERR(FT_SetStreamPipe(ctx->ftHandle, FALSE, FALSE, pipe_in, ctx->inBlockSize));
	CHECK_FTERR(FT_SetPipeTimeout(ctx->ftHandle, pipe_in, 0));
	CHECK_FTERR(FT_SetPipeTimeout(ctx->ftHandle, pipe_out, 0));
	CHECK_FTERR(FT_SetPipeTimeout(ctx->ftHandle, pipe_cmd, 0));
	CHECK_FTERR(FT_SetPipeTimeout(ctx->ftHandle, pipe_sig, 0));

#endif
	//Enable GPIOs
	CHECK_FTERR(FT_SetGPIOPull(ctx->ftHandle,0x3,0x05))
    CHECK_FTERR(FT_WriteGPIO(ctx->ftHandle, 0x01, 0x00));
	CHECK_FTERR(FT_EnableGPIO(ctx->ftHandle, 0x01, 0x01));
	//TODO: check that board is in normal mode and not bootloader
	//Hardware FIFO reset
	oni_ft600_reset_acq(ctx, 1);
	//Set io priority
	uint8_t buffer[5];
	buffer[0] = CMD_SETPRIO;
	*(uint32_t*)(buffer + 1) = DEFAULT_PRIO;
	int res = oni_ft600_sendcmd(ctx, buffer, 5);
	if (res != ONI_ESUCCESS)
	{
		oni_ft600_free_ctx(ctx);
		return res;
	}
	ctx->state = STATE_INIT;
	return ONI_ESUCCESS;
}

int oni_driver_destroy_ctx(oni_driver_ctx driver_ctx)
{
	CTX_CAST;
	assert(ctx != NULL && "Driver context is NULL");
	//Let's keep the device in a cool, reset state
	FT_WriteGPIO(ctx->ftHandle, 0x01, 0x01);
	oni_ft600_free_ctx(ctx);
	free(ctx);
	return ONI_ESUCCESS;
}

int oni_driver_read_stream(oni_driver_ctx driver_ctx,
	oni_read_stream_t stream,
	void* data,
	size_t size)
{
	CTX_CAST;
	if (stream == ONI_READ_STREAM_SIGNAL)
	{
		while (!circBufferCanRead(&ctx->signalBuffer, size))
		{
#ifndef _WIN32
			oni_ft600_update_control(ctx);
#endif 
			if (ctx->sigError)
			{
				ctx->sigError = 0;
				return ONI_ESEEKFAILURE;
			}
			Sleep(1);
		}
		circBufferRead(&ctx->signalBuffer, data, size);
		return size;
	} 
	else if (stream == ONI_READ_STREAM_DATA)
	{
		size_t remaining = ((size >> 2) << 2);//round to 32bit boundaries;
		FT_STATUS ftStatus;
		int read = 0;
		if (remaining < ctx->inBlockSize) return ONI_EINVALREADSIZE;
#ifdef _WIN32
		size_t to_read;
		uint8_t* dstPtr = data;
		uint8_t* srcPtr;
		unsigned int lastIndex;
		if (ctx->lastReadOffset != 0)
		{
			lastIndex = (ctx->nextReadIndex - 1) % (2 * ctx->numInOverlapped);
			to_read = ctx->lastReadRead - ctx->lastReadOffset;
			srcPtr = ctx->inBuffer + ctx->inBlockSize * (size_t)lastIndex + ctx->lastReadOffset;
			memcpy(dstPtr, srcPtr, to_read);
			//	for (int i = 0; i < to_read; i++) printf("%x ", *(srcPtr + i));
			remaining -= to_read;
			dstPtr += to_read;
			read += to_read;
		}
		while (remaining > 0)
		{
			unsigned int simIndex = ctx->nextReadIndex % ctx->numInOverlapped;
			unsigned int nextIndex = (ctx->nextReadIndex + ctx->numInOverlapped) % (2 * ctx->numInOverlapped);
			ULONG transferred;
			ftStatus = FT_GetOverlappedResult(ctx->ftHandle, &ctx->inOverlapped[simIndex], &ctx->inTransferred[simIndex], TRUE);
			if (ftStatus != FT_OK)
			{
				printf("Read failure %d\n", ftStatus);
				return ONI_EREADFAILURE;
			}
			transferred = ctx->inTransferred[simIndex];
			//read in the next part of the double buffer
			FT_ReadPipeEx(ctx->ftHandle, pipe_in, ctx->inBuffer + ((size_t)nextIndex * ctx->inBlockSize), ctx->inBlockSize,
				&ctx->inTransferred[simIndex], &ctx->inOverlapped[simIndex]);
			srcPtr = ctx->inBuffer + ctx->inBlockSize * (size_t)ctx->nextReadIndex;
			to_read = MIN(remaining, transferred);
			memcpy(dstPtr, srcPtr, to_read);
			//	for (int i = 0; i < to_read; i++) printf("%x ", *(dstPtr + i));
			remaining -= to_read;
			dstPtr += to_read;
			read += to_read;
			ctx->nextReadIndex = (ctx->nextReadIndex + 1) % (2 * ctx->numInOverlapped);
			if (to_read < transferred)
			{
				ctx->lastReadRead = transferred;
				ctx->lastReadOffset = to_read;
			}
			else
				ctx->lastReadOffset = 0;
		}
#else	
		ULONG transferred;
        uint8_t* dstPtr = (uint8_t*)data;
		do
		{
			ftStatus = FT_ReadPipeEx(ctx->ftHandle, pipeid_data, dstPtr + read, remaining, &transferred, 1000);
			if (ftStatus != FT_OK && ftStatus != FT_TIMEOUT && ftStatus != FT_IO_PENDING)
			{
				printf("Read failure %d\n", ftStatus);
				return ONI_EREADFAILURE;
			}
			if (ftStatus == FT_OK)
			{
				remaining -= transferred;
				read += transferred;
			}
		} while (remaining > 0);
#endif
		return read;

	}
	else return ONI_EPATHINVALID;
}

int oni_driver_write_stream(oni_driver_ctx driver_ctx,
	oni_write_stream_t stream,
	const char* data,
	size_t size)
{
	CTX_CAST;
	FT_STATUS ftStatus;
	size_t remaining = ((size >> 2) << 2);//round to 32bit boundaries
	size_t to_send;
	ULONG transferred;
	uint8_t* ptr = (uint8_t*)data; //WritePipe do not have a const qualifier for the buffer, even if it does not modify it, so we need to get rid of it

	if (stream != ONI_WRITE_STREAM_DATA) return ONI_EPATHINVALID;

	while (remaining > 0)
	{
		to_send = MIN(remaining, ctx->outBlockSize);
#ifdef _WIN32
		FT_WritePipe(ctx->ftHandle, pipe_out, ptr, to_send, &transferred, &ctx->outOverlapped);
		ftStatus = FT_GetOverlappedResult(ctx->ftHandle, &ctx->outOverlapped, &transferred, TRUE);
		if (ftStatus != FT_OK) return ONI_EWRITEFAILURE;
#else
		ftStatus = FT_WritePipeEx(ctx->ftHandle, pipeid_data, ptr, to_send, &transferred, 1000);
		if (ftStatus != FT_OK && ftStatus != FT_TIMEOUT && ftStatus != FT_IO_PENDING) return ONI_EWRITEFAILURE;
#endif
		if (ftStatus == FT_OK)
		{
			ptr += transferred;
			remaining -= transferred;
		}
	}
	return size;
}

int oni_driver_write_config(oni_driver_ctx driver_ctx,
	oni_config_t reg,
	oni_reg_val_t value)
{
	CTX_CAST;
	uint8_t buffer[45];
	size_t size;
	//to avoid cluttering the USB interface will all the operations required for a device
	//register access, we latch them together and send them in a single USB operation
	if (reg == ONI_CONFIG_DEV_IDX)
	{
		ctx->regOperation.dev_idx = value;
		return ONI_ESUCCESS;
	}
	if (reg == ONI_CONFIG_REG_ADDR)
	{
		ctx->regOperation.dev_addr = value;
		return ONI_ESUCCESS;
	}
	if (reg == ONI_CONFIG_REG_VALUE)
	{
		ctx->regOperation.value = value;
		return ONI_ESUCCESS;
	}
	if (reg == ONI_CONFIG_RW)
	{
		ctx->regOperation.rw = value;
		return ONI_ESUCCESS;
	}
	if (reg == ONI_CONFIG_TRIG)
	{
		size_t i = 0;
		buffer[(9*i)] = CMD_WRITEREG;
		*(uint32_t*)(buffer + 1 + (9 * i)) = _oni_register_offset(ONI_CONFIG_DEV_IDX);
		*(uint32_t*)(buffer + 5 + (9 * i)) = ctx->regOperation.dev_idx;
		i++;
		buffer[(9 * i)] = CMD_WRITEREG;
		*(uint32_t*)(buffer + 1 + (9 * i)) = _oni_register_offset(ONI_CONFIG_REG_ADDR);
		*(uint32_t*)(buffer + 5 + (9 * i)) = ctx->regOperation.dev_addr;
		i++;
		buffer[(9 * i)] = CMD_WRITEREG;
		*(uint32_t*)(buffer + 1 + (9 * i)) = _oni_register_offset(ONI_CONFIG_RW);
		*(uint32_t*)(buffer + 5 + (9 * i)) = ctx->regOperation.rw;
		i++;
		if (ctx->regOperation.rw)
		{
			buffer[(9 * i)] = CMD_WRITEREG;
			*(uint32_t*)(buffer + 1 + (9 * i)) = _oni_register_offset(ONI_CONFIG_REG_VALUE);
			*(uint32_t*)(buffer + 5 + (9 * i)) = ctx->regOperation.value;
			i++;
		}
		buffer[(9 * i)] = CMD_WRITEREG;
		*(uint32_t*)(buffer + 1 + (9 * i)) = _oni_register_offset(ONI_CONFIG_TRIG);
		*(uint32_t*)(buffer + 5 + (9 * i)) = 1;
		i++;
		size = i * 9;
	}
	else
	{
		buffer[0] = CMD_WRITEREG;
		*(uint32_t*)(buffer + 1) = _oni_register_offset(reg);
		*(uint32_t*)(buffer + 5) = value;
		size = 9;
	}
	int res = oni_ft600_sendcmd(ctx, buffer, size);
	if (res != ONI_ESUCCESS) return res;

	if (reg == ONI_CONFIG_RESET && value != 0) oni_ft600_stop_acq(ctx);

	return ONI_ESUCCESS;
}

int oni_driver_read_config(oni_driver_ctx driver_ctx, oni_config_t reg, oni_reg_val_t* value)
{
	CTX_CAST;
	uint8_t buffer[5];
	buffer[0] = CMD_READREG;
	*(uint32_t*)(buffer + 1) = _oni_register_offset(reg);
	int res = oni_ft600_sendcmd(ctx, buffer, 5);
	if (res != ONI_ESUCCESS) return res;

	while (!circBufferCanRead(&ctx->regBuffer, sizeof(oni_reg_val_t))) 
	{
#ifndef _WIN32
		oni_ft600_update_control(ctx);
#endif 
		if (ctx->sigError)
		{
			ctx->sigError = 0;
			return ONI_ESEEKFAILURE;
		}
		Sleep(1);
	}
	circBufferRead(&ctx->regBuffer, (uint8_t*)value, sizeof(oni_reg_val_t));
	return ONI_ESUCCESS;
}


inline void oni_ft600_start_acq(oni_ft600_ctx ctx)
{
#if _WIN32
    FT_STATUS rc;
    rc = FT_SetStreamPipe(ctx->ftHandle, FALSE, FALSE, pipe_in, ctx->inBlockSize);
	for (size_t i = 0; i < ctx->numInOverlapped; i++)
	{
		FT_ReadPipeEx(ctx->ftHandle, pipe_in, ctx->inBuffer + (i * ctx->inBlockSize), ctx->inBlockSize, &ctx->inTransferred[i], &ctx->inOverlapped[i]);
	}
#endif
	ctx->state = STATE_RUNNING;
}

int oni_driver_set_opt_callback(oni_driver_ctx driver_ctx,
	int oni_option,
	const void* value,
	size_t option_len)
{
	CTX_CAST;
	UNUSED(option_len);
	if (oni_option == ONI_OPT_RUNNING)
	{
		if (*(uint32_t*)value == 0)
		{
			oni_ft600_stop_acq(ctx);
		} 
		else
		{
			oni_ft600_start_acq(ctx);
		}
	}
	else if (oni_option == ONI_OPT_RESETACQCOUNTER)
	{
		if (*(uint32_t*)value > 1)
			oni_ft600_start_acq(ctx);
	}
	else if (oni_option == ONI_OPT_BLOCKREADSIZE)
	{
		if (ctx->state == STATE_RUNNING) return ONI_EINVALSTATE;
		uint32_t size = *(oni_size_t*)value;
		if (size % sizeof(uint32_t) != 0) return ONI_EINVALREADSIZE;
		ctx->inBlockSize = size;
#ifdef _WIN32
		if (ctx->state == STATE_INIT) //memory already allocated
		{
			free(ctx->inBuffer);
			ctx->inBuffer = malloc(2 * (size_t)ctx->numInOverlapped * ctx->inBlockSize);
			if (ctx->inBuffer == NULL) return ONI_EBADALLOC;
		//	FT_ClearStreamPipe(ctx->ftHandle, FALSE, FALSE, pipe_in);
		//	FT_SetStreamPipe(ctx->ftHandle, FALSE, FALSE, pipe_in,size);
		}
#endif
	}
	return ONI_ESUCCESS;
}

// TODO: there are some options that could be set here
int oni_driver_set_opt(oni_driver_ctx driver_ctx,
	int driver_option,
	const void* value,
	size_t option_len)
{
	UNUSED(driver_ctx);
	UNUSED(driver_option);
	UNUSED(value);
	UNUSED(option_len);
	return ONI_EINVALOPT;
}

int oni_driver_get_opt(oni_driver_ctx driver_ctx,
	int driver_option,
	void* value,
	size_t* option_len)
{
	UNUSED(driver_ctx);
	UNUSED(driver_option);
	UNUSED(value);
	UNUSED(option_len);
	return ONI_EINVALOPT;
}

const oni_driver_info_t* oni_driver_info() 
{
    return &driverInfo;
}

static inline oni_conf_off_t _oni_register_offset(oni_config_t reg)
{
	switch (reg) {
	case ONI_CONFIG_DEV_IDX:
		return CONFDEVIDXOFFSET;
	case ONI_CONFIG_REG_ADDR:
		return CONFADDROFFSET;
	case ONI_CONFIG_REG_VALUE:
		return CONFVALUEOFFSET;
	case ONI_CONFIG_RW:
		return CONFRWOFFSET;
	case ONI_CONFIG_TRIG:
		return CONFTRIGOFFSET;
	case ONI_CONFIG_RUNNING:
		return CONFRUNNINGOFFSET;
	case ONI_CONFIG_RESET:
		return CONFRESETOFFSET;
	case ONI_CONFIG_SYSCLKHZ:
		return CONFSYSCLKHZOFFSET;
	case ONI_CONFIG_ACQCLKHZ:
		return CONFACQCLKHZOFFSET;
	case ONI_CONFIG_RESETACQCOUNTER:
		return CONFRESETACQCOUNTER;
	case ONI_CONFIG_HWADDRESS:
		return CONFHWADDRESS;
	default:
		return 0;
	}
}
