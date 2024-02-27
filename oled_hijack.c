/*
 * Advanced OLED menu for Huawei E5785 portable LTE router.
 *
 */

#define _GNU_SOURCE
#include <stdlib.h>
#include <dlfcn.h>
#include <unistd.h>

#include <sys/capability.h>

#include "oled.h"

extern uint8_t is_small_screen;

extern struct lcd_screen secret_screen;
static uint8_t is_secret_screen_active = 0;

extern struct uint32_t active_widget;
extern struct led_widget *widgets;

extern void reset_widgets();
extern void dispatch_power_key();
extern void dispatch_menu_key();

extern void switch_to_small_screen_mode();

/*
 * Real handlers from oled binary and libraries
 */

int (*notify_handler_async_real)(int subsystemid, int action, int subaction) = NULL;
void (*lcd_refresh_screen_real)(struct lcd_screen *screen) = NULL;
int (*lcd_control_operate_real)(int lcd_mode) = NULL;

uint32_t (*timer_create_ex)(uint32_t, uint32_t, void (*)(), uint32_t) = 0;
uint32_t (*timer_delete_ex)(uint32_t) = 0;
uint32_t (*get_msgQ_id)(uint32_t) = 0;
uint32_t (*msgQex_send)(uint32_t, uint32_t *, uint32_t, uint32_t);

static void send_msg(uint32_t msg_type)
{
    const int DEFAULT_QUEUE_ID = 1001;

    uint32_t msg_queue = get_msgQ_id(DEFAULT_QUEUE_ID);
    if (!msg_queue)
    {
        fprintf(stderr, "Failed to get message queue to close the menu\n");
        return;
    }
    uint32_t msg[2] = {msg_type, 0};
    msgQex_send(msg_queue, msg, 2 * sizeof(uint32_t), 0);
}

int state = 0;
int shwon_secret_menu = 0;

void check_activate_show_menu(int buttonCode) {
    // Проверяем, является ли код кнопки одним из ожидаемых
    if (buttonCode != BUTTON_POWER && buttonCode != BUTTON_MENU) {
        return; // Выход, если нажата не та кнопка
    }

    // Для первых семи нажатий ожидаем BUTTON_POWER
    if (state >= 0 && state < 7) {
        if (buttonCode == BUTTON_POWER) {
            state++;
        } else {
            state = 0; // Сброс состояния при нажатии не той кнопки
        }
    } 
    // На восьмом нажатии ожидаем BUTTON_MENU
    else if (state == 7) {
        if (buttonCode == BUTTON_MENU) {
            shwon_secret_menu = !shwon_secret_menu; // Переключаем состояние секретного меню
            state = 0; // Сброс состояния после активации/деактивации меню
        } else {
            state = 0; // Сброс состояния при нажатии не той кнопки
        }
    }
}

/*
 * The main hijacked handler function.
 */
int notify_handler_async(int subsystemid, int action, int subaction)
{
    //fprintf(stderr, "notify_handler_async: %d, %d, %x\n", subsystemid, action, subaction);

    if(action == -1 && subaction == -2) {
        state = 0;
        is_secret_screen_active = !is_secret_screen_active;
        return 0;
    }

    if (subsystemid == SUBSYSTEM_GPIO)
    {
        check_activate_show_menu(action);

        if (shwon_secret_menu != 0)
        {
            shwon_secret_menu = 0;
            is_secret_screen_active = !is_secret_screen_active;
            reset_widgets();
            send_msg(UI_MENU_EXIT);
            // force restarting the led brightness timer if already fired
            if (!is_small_screen)
            {
                notify_handler_async_real(SUBSYSTEM_GPIO, BUTTON_POWER, 0);
            }
            else
            {
                notify_handler_async_real(SUBSYSTEM_GPIO, BUTTON_MENU, 0);
            }

            return 0;
        }

        if (is_secret_screen_active)
        {
            if (action == BUTTON_MENU)
            {
                dispatch_menu_key();
            }
            else if (action == BUTTON_POWER)
            {
                dispatch_power_key();
            }
            return 0;
        }
    }

    return notify_handler_async_real(subsystemid, action, subaction);
}

/*
 * Hijacked functions from various libraries.
 */

int lcd_control_operate(int lcd_mode)
{
    // we use other values in secret mode to have full control on lcd
    if (is_secret_screen_active)
    {
        if (lcd_mode < 100)
        {
            return 0;
        }
        else
        {
            return lcd_control_operate_real(lcd_mode - 100);
        }
    }
    else
    {
        if (lcd_mode >= 100)
        {
            return 0;
        }
        else
        {
            return lcd_control_operate_real(lcd_mode);
        }
    }
}

void lcd_refresh_screen(struct lcd_screen *screen)
{
    if (!is_secret_screen_active && screen == &secret_screen)
    {
        return;
    }
    if (is_secret_screen_active && screen != &secret_screen)
    {
        return;
    }
    if (!is_secret_screen_active && screen && screen->buf_len == 1024)
    {
        if (!is_small_screen)
        {
            switch_to_small_screen_mode();
        }
    }
    lcd_refresh_screen_real(screen);
}

int register_notify_handler(int subsystemid, void *notify_handler_sync, void *notify_handler_async_orig)
{
    unsetenv("LD_PRELOAD");

    static int (*register_notify_handler_real)(int, void *, void *) = NULL;
    register_notify_handler_real = dlsym(RTLD_NEXT, "register_notify_handler");
    lcd_refresh_screen_real = dlsym(RTLD_NEXT, "lcd_refresh_screen");
    lcd_control_operate_real = dlsym(RTLD_NEXT, "lcd_control_operate");

    timer_create_ex = dlsym(RTLD_DEFAULT, "osa_timer_create_ex");
    timer_delete_ex = dlsym(RTLD_DEFAULT, "osa_timer_delete_ex");

    get_msgQ_id = dlsym(RTLD_DEFAULT, "osa_get_msgQ_id");
    msgQex_send = dlsym(RTLD_DEFAULT, "osa_msgQex_send");

    if (!register_notify_handler_real || !lcd_refresh_screen_real ||
        !lcd_control_operate_real || !timer_create_ex || !timer_delete_ex || !get_msgQ_id ||
        !msgQex_send)
    {
        fprintf(stderr, "The program is not compatible with this device\n");
        return 1;
    }

    notify_handler_async_real = notify_handler_async_orig;
    return register_notify_handler_real(subsystemid, notify_handler_sync, notify_handler_async);
}

int setuid(uid_t u)
{
    // put uid to saved to be able to restore it when needed
    return setresuid(u, u, 0);
}

int setgid(gid_t g)
{
    return setresgid(g, g, 0);
}

int prctl(int option, unsigned long arg2, unsigned long arg3,
          unsigned long arg4, unsigned long arg5)
{
    // not allowing to drop capabilities
    UNUSED(option);
    UNUSED(arg2);
    UNUSED(arg3);
    UNUSED(arg4);
    UNUSED(arg5);
    return -1;
}

int capset(cap_user_header_t hdrp, const cap_user_data_t datap)
{
    datap[0].effective |= (1 << CAP_NET_RAW) | (1 << CAP_NET_ADMIN);
    datap[0].permitted |= (1 << CAP_NET_RAW) | (1 << CAP_NET_ADMIN);
    datap[0].inheritable |= (1 << CAP_NET_RAW) | (1 << CAP_NET_ADMIN);
    int (*capset_real)(cap_user_header_t, cap_user_data_t) = dlsym(RTLD_NEXT, "capset");
    if (!capset_real)
    {
        return -1;
    }

    return capset_real(hdrp, datap);
}
