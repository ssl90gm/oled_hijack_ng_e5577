CC=armv7a-linux-androideabi19-clang

CFLAGS="-mthumb"


all: oled_hijack.so device_webhook.so device_webhook_client sms_webhook.so sms_webhook_client

oled_hijack.so: oled_hijack.c oled_paint.c oled_widgets.c oled_process.c oled.h oled_font.h
	$(CC) -W -shared -ldl -fPIC -O2 -s -o oled_hijack.so oled_hijack.c oled_paint.c oled_process.c oled_widgets.c

device_webhook.so: web_hook.c
	$(CC) $(CFLAGS) -static-libgcc -shared -ldl -fPIC -pthread -DHOOK -DSOCK_NAME='"/var/device_webhook"' -O2 -D__ANDROID_API__=19 -s -o device_webhook.so web_hook.c

device_webhook_client: web_hook.c
	$(CC) -fPIC -O2 -DCLIENT -DSOCK_NAME='"/var/device_webhook"' $(CFLAGS) -s -o device_webhook_client web_hook.c

sms_webhook.so: web_hook.c
	$(CC) -shared -ldl -fPIC $(CFLAGS) -O2 -s -pthread -DHOOK -DSOCK_NAME='"/var/sms_webhook"' -o sms_webhook.so web_hook.c

sms_webhook_client: web_hook.c
	$(CC) -static-libgcc -fPIC -O2 -DCLIENT -DSOCK_NAME='"/var/sms_webhook"' $(CFLAGS) -s -o sms_webhook_client web_hook.c

.PHONY: clean
clean:
	rm -f *.so device_webhook_client sms_webhook_client