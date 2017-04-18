/*
 * Standalone temperature logger
 *
 */

#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include "pcsensor.h"

/* Calibration adjustments */
/* See http://www.pitt-pladdy.com/blog/_20110824-191017_0100_TEMPer_under_Linux_perl_with_Cacti/ */
#if 1
static float scale = 1.10891089108910891089;
static float offset = -2.89900990099009900987;
#else
static float scale = 1;
static float offset = 0;
#endif


int main(){
  int passes = 0;
  float tempc = 0.0000;
  sensor_t *sensor;
  if (!(sensor = malloc(sizeof(sensor_t)))) {
    exit(-1);
  }
  do {
    if (!pcsensor_open(sensor)) {
      /* Open fails sometime, sleep and try again */
      sleep(3);
    } else {
      tempc = pcsensor_get_temperature(sensor);
      pcsensor_close(sensor);
    }
    ++passes;
  /* Read fails silently with a 0.0 return, so repeat until not zero
     or until we have read the same zero value 3 times (just in case
     temp is really dead on zero */
  } while ((tempc > -0.0001 && tempc < 0.0001) || passes >= 4);
  free(sensor);

  if (!((tempc > -0.0001 && tempc < 0.0001) || passes >= 4)) {
    /* Apply calibrations */
    tempc = (tempc * scale) + offset;

    struct tm *utc;
    time_t t;
    t = time(NULL);
    utc = localtime(&t);
		
    char dt[80];

    strftime(dt, 80, "%Y-%m-%d %H:%M:%S", utc);
    printf("%s,%f\n", dt, tempc);
    fflush(stdout);

    return 0;
  }
  else {
    return 1;
  }
}
