// Copyright 2017-2018 Leland Lucius
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#if !defined(_FATFLASH_H_)
#define _FATFLASH_H_ 1

#include "esp_err.h"
#include "esp_vfs.h"

#include "esp_vfs_fat.h"
#include "ff.h"
#include "diskio.h"

#include "WL_Flash.h"

#include "extflash.h"

typedef struct
{
    ExtFlash *flash;            // initialized instance of ExtFlash
    const char *base_path;      // mount point
    int open_files;             // number of open files to support
    bool auto_format;           // true=format if not valid
} fat_flash_config_t;

class FatFlash : public Flash_Access
{
public:
    FatFlash();
    virtual ~FatFlash();

    esp_err_t init(const fat_flash_config_t *config);
    void term();

private:
    //
    // ff_diskio_impl_t implementaion
    //
    static DSTATUS disk_initialize(BYTE pdrv);
    static DSTATUS disk_status(BYTE pdrv);
    static DRESULT disk_read(BYTE pdrv, BYTE* buff, DWORD sector, UINT count);
    static DRESULT disk_write(BYTE pdrv, const BYTE* buff, DWORD sector, UINT count);
    static DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void* buff);

    //
    // Flash_Access implementation
    //
    virtual size_t chip_size();
    virtual esp_err_t erase_sector(size_t sector);
    virtual esp_err_t erase_range(size_t start_address, size_t size);
    virtual esp_err_t write(size_t dest_addr, const void *src, size_t size);
    virtual esp_err_t read(size_t src_addr, void *dest, size_t size);
    virtual size_t sector_size();

private:
    fat_flash_config_t cfg;
    size_t chip_sz;
    size_t sector_sz;

    WL_Flash *flash = NULL;
    FATFS *fs = NULL;
    BYTE pdrv = FF_VOLUMES;
    bool mounted;

    static FatFlash *ff_instances[FF_VOLUMES];
};

#endif

