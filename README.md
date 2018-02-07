# Fat filesystem on external flash for the ESP32
FatFlash utilizes my [ExtFlash](components/extflash)
component to provide FATFS support on external flash chips.

It uses the FATFS and wear leveling components from the esp-idf SDK rather
than distributing local copies.

To use, just add the "extflash" and "fatflash" components to your
components directory and initialize it with something like:

```
#include "extflash.h"

void app_main()
{
    ExtFlash flash;
    esp_err_t err;

    ext_flash_config_t cfg =
    {
        .vspi = true,
        .sck_io_num = PIN_SPI_SCK,
        .miso_io_num = PIN_SPI_MISO,
        .mosi_io_num = PIN_SPI_MOSI,
        .ss_io_num = PIN_SPI_SS,
        .hd_io_num = PIN_SPI_HD,
        .wp_io_num = PIN_SPI_WP,
        .speed_mhz = 40,
        .dma_channel = 1,
        .queue_size = 2,
        .sector_size = 0,
        .capacity = 0
    };

    err = flash.init(&cfg);
    if (err != ESP_OK)
    {
        ...
    }

    fat_flash_config_t fat_cfg =
    {
        .flash = &extflash,
        .base_path = MOUNT_POINT,
        .open_files = 4,
        .auto_format = true
    };

    err = fatflash.init(&fat_cfg);
    if (err != ESP_OK)
    {
        ...
    }
    ...
}
```

The configuration options for FatFlash are:

```
typedef struct
{
    ExtFlash *flash;            // initialized instance of ExtFlash
    const char *base_path;      // mount point
    int open_files;             // number of open files to support
    bool auto_format;           // true=format if not valid
} fat_flash_config_t;
```

More documentation to follow.

