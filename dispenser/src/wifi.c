#include <zephyr/net/wifi_mgmt.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(wifi, LOG_LEVEL_INF);

#define WIFI_SSID "YOUR_SSID"
#define WIFI_PSK  "YOUR_PASSWORD"
#define WIFI_SSID "infrastructure"
#define WIFI_PSK  "the-generated-password-from-the-portal"

static struct net_mgmt_event_callback wifi_cb;
static K_SEM_DEFINE(wifi_connected_sem, 0, 1);

static void wifi_event_handler(struct net_mgmt_event_callback *cb,
                                uint32_t mgmt_event, struct net_if *iface)
{
    if (mgmt_event == NET_EVENT_WIFI_CONNECT_RESULT) {
        LOG_INF("WiFi connected!");
        k_sem_give(&wifi_connected_sem);
    }
}

void connect_wifi(void)
{
    struct net_if *iface = net_if_get_default();
    struct wifi_connect_req_params params = {
        .ssid = WIFI_SSID,
        .ssid_length = strlen(WIFI_SSID),
        .psk = WIFI_PSK,
        .psk_length = strlen(WIFI_PSK),
        .channel = WIFI_CHANNEL_ANY,
        .security = WIFI_SECURITY_TYPE_PSK,
        .ignore_broadcast_ssid = 1,   // <-- add this for hidden network
    };  

    net_mgmt_init_event_callback(&wifi_cb, wifi_event_handler,
                                  NET_EVENT_WIFI_CONNECT_RESULT);
    net_mgmt_add_event_callback(&wifi_cb);
    net_mgmt(NET_REQUEST_WIFI_CONNECT, iface, &params, sizeof(params));

    k_sem_take(&wifi_connected_sem, K_FOREVER);
}