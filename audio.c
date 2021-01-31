/*
 * Standard audio related includes
 */

#include <stdio.h>
#include <unistd.h>

#ifdef __APPLE__
  #include <CoreAudio/AudioHardware.h>
#elif defined(__linux__)
  #include <sys/ioctl.h>
  #include <stdlib.h>
  #include <fcntl.h>
  #include <sys/soundcard.h>
#endif

#include "audio.h"

/* Mandatory variables */
#ifdef __APPLE__
  static AudioDeviceID device;     /* the default device */
  static UInt32 deviceBufferSize;  /* bufferSize returned by kAudioDevicePropertyBufferSize */
  static AudioStreamBasicDescription	deviceFormat;	/* info about the default device */
  
  #define MAX_AUDIO_RB 22050
  float ringbuf[MAX_AUDIO_RB];
  
  volatile unsigned int ringbuffer_read = 0;
  volatile unsigned int ringbuffer_write = 0;
#elif defined(__linux__)
  #define DEVICE_NAME "/dev/dsp"
  int audio_fd;
#endif

#ifdef __APPLE__
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

#elif defined(__linux__)
void play_buffer (unsigned char *buf, int count)
{
  /* then make some noise! */
  int len;

  if ((len = write (audio_fd, buf, count)) == -1)
    {
      perror ("audio write");
      exit (1);
    }
}
#endif

/* note, freq is changed! */

int 
open_audio(int *freq)
{
#ifdef __APPLE__
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
#elif defined(__linux__)
  int format;
  int stereo;
  int speed;

  /* Open the audio device thingy */

  if ((audio_fd = open (DEVICE_NAME, O_WRONLY, 0)) == -1)
    {
      /* Opening device failed */
      perror (DEVICE_NAME);
      exit (1);
    }

  /*
   * set the audio parameters 
   */


  /* First the audio format */

  format = AFMT_U8;
  if (ioctl (audio_fd, SNDCTL_DSP_SETFMT, &format) == -1)
    {
      /* Fatal error */
      perror ("SNDCTL_DSP_SETFMT");
      exit (1);
    }

  if (format != AFMT_U8)
    {
      /* The device doesn't support the requested audio format. The program  
         should use another format (for example the one returned in "format") 
         or alternatively it must display an error message and to abort. */

      /* if the device does not support AFMT_U8 then it can **** ***! :) */

      puts ("bloody hell!");
      exit (1);
    }

  /* then number of channels */

  stereo = 0;			/* 0=mono, 1=stereo */

  if (ioctl (audio_fd, SNDCTL_DSP_STEREO, &stereo) == -1)
    {
      /* Fatal error */
      perror ("SNDCTL_DSP_STEREO");
      exit (1);
    }

  if (stereo != 0)
    {
      /* The device doesn't support stereo mode. */

      /* which is quite unbelievable! */

      puts ("holy smoke!");
      exit (1);
    }

  /* and finally, sampling speed */

  speed = *freq;

  if (ioctl (audio_fd, SNDCTL_DSP_SPEED, &speed) == -1)
    {
      /* Fatal error */
      perror ("SNDCTL_DSP_SPEED");
      exit (1);
    }

  if (speed != *freq)
    {
      /* The device doesn't support the requested speed. */

      printf ("using speed of %d instead of %d\n", speed, *freq);
    }

  *freq = speed;

  return (audio_fd);
#endif
}

void 
close_audio()
{
#ifdef __APPLE__
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
#elif defined(__linux__)
  close (audio_fd);
#endif
}
