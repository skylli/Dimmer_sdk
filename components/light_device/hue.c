/*
 * @Author: sky
 * @Date: 2020-03-09 18:34:28
 * @LastEditTime: 2020-03-09 18:53:03
 * @LastEditors: Please set LastEditors
 * @Description: ota
 * @FilePath: \mqtt_example\components\light_device\light_handle.c
 */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <string.h>

#include "mwifi.h"
#include "mupgrade.h"
#include "mlink.h"

#include "utlis.h"
#include "event_queue.h"
#include "light_device.h"
#include "light_device_config.h"

#define FLASH_KEY_HUB	"f_hue_info"
static const char *TAG          = "hue"; 

typedef enum{
	HUE_NONE,
	HUE_SET,
	HUE_RELAY,
	
	HUE_MAX
}OTA_status_t;
	
char p_hub_conn[128];

void hue_set(mlink_handle_data_t* p_mdata){
    int code = 400, rc = 0;
    
    char ipaddrbuf[30]={0};
	char usrname[55]={0};
	char *p_data = NULL, *p_rep = NULL;

	MDF_LOGE("get data %s\n", p_mdata->req_data);
	if( !mlink_json_parse(p_mdata->req_data,"data", &p_data) && p_data && strlen(p_data) > 0 \
		&& !mlink_json_parse(p_data,"IP", ipaddrbuf) && strlen(ipaddrbuf) < 16 \
		&& !mlink_json_parse(p_data,"HueBridgeUsername", usrname)){

		memset(p_hub_conn, 0, 128);
		sprintf(p_hub_conn,"http://%s/api/%s/",ipaddrbuf,usrname);
		mdf_info_save(FLASH_KEY_HUB, p_hub_conn,sizeof(p_hub_conn));
		code = 200;
    }else{
		code = 404;
		p_rep =(char *)malloc_copy_str("Failt Not data\n");
	}

	rc = mevt_command_respond_creat((char *)p_mdata->req_data, code, p_rep );
	MDF_FREE( p_rep);	
	MDF_FREE( p_data);
}
static char *p_g_http_method_str[]={
	"GET",
	"POST",
	"PUT",
	"PATCH",
	"DELETE"
	};


esp_err_t _http_event_handle(esp_http_client_event_t *evt)
{
   
    switch(evt->event_id) {
        case HTTP_EVENT_ERROR:
            ESP_LOGD(TAG, "HTTP_EVENT_ERROR");
			
            break;
        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");
			
            break;
        case HTTP_EVENT_HEADER_SENT:
            ESP_LOGD(TAG, "HTTP_EVENT_HEADER_SENT");
            break;
        case HTTP_EVENT_ON_HEADER:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER");
            //printf("%.*s", evt->data_len, (char*)evt->data);
            break;
        case HTTP_EVENT_ON_DATA:
            MDF_LOGD("HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
			MDF_LOGD("%d %s", evt->data_len, (char*)evt->data);
            if (!esp_http_client_is_chunked_response(evt->client)) {
                MDF_LOGD("%d, %s", evt->data_len, (char*)evt->data);
            }			
            break;
        case HTTP_EVENT_ON_FINISH:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");			
            break;

        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGD(TAG, "HTTP_EVENT_DISCONNECTED");
			
            break;
    }
    return ESP_OK;
}

// in use
int http_perform_as_stream_reader(char* connect_ip,int _method,char* bodystr,char** recvbody)
{
    char *buffer = MDF_MALLOC(  CNF_MAX_APP_LEN + 1);
    if (buffer == NULL) {
        ESP_LOGE(TAG, "Cannot MDF_MALLOC http receive buffer");
        return 0;
    }
    esp_http_client_config_t config = {
        .url = connect_ip,
        .event_handler = _http_event_handle,
    };
		
    esp_http_client_handle_t client = esp_http_client_init(&config);
	struct esp_http_client *p_clinet = (struct esp_http_client *) client;
	esp_http_client_set_method(client, _method);
	int writlen=0;
	if(bodystr){
    	esp_http_client_set_post_field(client, bodystr, strlen(bodystr));
		writlen=strlen(bodystr);
		}
    esp_err_t err;
    if ((err = esp_http_client_open(client,writlen )) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open HTTP connection: %s", esp_err_to_name(err));
		esp_http_client_close(client);
        esp_http_client_cleanup(client);
        free(buffer);
        return 0;
    }
	if(bodystr)
	esp_http_client_write(client,bodystr,writlen);
    int content_length = CNF_MAX_APP_LEN ;
    esp_http_client_fetch_headers(client);
	if( esp_http_client_is_chunked_response(client) ){
	}
		
    int  read_len = 0;
	if ( esp_http_client_is_chunked_response(client) ) {
		 
        read_len = esp_http_client_read(client, buffer, content_length);
        if (read_len <= 0) {
            ESP_LOGE(TAG, "Error read data");
        }
        buffer[read_len] = 0;
        ESP_LOGI(TAG, "read_len = %d", read_len);
    }
	int ret=0;
    ESP_LOGI(TAG, "HTTP Stream reader Status = %d, content_length = %d",
                   ret= esp_http_client_get_status_code(client),
                    esp_http_client_get_content_length(client));
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
	if(recvbody)
		*recvbody=buffer;
	else
       MDF_FREE(buffer);
	return ret;
}

// send out http to philips hue
// Note: only the root node can sent it out.
esp_err_t hue_active( const char *p_rq, char **pp_http_recv){

	int ret = -1;
	char *p_buff = NULL, *p_url = NULL;
	char methodbuf[10]={0};
	esp_http_client_method_t meth_idex=0;

	if( !p_rq || strlen(p_rq) == 0  || strlen(p_hub_conn) == 0 ){
		MDF_LOGW("illiagle arg \n");
		return -1;
	}
	p_buff = MDF_MALLOC( strlen(p_rq) + 1 );
	if(!p_buff){
		MDF_LOGW("mdf alloc failt!\n");
		return -1;
	}
	bzero(p_buff, strlen(p_rq) + 1);
	if( mlink_json_parse(p_rq,"method", p_buff) )
		goto _hue_active;
	for(meth_idex=0; meth_idex < 5; meth_idex++){
		if(!strcasecmp(p_buff,p_g_http_method_str[meth_idex])){
			break;
			}
		}

	if( meth_idex >= 5 ){
		goto _hue_active;
	}
	// get  url
	bzero(p_buff, strlen(p_rq) + 1);
	mlink_json_parse(p_rq,"url", p_buff);
	
	if(strlen(p_buff) > 0){
		int url_len = strlen(p_buff) + strlen(p_hub_conn);
		p_url = MDF_MALLOC( url_len + 1);
		if(p_url){
			bzero(p_url, url_len + 1);
			sprintf(p_url,"%s%s",p_hub_conn, p_buff);
		
			MDF_LOGW("url %s \n", p_url);
		}
		bzero(p_buff, strlen(p_rq) + 1);
	}
	// body
	mlink_json_parse(p_rq,"body", p_buff);
	MDF_LOGD("p_buff %s\n", p_buff);
	MDF_LOGD("send  %s  %s to url %s \n", p_g_http_method_str[meth_idex], p_url, p_buff);
	
	if( 0 == http_perform_as_stream_reader(p_url, meth_idex, p_buff, pp_http_recv)){
		MDF_LOGW("hue send http rq failt !!\n");
	}
	if(pp_http_recv && *pp_http_recv)
		MDF_LOGE("recv %s \n", *pp_http_recv);

	ret = 0;
	
_hue_active:
	MDF_FREE(p_buff);
	MDF_FREE(p_url);
	return ret;
}
// only root node
void hue_relay(mlink_handle_data_t* p_http_data){

	char *p_data = NULL, *p_rq_body = NULL, *p_recv = NULL;

	MDF_LOGE("in %d :  hue_relay %s \n", p_http_data->req_size, p_http_data->req_data);

	if( !DEV_IS_ROOT() || !p_http_data->req_data || p_http_data->req_size <=0 ) 
		return;
	
	//get body
	mlink_json_parse(p_http_data->req_data,"data",&p_data);
	if(p_data && strlen(p_data) > 0 ){
		mlink_json_parse(p_data,"Hue_request",&p_rq_body);
		if(p_rq_body && strlen(p_rq_body) > 0){
			
			MDF_LOGW("p_rq_body %s\n", p_rq_body);
			if( 0 == hue_active(p_rq_body, & p_recv ) ){
					MDF_LOGD("http receive %s\n", p_recv);
					mevt_command_respond_creat((char *)p_http_data->req_data, 200, p_recv );
				}
		}else{
			mevt_command_respond_creat((char *)p_http_data->resp_data, 300, "Failt to control philips devices!" );
		}
	}else{
		mevt_command_respond_creat((char *)p_http_data->resp_data, 300, "Failt to control philips devices!" );
	}
	
	MDF_FREE( p_data );
	MDF_FREE( p_recv );
	MDF_FREE( p_rq_body );

}

void hue_init(void){
	esp_log_level_set(TAG, ESP_LOG_DEBUG);

	mdf_info_load(FLASH_KEY_HUB, p_hub_conn,sizeof(p_hub_conn));
	
	MDF_ERROR_ASSERT(mlink_set_handle("hue_relay", hue_relay));
	MDF_ERROR_ASSERT(mlink_set_handle("hue_set", hue_set) );

}
