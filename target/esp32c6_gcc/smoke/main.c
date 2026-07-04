#include <stdint.h>

#define USBJTAG_BASE    0x6000F000U
#define USBJTAG_EP1      (*(volatile uint32_t *)(USBJTAG_BASE + 0x00))
#define USBJTAG_EP1_CONF (*(volatile uint32_t *)(USBJTAG_BASE + 0x04))
#define USBJTAG_WR_DONE          (1U << 0)
#define USBJTAG_IN_EP_DATA_FREE  (1U << 1)

#define TIMG0_BASE      0x60008000U
#define TIMG1_BASE      0x60009000U
#define TIMG_WDTCONFIG0(base)   (*(volatile uint32_t *)((base) + 0x48))
#define TIMG_WDTWPROTECT(base)  (*(volatile uint32_t *)((base) + 0x64))
#define TIMG_WDT_WKEY           0x50D83AA1U

#define LP_WDT_BASE             0x600B1C00U
#define LP_WDT_CONFIG0          (*(volatile uint32_t *)(LP_WDT_BASE + 0x00))
#define LP_WDT_WPROTECT         (*(volatile uint32_t *)(LP_WDT_BASE + 0x18))
#define LP_WDT_WDT_WKEY         0x50D83AA1U
#define LP_WDT_SWD_CONFIG       (*(volatile uint32_t *)(LP_WDT_BASE + 0x1c))
#define LP_WDT_SWD_WPROTECT     (*(volatile uint32_t *)(LP_WDT_BASE + 0x20))
#define LP_WDT_SWD_WKEY         0x8F1D312AU
#define LP_WDT_SWD_AUTO_FEED_EN (1U << 18)

static void
disable_timg_wdt(uint32_t base)
{
    TIMG_WDTWPROTECT(base) = TIMG_WDT_WKEY;
    TIMG_WDTCONFIG0(base) = 0U;
    TIMG_WDTWPROTECT(base) = 0U;
}

static void
disable_watchdogs(void)
{
    disable_timg_wdt(TIMG0_BASE);
    disable_timg_wdt(TIMG1_BASE);

    LP_WDT_WPROTECT = LP_WDT_WDT_WKEY;
    LP_WDT_CONFIG0 = 0U;
    LP_WDT_WPROTECT = 0U;

    LP_WDT_SWD_WPROTECT = LP_WDT_SWD_WKEY;
    LP_WDT_SWD_CONFIG |= LP_WDT_SWD_AUTO_FEED_EN;
    LP_WDT_SWD_WPROTECT = 0U;
}

static void
usbjtag_putc(char c)
{
    uint32_t retry = 0;
    while ((USBJTAG_EP1_CONF & USBJTAG_IN_EP_DATA_FREE) == 0U) {
        if (++retry > 200000U) {
            return;
        }
    }
    USBJTAG_EP1 = (uint32_t) c;
    USBJTAG_EP1_CONF = USBJTAG_WR_DONE;
}

static void
usbjtag_puts(const char *s)
{
    while (*s) {
        if (*s == '\n') {
            usbjtag_putc('\r');
        }
        usbjtag_putc(*s++);
    }
}

void
c_main(void)
{
    volatile uint32_t i;

    disable_watchdogs();

    for (;;) {
        usbjtag_puts("ASP3 ESP32-C6 smoke test: boot OK, polled USB-Serial/JTAG print\n");
        for (i = 0; i < 3000000; i++) {
            /* delay */
        }
    }
}
