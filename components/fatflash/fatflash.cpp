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

#include "esp_log.h"

#include "fatflash.h"

static const char *TAG = "fatflash";

FatFlash::FatFlash()
{
    flash = NULL;
    fs = NULL;
    pdrv = FF_VOLUMES;
    mounted = false;
}

FatFlash::~FatFlash()
{
    term();
}

esp_err_t FatFlash::init(const fat_flash_config_t *config)
{
    esp_err_t err = ESP_OK;
    FRESULT result = FR_OK;

    cfg = *config;

    sector_sz = cfg.flash->sector_size();
    chip_sz = cfg.flash->chip_size();

    wl_config_t wl_cfg =
    {
        .start_addr = 0,
        .full_mem_size = chip_sz,
        .page_size = sector_sz,
        .sector_size = sector_sz,
        .updaterate = 16,
        .wr_size = 16,
        .version = 0,
        .temp_buff_size = 32,
        .crc = 0
    };

    flash = new WL_Flash();
    if (flash == NULL)
    {
        return ESP_ERR_NO_MEM;
    }

    err = flash->config(&wl_cfg, this);
    if (err != ESP_OK)
    {
        return err;
    }

    err = flash->init();
    if (err != ESP_OK)
    {
        return err;
    }

    err = ff_diskio_get_drive(&pdrv);
    if (err != ESP_OK)
    {
        return err;
    }

    static const ff_diskio_impl_t diskio_impl =
    {
        .init = &disk_initialize,
        .status = &disk_status,
        .read = &disk_read,
        .write = &disk_write,
        .ioctl = &disk_ioctl
    };

    ff_diskio_register(pdrv, &diskio_impl);

    ff_instances[pdrv] = this;

    char drv[3] =
    {
        (char)('0' + pdrv),
        ':',
        0
    };

    err = esp_vfs_fat_register(cfg.base_path, drv, cfg.open_files, &fs);
    if (err != ESP_OK)
    {
        return err;
    }

    result = f_mount(fs, drv, 1);
    if (result != FR_OK)
    {
        if (result != FR_NO_FILESYSTEM || !cfg.auto_format)
        {
            ESP_LOGE(TAG, "f_mount failed (%d)", result);

            return ESP_FAIL;
        }

        ESP_LOGI(TAG, "Formatting FATFS partition");
        size_t wbsize = sector_sz;
        void *workbuf = malloc(wbsize);
        if (workbuf)
        {
            result = f_mkfs(drv, FM_ANY | FM_SFD, 0, workbuf, wbsize);
            free(workbuf);
        }
        else
        {
            result = FR_NOT_ENOUGH_CORE;
        }

        if (result != FR_OK)
        {
            ESP_LOGE(TAG, "f_mkfs failed (%d)", result);
            return ESP_FAIL;
        }

        result = f_mount(fs, drv, 0);
        if (result != FR_OK)
        {
            ESP_LOGE(TAG, "f_mount failed after formatting (%d)", result);
            return ESP_FAIL;
        }
    }

    mounted = true;

    return ESP_OK;
}

void FatFlash::term()
{
    if (mounted)
    {
        char drv[3] =
        { 
            (char)('0' + pdrv),
            ':',
            0
        };

        f_unmount(drv);
        mounted = false;
    }

    if (fs)
    {
        esp_vfs_fat_unregister_path(cfg.base_path);
        fs = NULL;
    }

    if (pdrv != FF_VOLUMES)
    {
        ff_diskio_unregister(pdrv);
        pdrv = FF_VOLUMES;
    }

    if (flash)
    {
        delete flash;
        flash = NULL;
    }
}

// ============================================================================
// ff_diskio_impl_t implementation
// ============================================================================

FatFlash *FatFlash::ff_instances[FF_VOLUMES];

DSTATUS FatFlash::disk_initialize(BYTE pdrv)
{
    ESP_LOGD(TAG, "%s - pdrv=%d", __func__, pdrv);

    return ff_instances[pdrv] ? 0 : STA_NOINIT;
}

DSTATUS FatFlash::disk_status(BYTE pdrv)
{
    ESP_LOGD(TAG, "%s - pdrv=%d", __func__, pdrv);

    return ff_instances[pdrv] ? 0 : STA_NOINIT;
}

DRESULT FatFlash::disk_read(BYTE pdrv, BYTE *buff, DWORD sector, UINT count)
{
    ESP_LOGD(TAG, "%s - pdrv=%d, sector=%ld, count=%d", __func__, pdrv, sector, count);

    FatFlash *that = ff_instances[pdrv];
    if (that == NULL)
    {
        return RES_NOTRDY;
    }

    size_t addr = sector * that->sector_sz;
    size_t len = count * that->sector_sz;
    esp_err_t err;

    err = that->flash->read(addr, buff, len);
    if (err != ESP_OK)
    {
        return RES_ERROR;
    }

    return RES_OK;
}

DRESULT FatFlash::disk_write(BYTE pdrv, const BYTE *buff, DWORD sector, UINT count)
{
    ESP_LOGD(TAG, "%s - pdrv=%d, sector=%ld, count=%d", __func__, pdrv, sector, count);

    FatFlash *that = ff_instances[pdrv];
    if (that == NULL)
    {
        return RES_NOTRDY;
    }

    size_t addr = sector * that->sector_sz;
    size_t len = count * that->sector_sz;
    esp_err_t err;

    err = that->flash->erase_range(addr, len);
    if (err != ESP_OK)
    {
        return RES_ERROR;
    }

    err = that->flash->write(addr, buff, len);
    if (err != ESP_OK)
    {
        return RES_ERROR;
    }

    return RES_OK;
}

DRESULT FatFlash::disk_ioctl(BYTE pdrv, BYTE cmd, void *buff)
{
    ESP_LOGD(TAG, "%s: cmd=%d", __func__, cmd);

    FatFlash *that = ff_instances[pdrv];
    if (that == NULL)
    {
        return RES_NOTRDY;
    }

    switch (cmd)
    {
        case CTRL_SYNC:
            return RES_OK;

        case GET_SECTOR_COUNT:
            *((DWORD *) buff) = that->chip_sz / that->sector_sz;
            return RES_OK;

        case GET_SECTOR_SIZE:
            *((WORD *) buff) = that->sector_sz;
            return RES_OK;

        case GET_BLOCK_SIZE:
            return RES_ERROR;
    }

    return RES_ERROR;
}

// ============================================================================
// Flash_Access implementation
// ============================================================================

size_t FatFlash::chip_size()
{
    return chip_sz;
}

esp_err_t FatFlash::erase_sector(size_t sector)
{
    return cfg.flash->erase_sector(sector);
}

esp_err_t FatFlash::erase_range(size_t start_address, size_t size)
{
    return cfg.flash->erase_range(start_address, size);
}

esp_err_t FatFlash::write(size_t dest_addr, const void *src, size_t size)
{
    return cfg.flash->write(dest_addr, src, size);
}

esp_err_t FatFlash::read(size_t src_addr, void *dest, size_t size)
{
    return cfg.flash->read(src_addr, dest, size);
}

size_t FatFlash::sector_size()
{
    return sector_sz;
}



