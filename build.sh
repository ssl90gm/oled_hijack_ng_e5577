#!/bin/bash
DIR=$HOME/ndk19

export PATH=$PATH:$DIR/bin
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$DIR/lib

export CFLAGS="-march=armv7-a -mfloat-abi=softfp -mfpu=vfpv3-d16 -mthumb -O2 -s -Wno-implicit-function-declaration -D__ANDROID_API__=19"
export CC=arm-linux-androideabi-clang

# device_webhook
${CC} -shared -ldl -fPIC -pthread -DHOOK -DSOCK_NAME='"/var/device_webhook"' ${CFLAGS} -o device_webhook.so web_hook.c
${CC} -fPIC -DCLIENT -DSOCK_NAME='"/var/device_webhook"' ${CFLAGS} -o device_webhook_client web_hook.c

# # sms_webhook
${CC} -shared -ldl -fPIC -pthread -DHOOK -DSOCK_NAME='"/var/sms_webhook"' ${CFLAGS} -o sms_webhook.so web_hook.c
${CC} -fPIC -DCLIENT -DSOCK_NAME='"/var/sms_webhook"' ${CFLAGS} -o sms_webhook_client web_hook.c

# # oled
${CC} -shared -W -ldl -fPIC ${CFLAGS} -o oled_hijack.so oled_hijack.c oled_paint.c oled_process.c oled_widgets.c

#make oled_hijack


