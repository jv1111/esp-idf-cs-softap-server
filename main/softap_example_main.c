/*  WiFi softAP Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_mac.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "lwip/err.h"
#include "lwip/sys.h"

#include <stdlib.h>
#include <unistd.h>
#include <sys/param.h>
#include <esp_http_server.h>
#include "esp_netif.h"
#include "esp_tls.h"

#include "cJSON.h"

/* The examples use WiFi configuration that you can set via project configuration menu.

   If you'd rather not, just change the below entries to strings with
   the config you want - ie #define EXAMPLE_WIFI_SSID "mywifissid"
*/
#define EXAMPLE_ESP_WIFI_SSID      "ESP32"
#define EXAMPLE_ESP_WIFI_PASS      "mysolongpassword"
#define EXAMPLE_ESP_WIFI_CHANNEL   1
#define EXAMPLE_MAX_STA_CONN       4

bool isLedOn = false;

static const char *TAG = "webserver";

//-----------------------HTTP SERVER--------------------------------------
static esp_err_t led_off_handler(httpd_req_t *req)
{
	esp_err_t error;
	ESP_LOGI(TAG, "LED OFF");
	isLedOn = false;
	const char *response = (const char *) req->user_ctx;
	error = httpd_resp_send(req, response, strlen(response));
	if(error != ESP_OK)
	{
		ESP_LOGI(TAG, "Error %d while sending Response", error);
	}else ESP_LOGI(TAG, "response sent successfully");
	return error;
}
static httpd_uri_t ledoff = {
    .uri       = "/ledoff",
    .method    = HTTP_GET,
    .handler   = led_off_handler,
	.user_ctx =  "<!DOCTYPE html><html><body><button type=\"button\" onclick=\"window.location.href = '/ledon'\">Led on</button></body></html>"
};

static esp_err_t led_handler_on(httpd_req_t *req)
{
	esp_err_t error;
	ESP_LOGI(TAG, "LED ON");
	isLedOn = true;
	const char *response = (const char *) req->user_ctx;
	error = httpd_resp_send(req, response, strlen(response));
	if(error != ESP_OK)
	{
		ESP_LOGI(TAG, "Error %d while sending Response", error);
	}else ESP_LOGI(TAG, "response sent successfully");
	return error;
}
static httpd_uri_t ledon = {
    .uri       = "/ledon",
    .method    = HTTP_GET,
    .handler   = led_handler_on,
	.user_ctx = "<!DOCTYPE html><html><body><button type=\"button\" onclick=\"window.location.href = '/ledoff'\">Led off</button></body></html>"
};

/* An HTTP POST handler */
static esp_err_t echo_post_handler(httpd_req_t *req) {
	char buf[100];
	int ret, remaining = req->content_len;

	while (remaining > 0) {
		/* Read the data for the request */
		if ((ret = httpd_req_recv(req, buf, MIN(remaining, sizeof(buf))))
				<= 0) {
			if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
				/* Retry receiving if timeout occurred */
				continue;
			}
			return ESP_FAIL;
		}

		/* Log data received */
		ESP_LOGI(TAG, "=========== RECEIVED DATA ==========");
		ESP_LOGI(TAG, "%.*s", ret, buf);
		ESP_LOGI(TAG, "====================================");

		 // Parse the received data as a JSON object
		cJSON *json = cJSON_Parse(buf);
		if (json == NULL) {
			ESP_LOGE(TAG, "Failed to parse JSON data");
			return ESP_FAIL;
		}
		// Read the value of the "on" field
		cJSON *onField = cJSON_GetObjectItem(json, "on");
		if (onField != NULL) {
			// Check if the value is true or false
			bool esp32OnVar = cJSON_IsTrue(onField);
			ESP_LOGI(TAG, "Received value of 'on': %s",
					esp32OnVar ? "true" : "false");
			isLedOn = esp32OnVar;
			// Perform actions based on the value of 'on'
			// ...

		} else {
			ESP_LOGE(TAG, "Failed to get value of 'on' field");
		}

		// Read the value of the "password" field as a string
		cJSON *passwordField = cJSON_GetObjectItem(json, "password");
		if (passwordField != NULL && passwordField->type == cJSON_String) {
			const char *passwordValue = passwordField->valuestring;
			ESP_LOGI(TAG, "Received value of 'password': %s", passwordValue);

			// Perform actions based on the value of 'password'
			// ...

		} else {
			ESP_LOGE(TAG, "Failed to get value of 'password' field");
		}

		// Delete the JSON object
		cJSON_Delete(json);
		remaining -= ret;
	}

	// End response
	const char *response = (const char *)req->user_ctx;
	httpd_resp_send(req, response, strlen(response));
	return ESP_OK;
}
static const httpd_uri_t echo = {
		.uri = "/echo",
		.method = HTTP_POST,
		.handler = echo_post_handler,
		.user_ctx = "{\"success\": true}"
};

esp_err_t http_404_error_handler(httpd_req_t *req, httpd_err_code_t err)
{
    /* For any other URI send 404 and close socket */
    httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Some 404 error message");
    return ESP_FAIL;
}

static httpd_handle_t start_webserver(void)
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.lru_purge_enable = true;

    // Start the httpd server
    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        // Set URI handlers
        ESP_LOGI(TAG, "Registering URI handlers");
        httpd_register_uri_handler(server, &ledoff);
        httpd_register_uri_handler(server, &ledon);
        httpd_register_uri_handler(server, &echo);
        return server;
    }

    ESP_LOGI(TAG, "Error starting server!");
    return NULL;
}

//static esp_err_t stop_webserver(httpd_handle_t server)
//{
//    // Stop the httpd server
//    return httpd_stop(server);
//}

//static void disconnect_handler(void* arg, esp_event_base_t event_base,
//                               int32_t event_id, void* event_data)
//{
//    httpd_handle_t* server = (httpd_handle_t*) arg;
//    if (*server) {
//        ESP_LOGI(TAG, "Stopping webserver");
//        if (stop_webserver(*server) == ESP_OK) {
//            *server = NULL;
//        } else {
//            ESP_LOGE(TAG, "Failed to stop http server");
//        }
//    }
//}

static void connect_handler(void* arg, esp_event_base_t event_base,
                            int32_t event_id, void* event_data)
{
    httpd_handle_t* server = (httpd_handle_t*) arg;
    if (*server == NULL) {
        ESP_LOGI(TAG, "Starting webserver");
        *server = start_webserver();
    }
}

void setupServer(){
	static httpd_handle_t server = NULL;
	ESP_ERROR_CHECK(esp_netif_init());

	ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_AP_STAIPASSIGNED, &connect_handler, &server));
	//    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &disconnect_handler, &server));
}
//-----------------------------------END SERVER-------------------------------------------------

//-----------------------------------WIFI-------------------------------------------------------
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                    int32_t event_id, void* event_data)
{
    if (event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
        ESP_LOGI(TAG, "station "MACSTR" join, AID=%d",
                 MAC2STR(event->mac), event->aid);
    } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
        ESP_LOGI(TAG, "station "MACSTR" leave, AID=%d",
                 MAC2STR(event->mac), event->aid);
    }
}

void wifi_init_softap(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));

    wifi_config_t wifi_config = {
        .ap = {
            .ssid = EXAMPLE_ESP_WIFI_SSID,
            .ssid_len = strlen(EXAMPLE_ESP_WIFI_SSID),
            .channel = EXAMPLE_ESP_WIFI_CHANNEL,
            .password = EXAMPLE_ESP_WIFI_PASS,
            .max_connection = EXAMPLE_MAX_STA_CONN,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK,
            .pmf_cfg = {
                    .required = true,
            },
        },
    };
    if (strlen(EXAMPLE_ESP_WIFI_PASS) == 0) {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "wifi_init_softap finished. SSID:%s password:%s channel:%d",
             EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS, EXAMPLE_ESP_WIFI_CHANNEL);
}

void setupSoftAp(){

    //Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_LOGI(TAG, "ESP_WIFI_MODE_AP");
    wifi_init_softap();
}
//-------------------------------------END WIFI--------------------------------------------------
void app_main(void)
{
	setupSoftAp();
	setupServer();
	while(1){
		if(isLedOn){
			printf("led is on\n");
		}
		sleep(1);
	}
}
