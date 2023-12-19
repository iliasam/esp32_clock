// Get wearther from Yandex
//Set weather_values.is_actual_flag if parsed OK

#include "clock_config.h"
#include <time.h>
#include <sys/time.h>
#include "esp_log.h"
#include "protocol_examples_common.h"
#include "esp_sntp.h"
#include "esp_system.h"
#include "esp_event.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/dns.h"
#include "keys.h"

#include "weather_json_parse.h"

#include "weather_update.h"

#define NIGHT_THRESHOLD_HOUR    (6)

#define WEATHER_RX_BUF_SIZE     (2000)

#define WEATHER_WEB_SERVER      "api.weather.yandex.ru"
#define WEATHER_WEB_PORT        "80"

static const char *TAG2 = "WEATHER_UPDATE";

static const char *WEATHER_REQUEST = "GET /v2/informers/?lat=55.831308&lon=37.304726 HTTP/1.1\r\n"
    "Host: "WEATHER_WEB_SERVER":80\r\n"
    "User-Agent: esp-idf/1.0 esp32\r\n"
    WEATHER_KEY
    "Accept: */*\r\n"
    "\r\n";

typedef enum
{
    WEATHER_OK = 0,
    NEED_NET_RECONNECT,
    PARSE_FAIL,
    PARSE_PARTIALLY_FAIL,
    NET_NOT_CONNECTED,
} weather_update_result;

typedef struct 
{
    uint8_t     update_in_process;
    time_t      last_update_timestamp;
} weather_update_state_t;

weather_update_state_t weather_update_state;

///Buffer for JSON
char weather_rx_buf[WEATHER_RX_BUF_SIZE];

extern weather_values_t weather_values;

//***********************************************************************

weather_update_result weather_update_get_data(void);
weather_update_result weather_update_send_process_request(void);
bool weather_update_make_tries(void);
void  weather_update_print_result(weather_update_result res);

//***************************************************************************

/// @brief Called from the task
/// @param  
void weather_update_handler(void)
{
    //periodic update here
    time_t now_binary;
    struct tm timeinfo;

    now_binary = time(NULL);
    time_t diff_s = now_binary - weather_update_state.last_update_timestamp;
    localtime_r(&now_binary, &timeinfo);

    time_t update_period_s = WEATHER_UPDATE_PERIOD_S;
    if (timeinfo.tm_hour < NIGHT_THRESHOLD_HOUR)
        update_period_s = WEATHER_UPDATE_PERIOD_HIGHT_S;//longer period at hight

    if ((diff_s > update_period_s) || (diff_s < 0) ||
        (weather_update_state.last_update_timestamp == 0))
    {
        weather_update_state.update_in_process = 1;
        weather_update_make_tries();
        weather_update_state.update_in_process = 0;
        weather_update_state.last_update_timestamp = time(NULL);
    }  
    return;
}

bool weather_update_is_updating(void)
{
    return (weather_update_state.update_in_process != 0);
}

/// @brief Chech that parsed values are too old
/// @param  
/// @return Return 1 if parsed weather values are olde that doubled update period
bool weather_update_is_too_old(void)
{
    struct tm timeinfo;
    time_t now_binary = time(NULL);
    time_t diff_s = now_binary - weather_values.good_parse_timestamp;
    localtime_r(&now_binary, &timeinfo);

    time_t update_period_s = WEATHER_UPDATE_PERIOD_S;
    if (timeinfo.tm_hour < NIGHT_THRESHOLD_HOUR)
        update_period_s = WEATHER_UPDATE_PERIOD_HIGHT_S;//longer period at hight
    update_period_s = update_period_s * 2;//wait more
    if ((diff_s > update_period_s) || (diff_s < 0))
        return true;
    else
        return false;
}

bool weather_update_make_tries(void)
{
    weather_update_result update_res;
    for (uint8_t try_cnt = 0; try_cnt < 3; try_cnt++)
    {
        update_res = weather_update_get_data();
        if ((update_res == WEATHER_OK) || (update_res == PARSE_PARTIALLY_FAIL))
            return true;
    }
    return false;
}

/// @brief Connect to wifi, send request, parse data, disconnect wifi
/// @param  
weather_update_result weather_update_get_data(void)
{
#ifdef LWIP_DHCP_GET_NTP_SRV
    sntp_servermode_dhcp(1);
#endif

    if (example_is_wifi_enabled()) //this code is simple so it is expecting that wifi is off
        return NET_NOT_CONNECTED;

    ESP_LOGI(TAG2, "Need to get weather");
    esp_err_t res = example_connect();
    if (res != ESP_OK)
    {
        ESP_LOGI(TAG2, "Error - can't connect to WIFI");
        ESP_LOGI(TAG2, "Force Disconnect WIFI");
        ESP_ERROR_CHECK(example_disconnect());
        return NET_NOT_CONNECTED;
    }

    weather_update_result update_res;
    uint8_t try_cnt = 0;
    for (try_cnt = 0; try_cnt < 3; try_cnt++)
    {
        update_res = weather_update_send_process_request();
        if ((update_res == PARSE_FAIL))
            continue;
        else
            break;
    }
    
    ESP_LOGI(TAG2, "Disconnect WIFI");
    ESP_ERROR_CHECK(example_disconnect());
    weather_update_print_result(update_res);
    return update_res;
}

/// @brief Send request to the weather server and process received data
/// @param  
/// @return see weather_update_result type
weather_update_result weather_update_send_process_request(void)
{
    const struct addrinfo hints = {
        .ai_family = AF_INET,
        .ai_socktype = SOCK_STREAM,
    };
    struct addrinfo *res;
    struct in_addr *addr;
    int s;
    int rx_size;

    int err = getaddrinfo(WEATHER_WEB_SERVER, WEATHER_WEB_PORT, &hints, &res);

    if(err != 0 || res == NULL) 
    {
        ESP_LOGE(TAG2, "DNS lookup failed err=%d res=%p", err, res);
        return NEED_NET_RECONNECT;
    }

    addr = &((struct sockaddr_in *)res->ai_addr)->sin_addr;
    ESP_LOGI(TAG2, "DNS lookup succeeded. IP=%s", inet_ntoa(*addr));

    s = socket(res->ai_family, res->ai_socktype, 0);
    if(s < 0) 
    {
        ESP_LOGE(TAG2, "Failed to allocate socket.");
        freeaddrinfo(res);
        return NEED_NET_RECONNECT;
    }

    if(connect(s, res->ai_addr, res->ai_addrlen) != 0) 
    {
        ESP_LOGE(TAG2, "Socket connect failed errno=%d", errno);
        close(s);
        freeaddrinfo(res);
        return NEED_NET_RECONNECT;
    }

    ESP_LOGI(TAG2, "Connected");
    freeaddrinfo(res);

    weather_values.last_parse_result = -10;
    weather_values.last_bytes_rx = 0;
    weather_values.is_actual_flag = 0;

    if (write(s, WEATHER_REQUEST, strlen(WEATHER_REQUEST)) < 0)
    {
        ESP_LOGE(TAG2, "Socket send failed");
        close(s);
        return NEED_NET_RECONNECT;
    }
    ESP_LOGI(TAG2, "Socket send success");

    struct timeval receiving_timeout;
    receiving_timeout.tv_sec = 5;
    receiving_timeout.tv_usec = 0;
    if (setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &receiving_timeout,
        sizeof(receiving_timeout)) < 0) 
    {
        ESP_LOGE(TAG2, "Failed to set socket receiving timeout");
        close(s);
        return PARSE_FAIL;
    }

    bzero(weather_rx_buf, sizeof(weather_rx_buf));
    uint16_t main_buf_rx_cnt = 0;
    char recv_buf[64];
    ESP_LOGI(TAG2, "Start read");
    int32_t total_rx_size = 0;
    do {
        bzero(recv_buf, sizeof(recv_buf));
        rx_size = read(s, recv_buf, sizeof(recv_buf)-1);
        total_rx_size += rx_size;
        for(int i = 0; i < rx_size; i++)
        {
            if (main_buf_rx_cnt < WEATHER_RX_BUF_SIZE)
            {
                weather_rx_buf[main_buf_rx_cnt] = recv_buf[i];
                main_buf_rx_cnt++;
            }
            //putchar(recv_buf[i]);
        }
    } while(rx_size > 0);

    ESP_LOGI(TAG2, "Total bytes rx: %li", total_rx_size);
    ESP_LOGI(TAG2, "Socket closed");
    close(s);
    
    int32_t parse_res = -20;
    if (total_rx_size > 100)
        parse_res = parse_weather_json(weather_rx_buf);
    else
         ESP_LOGI(TAG2, "Parse fail - nothing to parse!");

    ESP_LOGI(TAG2, "State1: %i", weather_values.forecast1_state);
    ESP_LOGI(TAG2, "State2: %i", weather_values.forecast2_state);
    ESP_LOGI(TAG2, "Parse res: %li", parse_res);

    ESP_LOGI(TAG2, "sunrise: %d-%d", weather_values.sunrise_time.tm_hour, weather_values.sunrise_time.tm_min);
    ESP_LOGI(TAG2, "sunset: %d-%d", weather_values.sunset_time.tm_hour, weather_values.sunset_time.tm_min);
    
    weather_values.last_parse_result = parse_res;
    weather_values.last_bytes_rx = total_rx_size;

    if (weather_values.is_actual_flag == 0)
    {
        ESP_LOGI(TAG2, "RX DATA: %s", weather_rx_buf);

        if (parse_res == -20)
            return NEED_NET_RECONNECT;

        if (parse_res <= -2)
            return PARSE_PARTIALLY_FAIL;//partially parsed, so do not try again - becase tries are limited by Yandex
        else
            return PARSE_FAIL;
    }

    return WEATHER_OK;
}

void  weather_update_print_result(weather_update_result res)
{
    switch (res)
    {
        case WEATHER_OK:
            ESP_LOGI(TAG2, "Final result: WEATHER_OK");
            break;
        case NEED_NET_RECONNECT:
            ESP_LOGI(TAG2, "Final result: NEED_NET_RECONNECT");
            break;
        case PARSE_FAIL:
            ESP_LOGI(TAG2, "Final result: PARSE_FAIL");
            break;
        case PARSE_PARTIALLY_FAIL:
            ESP_LOGI(TAG2, "Final result: PARSE_PARTIALLY_FAIL");
            break;
        case NET_NOT_CONNECTED:
            ESP_LOGI(TAG2, "Final result: NET_NOT_CONNECTED");
            break;
        default:
            ESP_LOGI(TAG2, "Final result: UNKNOWN");
            break;

    }
}

