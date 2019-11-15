/*
 * Copyright (C) 2019 HAW Hamburg
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup tests
 * @{
 *
 * @file
 * @brief       Test application for periph timer API
 *
 * @author      Michel Rottleuthner <michel.rottleuthner@haw-hamburg.de>
 *
 * @}
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include "shell.h"
#include "periph/timer.h"
#include "periph/gpio.h"
#include "mutex.h"

#include "sc_args.h"

#define ARG_ERROR       (-1)
#define CONVERT_ERROR   (-32768)
#define RESULT_OK       (0)
#define RESULT_ERROR    (-1)
#define INVALID_ARGS    puts("Error: Invalid number of arguments")
#define PARSE_ERROR     puts("Error: unable to parse arguments")

#define CB_TOGGLE_STR   "cb_toggle"
#define CB_HIGH_STR     "cb_high"
#define CB_LOW_STR      "cb_low"

static mutex_t cb_mutex;
static gpio_t debug_pins[TIMER_NUMOF];

static inline void _debug_toogle(gpio_t pin)
{
    if (pin != GPIO_UNDEF) {
        gpio_toggle(pin);
    }
}

static inline void _debug_set(gpio_t pin)
{
    if (pin != GPIO_UNDEF) {
        gpio_set(pin);
    }
}

static inline void _debug_clear(gpio_t pin)
{
    if (pin != GPIO_UNDEF) {
        gpio_clear(pin);
    }
}

static int _print_cmd_result(const char *cmd, bool success, int ret,
                             bool print_ret)
{
    printf("%s: %s()", success ? "Success" : "Error", cmd);

    if (print_ret) {
        printf(": [%d]", ret);
    }

    printf("\n");

    return success ? RESULT_OK : RESULT_ERROR;
}

void cb_toggle(void *arg, int channel)
{
    (void)channel;
    gpio_t pin = (gpio_t)(intptr_t)arg;
    _debug_toogle(pin);
    mutex_unlock(&cb_mutex);
}

void cb_high(void *arg, int channel)
{
    (void)channel;
    gpio_t pin = (gpio_t)(intptr_t)arg;
    _debug_set(pin);
    mutex_unlock(&cb_mutex);
}

void cb_low(void *arg, int channel)
{
    (void)channel;
    gpio_t pin = (gpio_t)(intptr_t)arg;
    _debug_clear(pin);
    mutex_unlock(&cb_mutex);
}

/* API calls */

int cmd_timer_init(int argc, char **argv)
{
    if (sc_args_check(argc, argv, 3, 3, "DEV FREQ CALLBACK") != ARGS_OK) {
        return ARGS_ERROR;
    }

    int dev = sc_arg2dev(argv[1], TIMER_NUMOF);
    if (dev < 0) {
        return -ENODEV;
    }

    long freq = 0;
    if (sc_arg2long(argv[2], &freq) != ARGS_OK) {
        return ARGS_ERROR;
    }

    timer_cb_t cb = NULL;
    if (strncmp(CB_TOGGLE_STR, argv[3], strlen(argv[3])) == 0) {
        cb = cb_toggle;
    }
    else if (strncmp(CB_HIGH_STR, argv[3], strlen(argv[3])) == 0) {
        cb = cb_high;
    }
    else if (strncmp(CB_LOW_STR, argv[3], strlen(argv[3])) == 0) {
        cb = cb_low;
    }
    else {
        printf("no valid callback name given. Valid values are %s, %s or %s\n",
               CB_TOGGLE_STR, CB_HIGH_STR, CB_LOW_STR);
        return ARGS_ERROR;
    }

    int res = timer_init(dev, freq, cb, (void*)(intptr_t)debug_pins[dev]);

    return _print_cmd_result("timer_init", res == 0, res, true);
}

int _timer_set(int argc, char **argv, bool absolute)
{
    if (sc_args_check(argc, argv, 3, 3, "DEV CHANNEL TICKS") != ARGS_OK) {
        return ARGS_ERROR;
    }

    int dev = sc_arg2dev(argv[1], TIMER_NUMOF);
    if (dev < 0) {
        return -ENODEV;
    }

    int chan = 0;
    if (sc_arg2int(argv[2], &chan) != ARGS_OK) {
        return ARGS_ERROR;
    }

    unsigned int timeout = 0;
    if (sc_arg2uint(argv[3], &timeout) != ARGS_OK) {
        return ARGS_ERROR;
    }

    int res = 0;
    mutex_lock(&cb_mutex);

    _debug_toogle(debug_pins[dev]);
    if (absolute) {
        res = timer_set_absolute(dev, chan, timeout);
    }
    else {
        res = timer_set(dev, chan, timeout);
    }

    /* wait for unlock by cb */
    mutex_lock(&cb_mutex);

    /* reset mutex state */
    mutex_unlock(&cb_mutex);
    return res;
}

int cmd_timer_set(int argc, char **argv)
{
    int res = _timer_set(argc, argv, false);
    return _print_cmd_result("timer_set", (res == 0), res, true);
}

int cmd_timer_set_absolute(int argc, char **argv)
{
    int res = _timer_set(argc, argv, true);
    return _print_cmd_result("timer_set_absolute", (res == 0), res, true);
}

int cmd_timer_clear(int argc, char **argv)
{
    if (sc_args_check(argc, argv, 3, 3, "DEV CHANNEL TICKS") != ARGS_OK) {
        return ARGS_ERROR;
    }

    int dev = sc_arg2dev(argv[1], TIMER_NUMOF);
    if (dev < 0) {
        return -ENODEV;
    }

    int chan = 0;
    if (sc_arg2int(argv[2], &chan) != ARGS_OK) {
        return ARGS_ERROR;
    }

    int res = timer_clear(dev, chan);

    return _print_cmd_result("timer_clear", (res == 0), res, true);
}

int cmd_timer_read(int argc, char **argv)
{
    if (sc_args_check(argc, argv, 3, 3, "DEV CHANNEL TICKS") != ARGS_OK) {
        return ARGS_ERROR;
    }

    int dev = sc_arg2dev(argv[1], TIMER_NUMOF);
    if (dev < 0) {
        return -ENODEV;
    }

    printf("Success: timer_read(): [%u]\n", timer_read(dev));
    return RESULT_OK;
}

int cmd_timer_start(int argc, char **argv)
{
    if (sc_args_check(argc, argv, 3, 3, "DEV CHANNEL TICKS") != ARGS_OK) {
        return ARGS_ERROR;
    }

    int dev = sc_arg2dev(argv[1], TIMER_NUMOF);
    if (dev < 0) {
        return -ENODEV;
    }

    timer_start(dev);
    return _print_cmd_result("timer_start", true, 0, false);
}

int cmd_timer_stop(int argc, char **argv)
{
    if (sc_args_check(argc, argv, 3, 3, "DEV CHANNEL TICKS") != ARGS_OK) {
        return ARGS_ERROR;
    }

    int dev = sc_arg2dev(argv[1], TIMER_NUMOF);
    if (dev < 0) {
        return -ENODEV;
    }

    timer_stop(dev);
    return _print_cmd_result("timer_stop", true, 0, false);
}

/* helper calls (non-API) */

int cmd_timer_debug_pin(int argc, char **argv)
{
    if (sc_args_check(argc, argv, 3, 3, "DEV PORT PIN") != ARGS_OK) {
        return ARGS_ERROR;
    }

    int dev = sc_arg2dev(argv[1], TIMER_NUMOF);
    if (dev < 0) {
        return -ENODEV;
    }

    /* parse and init debug pin */
    uint32_t port, pin = 0;
    if ((sc_arg2ulong(argv[2], &port) != ARGS_OK) ||
        (sc_arg2ulong(argv[3], &pin) != ARGS_OK)) {
        return _print_cmd_result("timer_debug_pin", false, 1, false);
    }

    debug_pins[dev] = GPIO_PIN(port, pin);
    gpio_init(pin, GPIO_OUT);

    return _print_cmd_result("timer_debug_pin", true, 0, false);
}

int cmd_timer_bench_read(int argc, char **argv)
{
    if (sc_args_check(argc, argv, 2, 2, "DEV REPEAT") != ARGS_OK) {
        return ARGS_ERROR;
    }

    int dev = sc_arg2dev(argv[1], TIMER_NUMOF);
    if (dev < 0) {
        return -ENODEV;
    }

    unsigned int repeat = 0;
    if (sc_arg2uint(argv[2], &repeat) != ARGS_OK) {
        return ARGS_ERROR;
    }

    _debug_toogle(debug_pins[dev]);

    for (unsigned int i = 0; i < repeat; i++) {
        timer_read(dev);
    }

    _debug_toogle(debug_pins[dev]);

    return _print_cmd_result("cmd_timer_read_bench", true, 0, false);
}

int cmd_get_metadata(int argc, char **argv)
{
    (void)argv;
    (void)argc;

    printf("Success: [%s, %s]\n", RIOT_BOARD, RIOT_APPLICATION);

    return 0;
}

static const shell_command_t shell_commands[] = {
    { "timer_init", "Initialize timer device", cmd_timer_init },
    { "timer_set", "set timer to relative value", cmd_timer_set },
    { "timer_set_absolute", "set timer to absolute value",
      cmd_timer_set_absolute },
    { "timer_clear", "clear timer", cmd_timer_clear },
    { "timer_read", "read timer", cmd_timer_read },
    { "timer_start", "start timer", cmd_timer_start },
    { "timer_stop", "stop timer", cmd_timer_stop },
    { "timer_debug_pin", "config debug pin", cmd_timer_debug_pin },
    { "timer_read_bench", "execute multiple reads to determine overhead",
      cmd_timer_bench_read },
    { "get_metadata", "Get the metadata of the test firmware",
      cmd_get_metadata },
    { NULL, NULL, NULL }
};

int main(void)
{
    puts("Start: Test for the timer API");

    /* set all debug pins to undef */
    for (unsigned i = 0; i < TIMER_NUMOF; ++i) {
        debug_pins[i] = GPIO_UNDEF;
    }

    char line_buf[SHELL_DEFAULT_BUFSIZE];
    shell_run(shell_commands, line_buf, SHELL_DEFAULT_BUFSIZE);

    return 0;
}
