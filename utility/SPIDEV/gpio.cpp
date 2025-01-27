/*
 *
 * Copyright (c) 2024, Copyright (c) 2024 TMRh20 & 2bndy5
 * All rights reserved.
 *
 *
 */
#include <linux/gpio.h>
#include <unistd.h>    // close()
#include <fcntl.h>     // open()
#include <sys/ioctl.h> // ioctl()
#include <errno.h>     // errno, strerror()
#include <string.h>    // std::string, strcpy()
#include <map>
#include "gpio.h"

// instantiate some global structs to setup cache
// doing this globally ensures the data struct is zero-ed out
typedef int gpio_fd; // for readability
std::map<rf24_gpio_pin_t, gpio_fd> cachedPins;
struct gpio_v2_line_request request;
struct gpio_v2_line_values data;
struct gpiochip_info chipMeta;

void GPIOChipCache::openDevice()
{
    if (fd < 0) {
        fd = open(chip, O_RDONLY);
        if (fd < 0) {
            std::string msg = "Can't open device ";
            msg += chip;
            msg += "; ";
            msg += strerror(errno);
            throw GPIOException(msg);
        }
    }
}

void GPIOChipCache::closeDevice()
{
    if (fd >= 0) {
        close(fd);
        fd = -1;
    }
}

GPIOChipCache::GPIOChipCache()
{
    try {
        openDevice();
    }
    catch (GPIOException& exc) {
        chip = "/dev/gpiochip0";
        openDevice();
    }
    request.num_lines = 1;
    strcpy(request.consumer, "RF24 lib");
    data.mask = 1ULL; // only change value for specified pin

    // cache chip info
    int ret = ioctl(fd, GPIO_GET_CHIPINFO_IOCTL, &chipMeta);
    if (ret < 0) {
        std::string msg = "Could not gather info about ";
        msg += chip;
        throw GPIOException(msg);
        return;
    }
    closeDevice(); // in case other apps want to access it
}

GPIOChipCache::~GPIOChipCache()
{
    closeDevice();
    for (std::map<rf24_gpio_pin_t, gpio_fd>::iterator i = cachedPins.begin(); i != cachedPins.end(); ++i) {
        if (i->second > 0) {
            close(i->second);
        }
    }
}

// GPIO chip cache manager
GPIOChipCache gpioCache;

GPIO::GPIO()
{
}

GPIO::~GPIO()
{
}

void GPIO::open(rf24_gpio_pin_t port, int DDR)
{
    if (port > chipMeta.lines) {
        std::string msg = "pin number " + std::to_string(port) + " not available for " + gpioCache.chip;
        throw GPIOException(msg);
        return;
    }

    // check if pin is already in use
    std::map<rf24_gpio_pin_t, gpio_fd>::iterator pin = cachedPins.find(port);
    if (pin == cachedPins.end()) { // pin not in use; add it to cached request
        request.offsets[0] = port;
        request.fd = 0;
    }
    else {
        request.fd = pin->second;
    }

    gpioCache.openDevice();
    int ret;
    if (request.fd <= 0) {
        ret = ioctl(gpioCache.fd, GPIO_V2_GET_LINE_IOCTL, &request);
        if (ret == -1 || request.fd <= 0) {
            std::string msg = "[GPIO::open] Can't get line handle from IOCTL; ";
            msg += strerror(errno);
            throw GPIOException(msg);
            return;
        }
    }
    gpioCache.closeDevice(); // in case other apps want to access it

    // set the pin and direction
    request.config.flags = DDR ? GPIO_V2_LINE_FLAG_OUTPUT : GPIO_V2_LINE_FLAG_INPUT;

    ret = ioctl(request.fd, GPIO_V2_LINE_SET_CONFIG_IOCTL, &request.config);
    if (ret == -1) {
        std::string msg = "[gpio::open] Can't set line config; ";
        msg += strerror(errno);
        throw GPIOException(msg);
        return;
    }
    cachedPins.insert(std::pair<rf24_gpio_pin_t, gpio_fd>(port, request.fd));
}

void GPIO::close(rf24_gpio_pin_t port)
{
    // This is not really used in RF24 convention (designed for embedded apps).
    // Instead rely on gpioCache destructor (see above)
}

int GPIO::read(rf24_gpio_pin_t port)
{
    std::map<rf24_gpio_pin_t, gpio_fd>::iterator pin = cachedPins.find(port);
    if (pin == cachedPins.end() || pin->second <= 0) {
        throw GPIOException("[GPIO::read] pin not initialized! Use GPIO::open() first");
        return -1;
    }

    data.bits = 0ULL;

    int ret = ioctl(pin->second, GPIO_V2_LINE_GET_VALUES_IOCTL, &data);
    if (ret == -1) {
        std::string msg = "[GPIO::read] Can't get line value from IOCTL; ";
        msg += strerror(errno);
        throw GPIOException(msg);
        return ret;
    }
    return data.bits & 1ULL;
}

void GPIO::write(rf24_gpio_pin_t port, int value)
{
    std::map<rf24_gpio_pin_t, gpio_fd>::iterator pin = cachedPins.find(port);
    if (pin == cachedPins.end() || pin->second <= 0) {
        throw GPIOException("[GPIO::write] pin not initialized! Use GPIO::open() first");
        return;
    }

    data.bits = value;

    int ret = ioctl(pin->second, GPIO_V2_LINE_SET_VALUES_IOCTL, &data);
    if (ret == -1) {
        std::string msg = "[GPIO::write] Can't set line value from IOCTL; ";
        msg += strerror(errno);
        throw GPIOException(msg);
        return;
    }
}
