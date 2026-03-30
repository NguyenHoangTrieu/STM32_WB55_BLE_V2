/**
  ******************************************************************************
  * @file    ble_connection.c
  * @brief   BLE Connection Management implementation
  * @author  BLE Gateway
  ******************************************************************************
  */

#include "ble_connection.h"
#include "ble_device_manager.h"
#include "debug_trace.h"
#include "at_command.h"
#include "ble_gap_aci.h"
#include "ble_hci_le.h"
#include "hw_if.h"
#include "app_conf.h"
#include <string.h>

/* Procedure code for direct connection establishment (STM32WB GAP) */
#define GAP_DIRECT_CONN_PROC  (0x40U)

extern void AT_Response_Send(const char *fmt, ...);

typedef struct {
    uint16_t conn_handle;
    BLE_ConnectionState_t state;
    uint8_t mac_addr[BLE_MAC_LEN];
} ConnectionInfo_t;

static ConnectionInfo_t connections[MAX_BLE_CONNECTIONS];
static uint8_t connection_count = 0;

/* Connection attempt timeout tracking */
static uint8_t conn_timeout_timer_id = 0xFFU;
static volatile uint8_t conn_in_progress = 0;
/* Timeout for connection attempt: 10 seconds
 * For STM32WB55 standard config: LSE=32768Hz, CFG_RTCCLK_DIV=16
 * CFG_TS_TICK_VAL = (16*1000000)/32768 = 488.28 us per tick
 * 10 seconds = 10,000,000 us / 488.28 = 20,480 ticks */
static const uint32_t conn_timeout_ticks = 20480U;

/* Disconnect retry timer — fires from RTC wakeup ISR.
 * The ISR sets disc_retry_pending then schedules the sequencer task.
 * hci_disconnect() itself is called from task context (safe for IPCC). */
static uint8_t          disc_retry_timer_id  = 0xFFU;
static volatile uint16_t pending_disc_handle  = 0xFFFFU;
static volatile uint8_t  disc_retry_pending   = 0U;
static volatile uint8_t  disc_retry_count     = 0U;
static volatile uint32_t disc_retry_arm_tick  = 0U;  /* timestamp for polling-based fallback */

#define MAX_DISC_RETRIES  5U    /* max hci_disconnect retries before force-clear */
#define DISC_RETRY_TICKS  410U  /* ~200ms = 200000us / 488us/tick */
#define DISC_RETRY_TIMEOUT_TICKS  600U  /* polling fallback: if ISR doesn't fire after 600 ticks (~300ms) */

static void ConnTimeout_Callback(void)
{
    if (!conn_in_progress) {
        return;
    }
    conn_in_progress = 0;
    DEBUG_ERROR("Connection attempt timed out");
    /* Cancel the pending direct connection establishment procedure */
    aci_gap_terminate_gap_proc(GAP_DIRECT_CONN_PROC);
    AT_Response_Send("+CONN_ERROR:TIMEOUT\r\n");
}

/* DiscRetry_Callback — runs in RTC wakeup ISR context.
 * MUST NOT call hci_disconnect() here: IPCC (CPU1⇄CPU2 mailbox) is not
 * interrupt-safe.  Only set a flag and wake the sequencer task.
 * NO printf/DEBUG_INFO here — HAL_UART_Transmit blocks if SysTick is
 * preempted by this ISR, causing an infinite spin. */
static void DiscRetry_Callback(void)
{
    if (pending_disc_handle == 0xFFFFU) {
        return;
    }
    disc_retry_pending = 1U;
    extern void AT_ScheduleTask(void);
    AT_ScheduleTask();
}

/* Polling-based fallback: if ISR doesn't fire, manually trigger retry.
 * Called from AT_ProcessReady to check if timeout has been reached. */
void DiscRetry_PollCheck(void)
{
    if (pending_disc_handle == 0xFFFFU) {
        return;  /* no pending retry */
    }
    if (disc_retry_pending != 0) {
        return;  /* already triggered by ISR */
    }
    if (disc_retry_count > 0) {
        return;  /* already retrying */
    }
    
    /* Fallback: check if enough task cycles have passed without ISR firing
     * If we've gotten here multiple times without the ISR, manually trigger */
    static uint8_t poll_count = 0;
    poll_count++;
    
    /* After ~10-20 task cycles (~50-100ms at normal rate), if ISR still hasn't fired,
     * manually trigger as fallback */
    if (poll_count > 10) {
        DEBUG_WARN("[FALLBACK] ISR didn't fire after %u polls, manually triggering", (unsigned)poll_count);
        disc_retry_pending = 1U;
        extern void AT_ScheduleTask(void);
        AT_ScheduleTask();
        poll_count = 0;
    }
}

void BLE_Connection_Init(void)
{
    HW_TS_ReturnStatus_t ts_ret;
    uint8_t i;
    
    for (i = 0; i < MAX_BLE_CONNECTIONS; i++) {
        connections[i].conn_handle = 0xFFFF;
        connections[i].state = CONN_STATE_IDLE;
    }
    connection_count = 0;
    conn_in_progress = 0;
    pending_disc_handle = 0xFFFF;
    disc_retry_count    = 0U;

    /* Create the connection-timeout timer (single-shot) */
    ts_ret = HW_TS_Create(CFG_TIM_CONN_TIMEOUT_ID,
                          &conn_timeout_timer_id,
                          hw_ts_SingleShot,
                          ConnTimeout_Callback);
    DEBUG_INFO("Conn timeout timer create: proc=%u timer_id=%u ret=%u",
               (unsigned)CFG_TIM_CONN_TIMEOUT_ID,
               (unsigned)conn_timeout_timer_id,
               (unsigned)ts_ret);

    /* Create the disconnect-retry timer (single-shot, 100ms).
     * Used when hci_disconnect returns 0x01 (CONTROLLER_BUSY).
     * 100ms = 100000us / 488us/tick (CFG_TS_TICK_VAL) ~ 205 ticks */
    ts_ret = HW_TS_Create(CFG_TIM_DISCONNECT_RETRY_ID,
                          &disc_retry_timer_id,
                          hw_ts_SingleShot,
                          DiscRetry_Callback);
    DEBUG_INFO("Disc retry timer: timer_id=%u ret=%u", (unsigned)disc_retry_timer_id, (unsigned)ts_ret);

    DEBUG_INFO("Connection Manager initialized");
}

/* Force-clear all internal connection state for a handle without sending a
 * real BLE disconnect.  Used only when all retries are exhausted. */
static void disc_force_clear(uint16_t hdl)
{
    uint8_t i;
    int fc_idx = BLE_DeviceManager_FindConnHandle(hdl);
    if (fc_idx >= 0) {
        BLE_DeviceManager_UpdateConnection(fc_idx, hdl, 0);
    }
    for (i = 0; i < MAX_BLE_CONNECTIONS; i++) {
        if (connections[i].conn_handle == hdl) {
            connections[i].conn_handle = 0xFFFF;
            connections[i].state       = CONN_STATE_IDLE;
            if (connection_count > 0) { connection_count--; }
            break;
        }
    }
    DEBUG_INFO("disc_force_clear: hdl=0x%04X retry_count=%u", hdl, (unsigned)disc_retry_count);
    pending_disc_handle = 0xFFFFU;
    disc_retry_count    = 0U;
    AT_Response_Send("+DISCONNECTED:0x%04X\r\n", hdl);
}

/* Called from AT_Command_ProcessReady() (task context) to execute the
 * deferred disconnect retry scheduled by DiscRetry_Callback (ISR context).
 * Returns 1 if a retry was processed (caller should return early), 0 otherwise. */
int BLE_Connection_ProcessRetry(void)
{
    uint16_t hdl;
    tBleStatus ret;

    if (!disc_retry_pending) { return 0; }
    disc_retry_pending = 0U;

    /* Read handle WITHOUT clearing it — we may need to re-schedule another retry. */
    hdl = pending_disc_handle;
    if (hdl == 0xFFFFU) {
        DEBUG_INFO("Disconnect retry wake with invalid handle (spurious)");
        return 1;
    }

    disc_retry_count++;
    DEBUG_INFO("Disconnect retry #%u/%u (task ctx): hdl=0x%04X timer_id=%u pending_state=%u",
               (unsigned)disc_retry_count,
               (unsigned)MAX_DISC_RETRIES,
               hdl,
               (unsigned)disc_retry_timer_id,
               (unsigned)disc_retry_pending);
    DEBUG_INFO("Before hci_disconnect: retry_count=%u max=%u handle=0x%04X",
               (unsigned)disc_retry_count,
               (unsigned)MAX_DISC_RETRIES,
               hdl);
    ret = hci_disconnect(hdl, 0x13);
    DEBUG_INFO("hci_disconnect returned: 0x%02X (0=success, 0x01=busy)", ret);

    if (ret == BLE_STATUS_SUCCESS) {
        /* HCI accepted the command — wait for HCI_DISCONNECTION_COMPLETE event. */
        pending_disc_handle = 0xFFFFU;
        disc_retry_count    = 0U;
        DEBUG_INFO("Disconnect retry accepted by HCI - waiting for DISCONNECTION_COMPLETE event");
        return 1;
    }

    if (ret == 0x01U && disc_retry_count < MAX_DISC_RETRIES) {
        /* Controller still BUSY — keep handle live and re-arm the timer.
         * DiscRetry_Callback will fire after DISC_RETRY_TICKS (~200ms) and
         * set disc_retry_pending again so this function is called once more. */
        DEBUG_INFO("hci_disconnect BUSY on retry #%u/%u - rescheduling timer_id=%u ticks=%u",
                   (unsigned)disc_retry_count,
                   (unsigned)MAX_DISC_RETRIES,
                   (unsigned)disc_retry_timer_id,
                   (unsigned)DISC_RETRY_TICKS);
        DEBUG_INFO("Before re-arm: pending_hdl=0x%04X disc_retry_pending=%u",
                   pending_disc_handle,
                   (unsigned)disc_retry_pending);
        HW_TS_Stop(disc_retry_timer_id);
        DEBUG_INFO("HW_TS_Stop called - stopping old timer");
        HW_TS_Start(disc_retry_timer_id, DISC_RETRY_TICKS);
        DEBUG_INFO("HW_TS_Start re-arm called - timer armed for ~200ms");
        return 1;
    }

    /* Retries exhausted or non-BUSY error — force-clear and give up. */
    DEBUG_INFO("Disconnect retry exhausted (err=0x%02X after %u tries) - force-clearing",
               ret, (unsigned)disc_retry_count);
    disc_force_clear(hdl);
    return 1;
}

int BLE_Connection_StartScan(uint16_t duration_ms)
{
    tBleStatus ret;
    
    DEBUG_INFO("Starting BLE scan: %dms", duration_ms);

    BLE_DeviceManager_ResetScanFlags();
    
    /* Use ACI_GAP_START_GENERAL_DISCOVERY_PROC for active scanning
     * Scan interval: 0x0010 = 10ms
     * Scan window: 0x0010 = 10ms  
     * Own address: Public (0x00)
     * Filter duplicates: No (0x00) - report all devices
     */
    ret = aci_gap_start_general_discovery_proc(
        0x0010,     /* LE_Scan_Interval: 10ms */
        0x0010,     /* LE_Scan_Window: 10ms */
        0x00,       /* Own_Address_Type: Public */
        0x00        /* Filter_Duplicates: Disabled (report all) */
    );
    
    if (ret != BLE_STATUS_SUCCESS) {
        DEBUG_ERROR("Failed to start general discovery: 0x%02X", ret);
        return -1;
    }
    
    BLE_DeviceManager_SetScanActive(1);
    DEBUG_INFO("Scan started successfully");
    return 0;
}

int BLE_Connection_StopScan(void)
{
    tBleStatus ret;
    
    DEBUG_INFO("Stopping BLE scan");
    
    /* Terminate general discovery procedure (0x02 = GAP_GENERAL_DISCOVERY_PROC) */
    ret = aci_gap_terminate_gap_proc(0x02);
    
    if (ret != BLE_STATUS_SUCCESS) {
        DEBUG_ERROR("Failed to terminate scan: 0x%02X", ret);
        return -1;
    }
    
    BLE_DeviceManager_SetScanActive(0);
    DEBUG_INFO("Scan stopped successfully");
    return 0;
}

int BLE_Connection_CreateConnection(const uint8_t *mac)
{
    tBleStatus ret;
    BLE_Device_t *dev;
    int dev_idx;
    
    if (mac == NULL) {
        return -1;
    }
    
    DEBUG_INFO("Creating connection to device");
    DEBUG_PrintMAC(mac);
    
    /* Find device to get addr_type */
    dev_idx = BLE_DeviceManager_FindDevice(mac);
    if (dev_idx < 0) {
        DEBUG_ERROR("Device not found in list");
        return -1;
    }
    
    dev = BLE_DeviceManager_GetDevice(dev_idx);
    if (dev == NULL) {
        return -1;
    }
    
    /* Stop scan first */
    BLE_Connection_StopScan();
    
    /* Create connection using ACI_GAP_CREATE_CONNECTION
     * Peer address type: 0x00 = Public (most common)
     */
    ret = aci_gap_create_connection(
        0x0010,         /* LE_Scan_Interval: 10ms (0x0010 * 0.625ms) */
        0x0010,         /* LE_Scan_Window: 10ms */
        dev->addr_type, /* Peer_Address_Type: Public */
        mac,            /* Peer_Address */
        0x00,           /* Own_Address_Type: Public */
        0x0018,         /* Conn_Interval_Min: 30ms (24 * 1.25ms) */
        0x0028,         /* Conn_Interval_Max: 50ms (40 * 1.25ms) */
        0x0000,         /* Conn_Latency: 0 */
        0x00C8,         /* Supervision_Timeout: 2000ms (200 * 10ms) */
        0x0000,         /* Minimum_CE_Length: 0 */
        0x0000          /* Maximum_CE_Length: 0 */
    );
    
    if (ret != BLE_STATUS_SUCCESS) {
        DEBUG_ERROR("Failed to create connection: 0x%02X", ret);
        return -1;
    }

    /* Start connection-attempt timeout (10 s) */
    conn_in_progress = 1;
    HW_TS_Start(conn_timeout_timer_id, conn_timeout_ticks);

    AT_Response_Send("+CONNECTING\r\n");
    DEBUG_INFO("Connection initiated");
    return 0;
}

int BLE_Connection_TerminateConnection(uint16_t conn_handle)
{
    tBleStatus ret;
    uint8_t i;

    DEBUG_INFO("Terminating connection: 0x%04X", conn_handle);

    ret = hci_disconnect(conn_handle, 0x13);

    if (ret == BLE_STATUS_SUCCESS) {
        DEBUG_INFO("Disconnect initiated");
        return 0;
    }

    if (ret == 0x01U) {
        /* CONTROLLER_BUSY — happens when AT+DISCONNECT is issued immediately after
         * AT+DISC / AT+CHARS. The BLE CPU2 is still finalising GATT teardown even
         * though ACI_GATT_PROC_COMPLETE was already delivered.
         *
         * Instead: start a HW_TS timer. The timer fires in sequencer context. */
        DEBUG_INFO("hci_disconnect BUSY: arming retry timer");
        pending_disc_handle = conn_handle;
        disc_retry_count    = 0U;
        
        DEBUG_INFO("[PIN1] About to call HW_TS_RTC_ReadLeftTicksToCount");
        disc_retry_arm_tick = HW_TS_RTC_ReadLeftTicksToCount();  
        DEBUG_INFO("[PIN2] HW_TS_RTC_ReadLeftTicksToCount returned");
        
        DEBUG_INFO("[PIN3] About to call HW_TS_Stop");
        HW_TS_Stop(disc_retry_timer_id);
        DEBUG_INFO("[PIN4] HW_TS_Stop returned");
        
        DEBUG_INFO("[PIN5] About to call HW_TS_Start");
        HW_TS_Start(disc_retry_timer_id, DISC_RETRY_TICKS);
        DEBUG_INFO("[PIN6] HW_TS_Start returned");
        DEBUG_INFO("[TIMER ARMED] Retry will fire in ~200ms");
        return 0;
    }

    /* Any other error — force-clear immediately */
    DEBUG_INFO("hci_disconnect 0x%02X - force-clearing state", ret);

    {
        int fc_idx = BLE_DeviceManager_FindConnHandle(conn_handle);
        if (fc_idx >= 0) {
            BLE_DeviceManager_UpdateConnection(fc_idx, conn_handle, 0);
        }
    }
    for (i = 0; i < MAX_BLE_CONNECTIONS; i++) {
        if (connections[i].conn_handle == conn_handle) {
            connections[i].conn_handle = 0xFFFF;
            connections[i].state       = CONN_STATE_IDLE;
            if (connection_count > 0) { connection_count--; }
            break;
        }
    }
    AT_Response_Send("+DISCONNECTED:0x%04X\r\n", conn_handle);
    return -1;
}

void BLE_Connection_SetState(uint16_t conn_handle, BLE_ConnectionState_t state)
{
    uint8_t i;
    
    for (i = 0; i < MAX_BLE_CONNECTIONS; i++) {
        if (connections[i].conn_handle == conn_handle) {
            connections[i].state = state;
            DEBUG_INFO("Conn 0x%04X state: %d", conn_handle, (int)state);
            return;
        }
    }
}

BLE_ConnectionState_t BLE_Connection_GetState(uint16_t conn_handle)
{
    uint8_t i;
    
    for (i = 0; i < MAX_BLE_CONNECTIONS; i++) {
        if (connections[i].conn_handle == conn_handle) {
            return connections[i].state;
        }
    }
    return CONN_STATE_IDLE;
}

uint8_t BLE_Connection_IsConnected(uint16_t conn_handle)
{
    uint8_t i;
    
    for (i = 0; i < MAX_BLE_CONNECTIONS; i++) {
        if (connections[i].conn_handle == conn_handle) {
            return (connections[i].state == CONN_STATE_CONNECTED) ? 1U : 0U;
        }
    }
    return 0;
}

/* ADV_IND=0x00 and ADV_DIRECT_IND=0x01 are connectable; all others are not */
#define BLE_EVT_TYPE_CONNECTABLE(t)  ((t) == 0x00U || (t) == 0x01U)

void BLE_Connection_OnScanReport(const uint8_t *mac, int8_t rssi,
                                  const char *name, uint8_t addr_type,
                                  uint8_t event_type)
{
    int idx;
    BLE_Device_t *dev;
    
    if (mac == NULL) {
        return;
    }
    
    idx = BLE_DeviceManager_AddDevice(mac, rssi);
    
    if (idx >= 0) {
        dev = BLE_DeviceManager_GetDevice(idx);
        if (dev == NULL) {
            return;
        }
        BLE_DeviceManager_UpdateAddrType(idx, addr_type);
        /* Store event type and track whether this device is connectable */
        dev->event_type = event_type;
        
        /* If we receive a connectable packet (ADV_IND/ADV_DIRECT_IND), mark it connectable.
         * NEVER reset to 0 on SCAN_RSP (0x04) because a device can send both! */
        if (BLE_EVT_TYPE_CONNECTABLE(event_type)) {
            dev->is_connectable = 1U;
        }
        
        if (name != NULL && name[0] != '\0') {
            BLE_DeviceManager_UpdateName(idx, name);
        }
        
        /* Send AT response for newly discovered device */
        if (!dev->reported_in_scan) {
            dev->reported_in_scan = 1;
            AT_Response_Send("+SCAN:%02X:%02X:%02X:%02X:%02X:%02X,%d,%s\r\n",
                mac[5], mac[4], mac[3], mac[2], mac[1], mac[0],
                (int)rssi,
                (name != NULL && name[0] != '\0') ? name : "Unknown");
        }
    }
}


void BLE_Connection_OnConnected(const uint8_t *mac, uint16_t conn_handle, uint8_t status)
{
    int dev_idx;
    uint8_t i;

    /* Stop timeout timer regardless of outcome */
    conn_in_progress = 0;
    HW_TS_Stop(conn_timeout_timer_id);

    DEBUG_INFO("Conn complete: hdl=0x%04X status=0x%02X", conn_handle, status);
    
    if (status != 0) {
        DEBUG_ERROR("Conn failed: 0x%02X", status);
        AT_Response_Send("+CONN_ERROR:%02X\r\n", status);
        return;
    }
    
    dev_idx = BLE_DeviceManager_FindDevice(mac);
    if (dev_idx >= 0) {
        BLE_DeviceManager_UpdateConnection(dev_idx, conn_handle, 1);
        
        /* Store in connections array */
        for (i = 0; i < MAX_BLE_CONNECTIONS; i++) {
            if (connections[i].conn_handle == 0xFFFF) {
                connections[i].conn_handle = conn_handle;
                connections[i].state = CONN_STATE_CONNECTED;
                memcpy(connections[i].mac_addr, mac, BLE_MAC_LEN);
                connection_count++;
                break;
            }
        }
        
        AT_Response_Send("+CONNECTED:%d,0x%04X\r\n", dev_idx, conn_handle);
    }
}

void BLE_Connection_OnDisconnected(uint16_t conn_handle, uint8_t reason)
{
    int dev_idx;
    uint8_t i;
    uint8_t was_connected = 0;

    DEBUG_INFO("Disconn: hdl=0x%04X reason=0x%02X", conn_handle, reason);

    dev_idx = BLE_DeviceManager_FindConnHandle(conn_handle);
    if (dev_idx >= 0) {
        BLE_Device_t *dev = BLE_DeviceManager_GetDevice(dev_idx);

        if (dev != NULL && dev->is_connected) {
            was_connected = 1;
        } else {
            DEBUG_INFO("Disconn event for already-cleared hdl 0x%04X - ignored", conn_handle);
        }

        BLE_DeviceManager_UpdateConnection(dev_idx, conn_handle, 0);
    }

    /* Remove from connections table regardless */
    for (i = 0; i < MAX_BLE_CONNECTIONS; i++) {
        if (connections[i].conn_handle == conn_handle) {
            connections[i].conn_handle = 0xFFFF;
            connections[i].state = CONN_STATE_IDLE;
            if (connection_count > 0) {
                connection_count--;
            }
            break;
        }
    }

    /* Notify AT layer only if device was connected when event arrived */
    if (was_connected) {
        AT_Response_Send("+DISCONNECTED:0x%04X\r\n", conn_handle);
    }
}
