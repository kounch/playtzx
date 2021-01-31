/*
 * Standard audio related includes
 */

#include <stdio.h>

#include <unistd.h>
#include <CoreAudio/AudioHardware.h>

#include "audio.h"

/* Mandatory variables */

static AudioDeviceID device;     /* the default device */
static UInt32 deviceBufferSize;  /* bufferSize returned by kAudioDevicePropertyBufferSize */
static AudioStreamBasicDescription	deviceFormat;	/* info about the default device */

#define MAX_AUDIO_RB 22050
float ringbuf[MAX_AUDIO_RB];

volatile unsigned int ringbuffer_read = 0;
volatile unsigned int ringbuffer_write = 0;

void 
play_buffer(register unsigned char *buf, register int count)
{
  /* then make some noise! */
  register int i;

  for( i=0; i<count; i++ ) {
    register float samp = (*buf++ - 128) * (1.0f / 128.0f);
    /* Mac OS X CoreAudio sounds are always in a float format */
    ringbuf[ ringbuffer_write++ ] = samp;
    if( ringbuffer_write == MAX_AUDIO_RB ) ringbuffer_write = 0;
    while ( ringbuffer_write + 1 == (ringbuffer_read == 0 ? MAX_AUDIO_RB : ringbuffer_read )) {
      /* If we have filled the buffer with loads of data, we can just
         wait till we have used up some of what we have */
      usleep( 50 );
    }
  }
}

int finishing = 0;

/* this is the audio processing callback. */
OSStatus appIOProc ( AudioDeviceID inDevice, const AudioTimeStamp* inNow,
                     const AudioBufferList* inInputData, const AudioTimeStamp*  inInputTime,
                     AudioBufferList*  outOutputData, const AudioTimeStamp* inOutputTime, void* defptr )
{
  register int f;
  register int numSamples = deviceBufferSize / deviceFormat.mBytesPerFrame;

  /* assume floats for now.... */
  register float *out = outOutputData->mBuffers[0].mData;

  if ( finishing && ( ringbuffer_read + 1 == (ringbuffer_write == 0 ? MAX_AUDIO_RB : ringbuffer_write ) ) )
    ringbuffer_write = ringbuffer_read;

  for( f=0; f<numSamples; f++ )
  {
    if( (ringbuffer_write == ringbuffer_read ||
        ringbuffer_read + 1 == (ringbuffer_write == 0 ? MAX_AUDIO_RB : ringbuffer_write ))) {
      /* buffer underrun - send last sample available */
      *out++ = ringbuf[ ringbuffer_read ];
      *out++ = ringbuf[ ringbuffer_read ];
    } else {
      *out++ = ringbuf[ ringbuffer_read ];
      *out++ = ringbuf[ ringbuffer_read++ ];
      if( ringbuffer_read == MAX_AUDIO_RB ) ringbuffer_read = 0;
    }
  }

  return kAudioHardwareNoError;
}

/* note, freq is changed! */

int 
open_audio(int *freq)
{
  OSStatus  err = kAudioHardwareNoError;
  UInt32    count;    
  device    = kAudioDeviceUnknown;
 
  /* get the default output device for the HAL */
  count = sizeof(device);   /* it is required to pass the size of the data to be returned */
  err = AudioHardwareGetProperty(kAudioHardwarePropertyDefaultOutputDevice, &count, (void *) &device);
  if (err != kAudioHardwareNoError) {
    fprintf(stderr, "get kAudioHardwarePropertyDefaultOutputDevice error %ld\n", err);
    return -1;
  }
    
  /* get the buffersize that the default device uses for IO */
  count = sizeof(deviceBufferSize); /* it is required to pass the size of the data to be returned */
  err = AudioDeviceGetProperty(device, 0, false, kAudioDevicePropertyBufferSize, &count, &deviceBufferSize);
  if (err != kAudioHardwareNoError) {
    fprintf(stderr, "get kAudioDevicePropertyBufferSize error %ld\n", err);
    return -1;
  }
   
  /* get a description of the data format used by the default device */
  count = sizeof(deviceFormat); /* it is required to pass the size of the data to be returned */
  err = AudioDeviceGetProperty(device, 0, false, kAudioDevicePropertyStreamFormat, &count, &deviceFormat);
  if (err != kAudioHardwareNoError) {
    fprintf(stderr, "get kAudioDevicePropertyStreamFormat error %ld\n", err);
    return -1;
  }
  if (deviceFormat.mFormatID != kAudioFormatLinearPCM) {
    fprintf(stderr, "mFormatID !=  kAudioFormatLinearPCM\n");
    return -1;
  }
  if (!(deviceFormat.mFormatFlags & kLinearPCMFormatFlagIsFloat)) {
    fprintf(stderr, "Sorry, currently only works with float format....\n");
    return -1;
  }

#if 0
  deviceFormat.mSampleRate = *freq;

  err = AudioDeviceSetProperty(device, NULL, 0, 0, kAudioDevicePropertyStreamFormat, count, &deviceFormat);
  if (err != kAudioHardwareNoError) {
    fprintf(stderr, "error setting sample rate\n", err);
    return -1;
  }
#endif
 
  *freq = deviceFormat.mSampleRate;

#if 0
  fprintf(stderr, "deviceBufferSize = %ld\n", deviceBufferSize);
  fprintf(stderr, "mSampleRate = %g\n", deviceFormat.mSampleRate);
  fprintf(stderr, "mFormatFlags = %08lX\n", deviceFormat.mFormatFlags);
  fprintf(stderr, "mBytesPerPacket = %ld\n", deviceFormat.mBytesPerPacket);
  fprintf(stderr, "mFramesPerPacket = %ld\n", deviceFormat.mFramesPerPacket);
  fprintf(stderr, "mChannelsPerFrame = %ld\n", deviceFormat.mChannelsPerFrame);
  fprintf(stderr, "mBytesPerFrame = %ld\n", deviceFormat.mBytesPerFrame);
  fprintf(stderr, "mBitsPerChannel = %ld\n", deviceFormat.mBitsPerChannel);
#endif

  err = AudioDeviceAddIOProc(device, appIOProc, NULL);  /* setup our device with an IO proc */
  if (err != kAudioHardwareNoError) return -1;
    
  err = AudioDeviceStart(device, appIOProc);        /* start playing sound through the device */
  if (err != kAudioHardwareNoError) return -1;

  return 0;
}

void 
close_audio()
{
  OSStatus err = kAudioHardwareNoError;

  finishing = 1;
  while ( ringbuffer_write != ringbuffer_read ) {
    /* Wait till we have used up all of the buffer */
    usleep( 50 );
  }
    
  err = AudioDeviceStop(device, appIOProc);       /* stop playing sound through the device */
  if (err != kAudioHardwareNoError) return;

  err = AudioDeviceRemoveIOProc(device, appIOProc);     /* remove the IO proc from the device */
  if (err != kAudioHardwareNoError) return;
}
