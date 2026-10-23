#include "pmu_axp2101.h"

#include "driver/i2c_master.h"
#include "esp_check.h"
#include "esp_log.h"
#include "bsp/esp-bsp.h"
#include "sdkconfig.h"

#define AXP2101_ADDR            0x34
#define AXP2101_REG_CHIP_ID     0x03
#define AXP2101_CHIP_ID         0x4A
#define AXP2101_REG_COMMON_CFG  0x10   /* bit0: software power-off */
#define AXP2101_REG_PWROFF_EN   0x22   /* bit1: PWRON long-press power-off enable */
#define AXP2101_REG_IRQ_EN1     0x41   /* bit0: PWRON positive edge, bit1: negative edge */
#define AXP2101_REG_IRQ_STS1    0x49   /* write-1-to-clear */
#define AXP2101_PKEY_POS        (1u << 0)
#define AXP2101_PKEY_NEG        (1u << 1)
#define AXP2101_PKEY_LONG       (1u << 2)
#define AXP2101_PKEY_SHORT      (1u << 3)
#define AXP2101_PKEY_ALL        (AXP2101_PKEY_POS | AXP2101_PKEY_NEG | AXP2101_PKEY_LONG | AXP2101_PKEY_SHORT)

static const char *TAG = "pmu_axp2101";
static i2c_master_dev_handle_t s_dev;
static bool s_ready;

static esp_err_t reg_read(uint8_t reg, uint8_t *value)
{
    return i2c_master_transmit_receive(s_dev, &reg, 1, value, 1, 50);
}

static esp_err_t reg_write(uint8_t reg, uint8_t value)
{
    const uint8_t buf[2] = { reg, value };
    return i2c_master_transmit(s_dev, buf, sizeof(buf), 50);
}

static esp_err_t reg_update(uint8_t reg, uint8_t clear_mask, uint8_t set_mask)
{
    uint8_t value = 0;
    ESP_RETURN_ON_ERROR(reg_read(reg, &value), TAG, "read 0x%02x", reg);
    value = (uint8_t)((value & ~clear_mask) | set_mask);
    return reg_write(reg, value);
}

esp_err_t pmu_init(void)
{
    if (s_ready) return ESP_OK;
    i2c_master_bus_handle_t bus = bsp_i2c_get_handle();
    ESP_RETURN_ON_FALSE(bus != NULL, ESP_FAIL, TAG, "BSP I2C bus unavailable");

    const i2c_device_config_t cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = AXP2101_ADDR,
        .scl_speed_hz = 400000,
    };
    ESP_RETURN_ON_ERROR(i2c_master_bus_add_device(bus, &cfg, &s_dev), TAG, "add AXP2101");

    uint8_t id = 0;
    ESP_RETURN_ON_ERROR(reg_read(AXP2101_REG_CHIP_ID, &id), TAG, "AXP2101 not responding");
    if (id != AXP2101_CHIP_ID) {
        ESP_LOGW(TAG, "Unexpected PMU chip id 0x%02x (expected 0x%02x)", id, AXP2101_CHIP_ID);
    }

#if CONFIG_ROUND_REMOTE_PWR_DISABLE_HW_LONG_OFF
    /* PWR is held for push-to-talk, so the PMU must not cut power on a long hold.
     * Power-off is done in firmware via a long press on BOOT instead. */
    ESP_RETURN_ON_ERROR(reg_update(AXP2101_REG_PWROFF_EN, 1u << 1, 0), TAG, "disable long-press off");
#endif
    ESP_RETURN_ON_ERROR(reg_update(AXP2101_REG_IRQ_EN1, 0, AXP2101_PKEY_ALL), TAG, "enable PWR edge irq");
    ESP_RETURN_ON_ERROR(reg_write(AXP2101_REG_IRQ_STS1, AXP2101_PKEY_ALL), TAG, "clear PWR edges");

    s_ready = true;
    ESP_LOGI(TAG, "AXP2101 ready (id 0x%02x)", id);
    return ESP_OK;
}

bool pmu_is_ready(void)
{
    return s_ready;
}

esp_err_t pmu_poll_pwr_edges(bool *pressed, bool *released)
{
    *pressed = false;
    *released = false;
    if (!s_ready) return ESP_ERR_INVALID_STATE;

    uint8_t sts = 0;
    ESP_RETURN_ON_ERROR(reg_read(AXP2101_REG_IRQ_STS1, &sts), TAG, "read irq status");
    sts &= AXP2101_PKEY_ALL;
    if (sts == 0) return ESP_OK;
    ESP_RETURN_ON_ERROR(reg_write(AXP2101_REG_IRQ_STS1, sts), TAG, "clear irq status");
    ESP_LOGD(TAG, "PWR irq status 0x%02x", sts);

    /* Some AXP2101 setups latch only the short-press IRQ, not the edges:
     * treat it as a complete tap so play/pause still works. */
    if ((sts & (AXP2101_PKEY_POS | AXP2101_PKEY_NEG)) == 0) {
        if (sts & AXP2101_PKEY_SHORT) {
            *pressed = true;
            *released = true;
        }
        return ESP_OK;
    }

    /* PWRON is active-low: the falling (negative) edge is the press. */
    bool neg = (sts & AXP2101_PKEY_NEG) != 0;
    bool pos = (sts & AXP2101_PKEY_POS) != 0;
#if CONFIG_ROUND_REMOTE_PWR_EDGE_INVERT
    *pressed = pos;
    *released = neg;
#else
    *pressed = neg;
    *released = pos;
#endif
    return ESP_OK;
}

esp_err_t pmu_power_off(void)
{
    ESP_RETURN_ON_FALSE(s_ready, ESP_ERR_INVALID_STATE, TAG, "PMU not ready");
    ESP_LOGW(TAG, "Software power-off");
    return reg_update(AXP2101_REG_COMMON_CFG, 0, 1u << 0);
}
