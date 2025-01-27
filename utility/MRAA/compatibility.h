/**
 * @file compatiblity.h
 * @author purinda
 *
 * Created on 24 June 2012, 3:08 PM
 */

#ifndef RF24_UTILITY_MRAA_COMPATIBLITY_H_
#define RF24_UTILITY_MRAA_COMPATIBLITY_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <time.h>
#include <sys/time.h>
#include <stdint.h>

void __msleep(int milisec);

void __usleep(int milisec);

void __start_timer();

uint32_t __millis();

#ifdef __cplusplus
}
#endif

#endif // RF24_UTILITY_MRAA_COMPATIBLITY_H_
