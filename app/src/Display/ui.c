#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/sys/print.h>
#include <zephyr/drivers/display.h>

#include <lvgl.h>

#include "ui.h"
static const struct device *display_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));
static lv_obj_t *screen = NULL;