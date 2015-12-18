/**
 * Copyright (c) 2015 Runtime Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#define HOSTCTLRTEST_ROLE_INITIATOR          (0)
#define HOSTCTLRTEST_ROLE_SCANNER            (1)
#define HOSTCTLRTEST_ROLE_ADVERTISER         (2)

#define HOSTCTLRTEST_CFG_ROLE                (0)

#include <assert.h>
#include <string.h>
#include <stdio.h>
#include "os/os.h"
#include "bsp/bsp.h"
#include "hal/hal_gpio.h"
#include "hal/hal_cputime.h"
#include "console/console.h"

/* BLE */
#include "nimble/ble.h"
#include "host/host_hci.h"
#include "host/ble_hs.h"
#include "host/ble_hs_uuid.h"
#include "host/ble_att.h"
#include "host/ble_gap.h"
#include "host/ble_gatt.h"
#include "controller/ble_ll.h"

/* Init all tasks */
volatile int tasks_initialized;

/* Task 1 */
#define HOST_TASK_PRIO      (1)

/* For LED toggling */
int g_led_pin;

/* Our global device address (public) */
uint8_t g_dev_addr[BLE_DEV_ADDR_LEN];

/* Our random address (in case we need it) */
uint8_t g_random_addr[BLE_DEV_ADDR_LEN];

/* A buffer for host advertising data */
uint8_t g_host_adv_data[BLE_HCI_MAX_ADV_DATA_LEN];
uint8_t g_host_adv_len;

static uint8_t hostctlrtest_slv_addr[6] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06};
//static uint8_t hostctlrtest_slv_addr[6] = {0x82, 0x6a, 0xd0, 0x48, 0xb4, 0xb0};
static uint8_t hostctlrtest_mst_addr[6] = {0x0f, 0x0e, 0x0d, 0x0c, 0x0b, 0x0a};

/* Create a mbuf pool of BLE mbufs */
#define MBUF_NUM_MBUFS      (8)
#define MBUF_BUF_SIZE       (256 + sizeof(struct hci_data_hdr))
#define MBUF_MEMBLOCK_SIZE  (MBUF_BUF_SIZE + BLE_MBUF_PKT_OVERHEAD)

#define MBUF_MEMPOOL_SIZE   OS_MEMPOOL_SIZE(MBUF_NUM_MBUFS, MBUF_MEMBLOCK_SIZE)

struct os_mbuf_pool g_mbuf_pool; 
struct os_mempool g_mbuf_mempool;
os_membuf_t g_mbuf_buffer[MBUF_MEMPOOL_SIZE];

/* Some application configurations */
#define HOSTCTLRTEST_CFG_FILT_DUP_ADV        (0)
#define HOSTCTLRTEST_CFG_ADV_ITVL            (500000 / BLE_HCI_ADV_ITVL)
#define HOSTCTLRTEST_CFG_ADV_TYPE            BLE_HCI_ADV_TYPE_ADV_IND
#define HOSTCTLRTEST_CFG_ADV_FILT_POLICY     (BLE_HCI_ADV_FILT_NONE)
#define HOSTCTLRTEST_CFG_SCAN_ITVL           (700000 / BLE_HCI_SCAN_ITVL)
#define HOSTCTLRTEST_CFG_SCAN_WINDOW         (650000 / BLE_HCI_SCAN_ITVL)
#define HOSTCTLRTEST_CFG_SCAN_TYPE           (BLE_HCI_SCAN_TYPE_ACTIVE)
#define HOSTCTLRTEST_CFG_SCAN_FILT_POLICY    (BLE_HCI_SCAN_FILT_NO_WL)
#define HOSTCTLRTEST_CFG_CONN_ITVL           (1000)  /* 1250 msecs */           
#define HOSTCTLRTEST_CFG_SLAVE_LATENCY       (0)
#define HOSTCTLRTEST_CFG_INIT_FILTER_POLICY  (BLE_HCI_CONN_FILT_NO_WL)
#define HOSTCTLRTEST_CFG_CONN_SPVN_TMO       (1000)  /* 10 seconds */
#define HOSTCTLRTEST_CFG_MIN_CE_LEN          (1000)    
#define HOSTCTLRTEST_CFG_MAX_CE_LEN          (HOSTCTLRTEST_CFG_CONN_ITVL * 2)

/* HOSTCTLRTEST variables */
#define HOSTCTLRTEST_STACK_SIZE              (256)
#define HOSTCTLRTEST_TASK_PRIO               (HOST_TASK_PRIO + 1)
uint32_t g_next_os_time;
int g_hostctlrtest_state;
struct os_eventq g_hostctlrtest_evq;
struct os_task hostctlrtest_task;
os_stack_t hostctlrtest_stack[HOSTCTLRTEST_STACK_SIZE];

void
bletest_inc_adv_pkt_num(void) { }


#if HOSTCTLRTEST_CFG_ROLE == HOSTCTLRTEST_ROLE_INITIATOR

static int
hostctlrtest_on_read(uint16_t conn_handle, uint8_t ble_hs_status,
                     uint8_t att_status, struct ble_gatt_attr *attr,
                     void *arg)
{
    uint8_t *u8ptr;
    int i;

    if (ble_hs_status != 0) {
        console_printf("characteristic read failure: ble_hs_status=%d "
                       "att_status=%d\n", ble_hs_status, att_status);
        return 0;
    }

    console_printf("characteristic read: handle=%d value=", attr->handle);
    u8ptr = attr->value;
    for (i = 0; i < attr->value_len; i++) {
        if (i != 0) {
            console_printf(":");
        }
        console_printf("%02x", u8ptr[i]);
    }
    console_printf("\n");

    return 0;
}

static int
hostctlrtest_on_disc_c(uint16_t conn_handle, uint8_t ble_hs_status,
                       uint8_t att_status, struct ble_gatt_chr *chr,
                       void *arg)
{
    int i;

    if (ble_hs_status != 0) {
        console_printf("characteristic discovery failure: ble_hs_status=%d "
                       "att_status=%d\n", ble_hs_status, att_status);
        return 0;
    }

    if (chr == NULL) {
        console_printf("characteristic discovery complete.\n");
        return 0;
    }

    console_printf("characteristic discovered: decl_handle=%d value_handle=%d "
                   "properties=%d uuid=", chr->decl_handle, chr->value_handle,
                   chr->properties);
    for (i = 0; i < 16; i++) {
        if (i != 0) {
            console_printf(":");
        }
        console_printf("%02x", chr->uuid128[i]);
    }
    console_printf("\n");

    ble_gatt_read(conn_handle, chr->value_handle, hostctlrtest_on_read, NULL);

    return 0;
}

static int
hostctlrtest_on_disc_s(uint16_t conn_handle, uint8_t ble_hs_status,
                       uint8_t att_status, struct ble_gatt_service *service,
                       void *arg)
{
    int i;

    if (ble_hs_status != 0) {
        console_printf("service discovery failure: ble_hs_status=%d "
                       "att_status=%d\n", ble_hs_status, att_status);
        return 0;
    }

    if (service == NULL) {
        console_printf("service discovery complete.");
        return 0;
    }

    console_printf("service discovered: start_handle=%d end_handle=%d, uuid=",
                   service->start_handle, service->end_handle);
    for (i = 0; i < 16; i++) {
        if (i != 0) {
            console_printf(":");
        }
        console_printf("%02x", service->uuid128[i]);
    }
    console_printf("\n");

    ble_gatt_disc_all_chars(conn_handle, service->start_handle,
                            service->end_handle, hostctlrtest_on_disc_c,
                            NULL);

    return 0;
}
#endif

#if HOSTCTLRTEST_CFG_ROLE == HOSTCTLRTEST_ROLE_ADVERTISER
static uint16_t hostctlrtest_service_handle;
static uint16_t hostctlrtest_char1_handle;
static uint16_t hostctlrtest_data1_handle;
static uint16_t hostctlrtest_char2_handle;
static uint16_t hostctlrtest_data2_handle;

static int
hostctlrtest_attr_cb(struct ble_att_svr_entry *entry, uint8_t op,
                     union ble_att_svr_handle_arg *arg)
{
    static uint8_t buf[128];

    assert(op == BLE_ATT_OP_READ_REQ);

    if (entry->ha_handle_id == hostctlrtest_service_handle) {
        console_printf("reading service declaration");
        htole16(buf, 0x1234);
        arg->aha_read.attr_data = buf;
        arg->aha_read.attr_len = 2;
    } else if (entry->ha_handle_id == hostctlrtest_char1_handle) {
        console_printf("reading characteristic1 declaration");

        /* Properties. */
        buf[0] = 0;

        /* Value handle. */
        htole16(buf + 1, hostctlrtest_data1_handle);

        /* UUID. */
        htole16(buf + 3, 0x5656);

        arg->aha_read.attr_data = buf;
        arg->aha_read.attr_len = 5;
    } else if (entry->ha_handle_id == hostctlrtest_data1_handle) {
        console_printf("reading characteristic1 value");
        memcpy(buf, "char1", 5);
        arg->aha_read.attr_data = buf;
        arg->aha_read.attr_len = 5;
    } else if (entry->ha_handle_id == hostctlrtest_char2_handle) {
        console_printf("reading characteristic2 declaration");

        /* Properties. */
        buf[0] = 0;

        /* Value handle. */
        htole16(buf + 1, hostctlrtest_data2_handle);

        /* UUID. */
        htole16(buf + 3, 0x6767);

        arg->aha_read.attr_data = buf;
        arg->aha_read.attr_len = 5;
    } else if (entry->ha_handle_id == hostctlrtest_data2_handle) {
        console_printf("reading characteristic2 value");
        memcpy(buf, "char2", 5);
        arg->aha_read.attr_data = buf;
        arg->aha_read.attr_len = 5;
    } else {
        assert(0);
    }

    return 0;
}

static void
hostctlrtest_register_attrs(void)
{
    uint8_t uuid128[16];
    int rc;

    /* Service. */
    rc = ble_hs_uuid_from_16bit(BLE_ATT_UUID_PRIMARY_SERVICE, uuid128);
    assert(rc == 0);
    rc = ble_att_svr_register(uuid128, 0, &hostctlrtest_service_handle,
                              hostctlrtest_attr_cb);
    assert(rc == 0);

    /* Characteristic 1 (UUID=0x5656).*/
    rc = ble_hs_uuid_from_16bit(BLE_ATT_UUID_CHARACTERISTIC, uuid128);
    assert(rc == 0);
    rc = ble_att_svr_register(uuid128, 0, &hostctlrtest_char1_handle,
                              hostctlrtest_attr_cb);
    assert(rc == 0);

    rc = ble_hs_uuid_from_16bit(0x5656, uuid128);
    assert(rc == 0);
    rc = ble_att_svr_register(uuid128, 0, &hostctlrtest_data1_handle,
                              hostctlrtest_attr_cb);
    assert(rc == 0);

    /* Characteristic 2 (UUID=0x6767).*/
    rc = ble_hs_uuid_from_16bit(BLE_ATT_UUID_CHARACTERISTIC, uuid128);
    assert(rc == 0);
    rc = ble_att_svr_register(uuid128, 0, &hostctlrtest_char2_handle,
                              hostctlrtest_attr_cb);
    assert(rc == 0);

    rc = ble_hs_uuid_from_16bit(0x6767, uuid128);
    assert(rc == 0);
    rc = ble_att_svr_register(uuid128, 0, &hostctlrtest_data2_handle,
                              hostctlrtest_attr_cb);
    assert(rc == 0);
}
#endif

static void
hostctlrtest_on_connect(struct ble_gap_conn_event *event, void *arg)
{
    console_printf("connection complete; handle=%d status=%d "
                   "peer_addr=%02x:%02x:%02x:%02x:%02x:%02x\n",
                   event->conn.handle, event->conn.status,
                   event->conn.peer_addr[0], event->conn.peer_addr[1],
                   event->conn.peer_addr[2], event->conn.peer_addr[3],
                   event->conn.peer_addr[4], event->conn.peer_addr[5]);

#if HOSTCTLRTEST_CFG_ROLE == HOSTCTLRTEST_ROLE_INITIATOR
    ble_gatt_disc_all_services(event->conn.handle, hostctlrtest_on_disc_s,
                               NULL);
#endif
}

/**
 * BLE test task 
 * 
 * @param arg 
 */
void
hostctlrtest_task_handler(void *arg)
{
    struct os_event *ev;
    struct os_callout_func *cf;
    int rc;

    /* We are initialized */
    console_printf("Starting BLE test task\n");

    /* Initialize eventq */
    os_eventq_init(&g_hostctlrtest_evq);

    /* Init hostctlrtest variables */
    g_hostctlrtest_state = 0;
    g_next_os_time = os_time_get();
    
    ble_gap_conn_set_cb(hostctlrtest_on_connect, NULL);

#if HOSTCTLRTEST_CFG_ROLE == HOSTCTLRTEST_ROLE_ADVERTISER
    hostctlrtest_register_attrs();
    console_printf("ble_gap_directed_connectable\n");
    rc = ble_gap_conn_direct_connectable(BLE_HCI_ADV_PEER_ADDR_PUBLIC,
                                         hostctlrtest_mst_addr);
#else
    console_printf("ble_gap_direct_connection_establishment\n");
    rc = ble_gap_conn_direct_connect(BLE_HCI_ADV_PEER_ADDR_PUBLIC,
                                     hostctlrtest_slv_addr);
#endif
    assert(rc == 0);

    while (1) {
        ev = os_eventq_get(&g_hostctlrtest_evq);
        switch (ev->ev_type) {
        case OS_EVENT_T_TIMER:
            cf = (struct os_callout_func *)ev;
            assert(cf->cf_func);
            cf->cf_func(cf->cf_arg);
            break;
        default:
            assert(0);
            break;
        }
    }
}

/**
 * init_tasks
 *  
 * Called by main.c after os_init(). This function performs initializations 
 * that are required before tasks are running. 
 *  
 * @return int 0 success; error otherwise.
 */
static int
init_tasks(void)
{
    int rc;

    os_task_init(&hostctlrtest_task, "hostctlrtest", hostctlrtest_task_handler,
                 NULL, HOSTCTLRTEST_TASK_PRIO, OS_WAIT_FOREVER,
                 hostctlrtest_stack, HOSTCTLRTEST_STACK_SIZE);

    tasks_initialized = 1;

    /* Initialize host HCI */
    rc = ble_hs_init(HOST_TASK_PRIO);
    assert(rc == 0);

    /* Initialize the BLE LL */
    ble_ll_init();

    return 0;
}

/**
 * main
 *  
 * The main function for the project. This function initializes the os, calls 
 * init_tasks to initialize tasks (and possibly other objects), then starts the 
 * OS. We should not return from os start. 
 *  
 * @return int NOTE: this function should never return!
 */
int
main(void)
{
    int i;
    int rc;
    uint32_t seed;

    /* Initialize OS */
    os_init();

    /* Set cputime to count at 1 usec increments */
    rc = cputime_init(1000000);
    assert(rc == 0);

    rc = os_mempool_init(&g_mbuf_mempool, MBUF_NUM_MBUFS, 
            MBUF_MEMBLOCK_SIZE, &g_mbuf_buffer[0], "mbuf_pool");

    rc = os_mbuf_pool_init(&g_mbuf_pool, &g_mbuf_mempool, MBUF_MEMBLOCK_SIZE, 
                           MBUF_NUM_MBUFS);
    assert(rc == 0);

    /* Dummy device address */
#if HOSTCTLRTEST_CFG_ROLE == HOSTCTLRTEST_ROLE_ADVERTISER
    memcpy(g_dev_addr, hostctlrtest_slv_addr, 6);
#else
    memcpy(g_dev_addr, hostctlrtest_mst_addr, 6);
#endif

    /* 
     * Seed random number generator with least significant bytes of device
     * address.
     */ 
    seed = 0;
    for (i = 0; i < 4; ++i) {
        seed |= g_dev_addr[i];
        seed <<= 8;
    }
    srand(seed);

    /* Set the led pin as an output */
    g_led_pin = LED_BLINK_PIN;
    gpio_init_out(g_led_pin, 1);

    /* Init the console */
    rc = console_init(NULL);
    assert(rc == 0);

    /* Init tasks */
    init_tasks();

    /* Start the OS */
    os_start();

    /* os start should never return. If it does, this should be an error */
    assert(0);

    return rc;
}
