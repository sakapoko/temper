/*
 * pcsensor.c by Michitaka Ohno (c) 2011 (elpeo@mars.dti.ne.jp)
 * based oc pcsensor.c by Juan Carlos Perez (c) 2011 (cray@isp-sl.com)
 * based on Temper.c by Robert Kavaler (c) 2009 (relavak.com)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 * 
 * THIS SOFTWARE IS PROVIDED BY Juan Carlos Perez ''AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL Robert kavaler BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * 
 */

#include "pcsensor.h"
 
#define INTERFACE1 (0x00)
#define INTERFACE2 (0x01)
#define SUPPORTED_DEVICES (2)

#define EP_2_IN 0x82

const static unsigned short vendor_id[] = { 
  0x1130,
  0x0c45
};
const static unsigned short product_id[] = { 
  0x660c,
  0x7401
};

const static char uTemperatura[] = { 0x01, 0x80, 0x33, 0x01, 0x00, 0x00, 0x00, 0x00 };
const static char uIni1[] = { 0x01, 0x82, 0x77, 0x01, 0x00, 0x00, 0x00, 0x00 };
const static char uIni2[] = { 0x01, 0x86, 0xff, 0x01, 0x00, 0x00, 0x00, 0x00 };
const static char uCmd0[] = {    0,    0,    0,    0,    0,    0,    0,    0 };
const static char uCmd1[] = {   10,   11,   12,   13,    0,    0,    2,    0 };
const static char uCmd2[] = {   10,   11,   12,   13,    0,    0,    1,    0 };
const static char uCmd3[] = { 0x52,    0,    0,    0,    0,    0,    0,    0 };
const static char uCmd4[] = { 0x54,    0,    0,    0,    0,    0,    0,    0 };

const static int reqIntLen=8;
const static int timeout=5000; /* timeout in ms */
 
static int debug = 0;

static int ini_control_transfer(libusb_device_handle *dev) {
  int r,i;
  char question[] = { 0x01,0x01 };

  r = libusb_control_transfer(dev, 0x21, 0x09, 0x0201, 0x00, (unsigned char *) question, reqIntLen, timeout);
  if (r < 0) {
    if (debug) {
      printf("USB control write"); 
    }
    return -1;
  }
  if (debug) {
    for (i = 0; i < reqIntLen; i++) printf("%02x ", question[i] & 0xff);
    printf("\n");
  }
  return 0;
}
 
static int control_transfer(libusb_device_handle *dev, const char *pquestion) {
  int r,i;
  char question[reqIntLen];
    
  memcpy(question, pquestion, sizeof question);
  r = libusb_control_transfer(dev, 0x21, 0x09, 0x0200, 0x01, (unsigned char *) question, reqIntLen, timeout);
  if (r < 0) {
    if (debug) {
      printf("USB control write");
    }
    return -1;
  }

  if (debug) {
    for (i = 0; i < reqIntLen; i++) printf("%02x ", question[i]  & 0xff);
    printf("\n");
  }
  return 0;
}

static int interrupt_read(libusb_device_handle *dev) {
  int r,i;
  int size = 0;
  unsigned char answer[reqIntLen];

  bzero(answer, reqIntLen);
  r = libusb_interrupt_transfer(dev, EP_2_IN, answer, reqIntLen, &size, timeout);
  if ( r != 0) {
    if(debug){
      printf("USB interrupt read %d %d\n", r, size);
    }
    return -1;
  }
  
  if (debug) {
    for (i = 0; i < reqIntLen; i++) printf("%02x ", answer[i]  & 0xFF);
    
    printf("\n");
  }
  return 0;
}

static int interrupt_read_temperatura(libusb_device_handle *dev, float *tempC) {
  int r,i, temperature;
  int size = 0;
  unsigned char answer[reqIntLen];

  bzero(answer, reqIntLen);
  r = libusb_interrupt_transfer(dev, EP_2_IN, answer, reqIntLen, &size, timeout);
  if (r != 0) {
    if (debug) {
      printf("USB interrupt read %d\n", r);
    }
    return -1;
  }

  if (debug) {
    for (i = 0; i < reqIntLen; i++) printf("%02x ",answer[i]  & 0xFF);
    printf("\n");
  }
    
  temperature = (answer[3] & 0xFF) + (answer[2] << 8);
  *tempC = temperature * (125.0 / 32000.0);
  return 0;
}

static int get_data(libusb_device_handle *dev, char *buf, int len){
  return libusb_control_transfer(dev, 0xa1, 1, 0x300, 0x01, (unsigned char *)buf, len, timeout);
}

static int get_temperature(libusb_device_handle *dev, float *tempC){
  char buf[256];
  int ret, temperature, i;

  control_transfer(dev, uCmd1 );
  control_transfer(dev, uCmd4 );
  for(i = 0; i < 7; i++) {
    control_transfer(dev, uCmd0 );
  }
  control_transfer(dev, uCmd2 );
  ret = get_data(dev, buf, 256);
  if(ret < 2) {
    return -1;
  }
  temperature = (buf[1] & 0xFF) + (buf[0] << 8);	
  *tempC = temperature * (125.0 / 32000.0);
  return 0;
}

static int initialize_sensor(sensor_t *sensor) {
  char buf[256];
  int i, ret;

  if (libusb_kernel_driver_active(sensor->device, INTERFACE1) == 1) {
    if (libusb_detach_kernel_driver(sensor->device, INTERFACE1) != 0) {
      perror("detaching kernel driver failed(1)");
      return 0;
    }
  }
  if (libusb_kernel_driver_active(sensor->device, INTERFACE2) == 1) {
    if (libusb_detach_kernel_driver(sensor->device, INTERFACE2) != 0) {
      perror("detaching kernel driver failed(2)");
      return 0;
    }
  }
  if (libusb_set_configuration(sensor->device, 0x01) < 0) {
    if(debug){
      printf("Could not set configuration 1\n");
    }
    return 0;
  }
  if (libusb_claim_interface(sensor->device, INTERFACE1) < 0) {
    perror("claim interface failed (1)");
    return 0;
  }
  if (libusb_claim_interface(sensor->device, INTERFACE2) < 0) {
    perror("claim interface failed (2)");
    return 0;
  }
  switch (sensor->type) {
    case 0:
      control_transfer(sensor->device, uCmd1 );
      control_transfer(sensor->device, uCmd3 );
      control_transfer(sensor->device, uCmd2 );
      ret = get_data(sensor->device, buf, 256);
      if(debug){	
        printf("Other Stuff (%d bytes):\n", ret);
        for(i = 0; i < ret; i++) {
          printf(" %02x", buf[i] & 0xFF);
          if(i % 16 == 15) {
            printf("\n");
          }
        }
        printf("\n");
      }
      break;
    case 1:
      if (ini_control_transfer(sensor->device) < 0) { /* 1 */
        fprintf(stderr, "Failed to ini_control_transfer (device_type 1)");
        return 0;
      }
#if 0 /* original sequence */
      control_transfer(sensor->device, uTemperatura ); /* 2 */
      interrupt_read(sensor->device);
 
      control_transfer(sensor->device, uIni1 ); /*3 */
      interrupt_read(sensor->device);
 
      control_transfer(sensor->device, uIni2 ); /* 4 */
      interrupt_read(sensor->device);
      interrupt_read(sensor->device);
#else
      /* modify reset sequence to 1,3,4,3,3 */
      control_transfer(sensor->device, uIni1 );
      interrupt_read(sensor->device);
      control_transfer(sensor->device, uIni2 );
      interrupt_read(sensor->device);
      interrupt_read(sensor->device);
      control_transfer(sensor->device, uIni1 );
      interrupt_read(sensor->device);
      control_transfer(sensor->device, uIni1 );
      interrupt_read(sensor->device);
#endif
      break;
  }
  return 1;
}

static int setup_libusb_access(sensor_t *sensor) {
  libusb_device *dev;
  libusb_device **devs;
  int i = 0;
  int j;

  libusb_init(&sensor->context);
  if(debug) {
    libusb_set_debug(sensor->context, 3);
  } else {
    libusb_set_debug(sensor->context, 0);
  }

  if (libusb_get_device_list(sensor->context, &devs) < 0) {
    perror("no usb device found");
    return 0;
  }
  while ((dev = devs[i++]) != NULL) {
    struct libusb_device_descriptor desc;
    if (libusb_get_device_descriptor(dev, &desc) < 0) {
      perror("failed to get device descriptor\n");
    }
    for (j = 0; j < SUPPORTED_DEVICES; j++) {
      /* open first device */
      if (desc.idVendor == vendor_id[j] && desc.idProduct == product_id[j]) {
        libusb_open(dev, &sensor->device);
        sensor->type = j;
        return initialize_sensor(sensor);
      }
    }
  }
  return 0;
}

int pcsensor_open(sensor_t *sensor){
  memset(sensor, 0, sizeof(sensor_t));
  
  if (!(setup_libusb_access(sensor))) {
    return 0;
  } 

  if(debug){
    printf("device_type=%d\n", sensor->type);
  }
  return 1;
}

void pcsensor_close(sensor_t *sensor) {
  libusb_close(sensor->device);
  libusb_exit(sensor->context);
}

float pcsensor_get_temperature(sensor_t *sensor) {
  float tempc;
  int ret;
  switch (sensor->type) {
    case 0:
      ret = get_temperature(sensor->device, &tempc);
      break;
    case 1:
      control_transfer(sensor->device, uTemperatura );
      ret = interrupt_read_temperatura(sensor->device, &tempc);
      break;
  }
  if(ret < 0){
    return FLT_MIN;
  }
  return tempc;
}

