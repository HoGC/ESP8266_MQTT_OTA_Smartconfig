#include "osapi.h"
#include "mem.h"
#include "user_interface.h"
#include "mqtt.h"
#include "driver/ota.h"
#include "driver/data.h"
#include "driver/uart.h"
#include "driver/wifi.h"
#include "driver/gpio_key.h"    

#define MAIN_DEBUG_ON

#if defined(MAIN_DEBUG_ON)
#define INFO( format, ... ) os_printf( format, ## __VA_ARGS__ )
#else
#define INFO( format, ... )
#endif

u8 mac_str[13];					//mac地址

//MQTT参数 请在include/mqtt_config.h修改
u8 ota_topic[50]={""};				//ota升级话题
u8 lwt_topic[50]={""};				//遗嘱话题
u8 birth_topic[30]={""};			//出生话题

MQTT_Client mqttClient;

uint32 ICACHE_FLASH_ATTR user_rf_cal_sector_set(void)
{
    enum flash_size_map size_map = system_get_flash_size_map();
    uint32 rf_cal_sec = 0;

    switch (size_map) {
        case FLASH_SIZE_4M_MAP_256_256:
            rf_cal_sec = 128 - 5;
            break;

        case FLASH_SIZE_8M_MAP_512_512:
            rf_cal_sec = 256 - 5;
            break;

        case FLASH_SIZE_16M_MAP_512_512:
        case FLASH_SIZE_16M_MAP_1024_1024:
            rf_cal_sec = 512 - 5;
            break;

        case FLASH_SIZE_32M_MAP_512_512:
        case FLASH_SIZE_32M_MAP_1024_1024:
            rf_cal_sec = 1024 - 5;
            break;

        case FLASH_SIZE_64M_MAP_1024_1024:
            rf_cal_sec = 2048 - 5;
            break;
        case FLASH_SIZE_128M_MAP_1024_1024:
            rf_cal_sec = 4096 - 5;
            break;
        default:
            rf_cal_sec = 0;
            break;
    }

    return rf_cal_sec;
}

/**
 * 	ota升级回调
 */
void ICACHE_FLASH_ATTR ota_finished_callback(void * arg) {
	struct upgrade_server_info *update = arg;
	if (update->upgrade_flag == true) {
		INFO("OTA  Success ! rebooting!\n");
		system_upgrade_reboot();
	} else {
		INFO("OTA Failed!\n");
	}
}

/**
 * 获取MAC
 */
void ICACHE_FLASH_ATTR get_mac(void) {

	u8 mac[6];
	wifi_get_macaddr(STATION_IF, mac);
	HexToStr(mac_str, mac, 6, 1);
	INFO("mac:%s\n", mac_str);

	os_sprintf(ota_topic,OTA_TOPIC,mac_str);
	os_sprintf(lwt_topic,LWT_TOPIC,mac_str);
    os_sprintf(birth_topic,BIRTH_TOPIC,mac_str);
}

/**
 * 	MQTT连接回调
 */
void mqttConnectedCb(uint32_t *args) {
	MQTT_Client* client = (MQTT_Client*) args;
	INFO("MQTT: Connected\r\n");
	MQTT_Publish(client, birth_topic, BIRTH_MESSAGE, os_strlen(BIRTH_MESSAGE), 0,0);
	MQTT_Subscribe(client,ota_topic, 0);
	if(updata_status_check()){
		MQTT_Publish(client, ota_topic, "updata_finish", os_strlen("updata_finish"), 0,0);
	}
}
/**

 * 	MQTT断开连接回调
 */
void mqttDisconnectedCb(uint32_t *args) {
	MQTT_Client* client = (MQTT_Client*) args;
	INFO("MQTT: Disconnected\r\n");
}

/**
 * 	MQTT发布消息回调
 */
void mqttPublishedCb(uint32_t *args) {
	MQTT_Client* client = (MQTT_Client*) args;
	INFO("MQTT: Published\r\n");
}

/**
 * 	MQTT接收数据回调
 */
void mqttDataCb(uint32_t *args, const char* topic, uint32_t topic_len,
		const char *data, uint32_t data_len) {
	char *topicBuf = (char*) os_zalloc(topic_len + 1), *dataBuf =
			(char*) os_zalloc(data_len + 1);

	MQTT_Client* client = (MQTT_Client*) args;

	os_memcpy(topicBuf, topic, topic_len);
	topicBuf[topic_len] = 0;

	os_memcpy(dataBuf, data, data_len);
	dataBuf[data_len] = 0;

	INFO("Receive topic: %s, data: %s \r\n", topicBuf, dataBuf);

	//data = {"url"="http://yourdomain.com:9001/ota/"}
	if (os_strcmp(topicBuf, ota_topic) == 0) {
		char url_data[200];
		if(get_josn_str(dataBuf,"url",url_data)){
            INFO("ota_start\n");
            ota_upgrade(url_data,ota_finished_callback);
		}
	}

	os_free(topicBuf);
	os_free(dataBuf);
}

void ICACHE_FLASH_ATTR connect_mqtt(void){
    INFO("connect_mqtt\n");
    MQTT_Connect(&mqttClient);
}

/**
 * 	MQTT初始化
 */
void ICACHE_FLASH_ATTR mqtt_init(void) {

	MQTT_InitConnection(&mqttClient, MQTT_HOST, MQTT_PORT, DEFAULT_SECURITY);
	MQTT_InitClient(&mqttClient, mac_str, MQTT_USER,MQTT_PASS, MQTT_KEEPALIVE, 1);
	MQTT_InitLWT(&mqttClient, lwt_topic, LWT_MESSAGE, 0, 0);
	MQTT_OnConnected(&mqttClient, mqttConnectedCb);
	MQTT_OnDisconnected(&mqttClient, mqttDisconnectedCb);
	MQTT_OnPublished(&mqttClient, mqttPublishedCb);
	MQTT_OnData(&mqttClient, mqttDataCb);
}

/**
 * 	smartconfig配置回调
 */
void ICACHE_FLASH_ATTR smartconfig_cd(sm_status status){

	switch (status)
	{
		case SM_STATUS_FINISH:
			INFO("smartconfig_finish");
			break;
	
		case SM_STATUS_GETINFO:
			INFO("wifiinfo_error");
			break;
		case SM_STATUS_TIMEOUT:
			INFO("smartconfig_timeout");
			break;
	}
}

/**
 * 	按键短按回调
 */
LOCAL void ICACHE_FLASH_ATTR key1ShortPress(void) {

	start_smartconfig(smartconfig_cd);
	INFO("start_smartconfig\n");
}

/**
 * 	按键初始化
 */
LOCAL void ICACHE_FLASH_ATTR keyInit(void) {

	//设置按键数量
	set_key_num(1);
	//长按、短按的按键回调
	key_add(D5, NULL, key1ShortPress);

}
/**
 * 	WIFI连接回调
 */
void wifi_connect_cb(void){

	INFO("wifi connect!\r\n");
	MQTT_Connect(&mqttClient);
}

/**
 * 	WIFI断开回调
 */
void wifi_disconnect_cb(void){
	INFO("wifi disconnect!\r\n");
	MQTT_Disconnect(&mqttClient);
}

/**
 * 串口接收回调
 */ 
void uart_cb(uint8* data, uint16 data_len){
    data[data_len]='\0';
    INFO("uart data:%s\r\n",data);
}

void ICACHE_FLASH_ATTR user_init(void)
{
	uart_init(115200, 115200);
	os_delay_us(60000);
	set_uart_cb(uart_cb);

    get_mac();

	wifi_set_opmode(STATION_MODE); 
	//设置wifi信息存储数量
	wifi_station_ap_number_set(2);

	keyInit();
    mqtt_init();

	set_wifistate_cb(wifi_connect_cb, wifi_disconnect_cb);
}
