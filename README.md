# ESP8266_MQTT_OTA_Smartconfig
### ESP8266 MQTT 加入 OTA Smartconfig 按键 多wifi信息储存 同时适配D1 mini引脚
* 使用mqtt消息进行OTA升级
* ESPTOUCH AIRKISS
* 串口接收回调
* 可存储多个wifi信息，自动尝试连接
* 添加D1 mini引脚的适配
    * 设置按键回调：`key_add(D5, key1LongPress, key1ShortPress)`
    * I/O口初始化：`gpio_out_init(D4,1)`

