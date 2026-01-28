/**
 * @file um_storage.c
 * @brief Simple SPIFFS storage component implementation
 * @version 1.0.0
 */

#include <stdio.h>
#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include "esp_err.h"
#include "esp_log.h"
#include "esp_spiffs.h"
#include "um_storage.h"

static const char *TAG = "storage";

static char s_base_path[32] = "/spiffs"; // same as in partitions.csv

esp_err_t um_storage_init(
    const char* base_path, 
    const char* partition_label,
    int max_files, 
    bool format_if_mount_failed)
{
    ESP_LOGI(TAG, "Initializing SPIFFS");
    
    if (base_path != NULL) {
        strncpy(s_base_path, base_path, sizeof(s_base_path) - 1);
        s_base_path[sizeof(s_base_path) - 1] = '\0';
    }
    
    esp_vfs_spiffs_conf_t conf = {
        .base_path = s_base_path,
        .partition_label = partition_label,
        .max_files = max_files,
        .format_if_mount_failed = format_if_mount_failed
    };
    
    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    
    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount or format filesystem");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE(TAG, "Failed to find SPIFFS partition at %s, label %s", s_base_path,partition_label);
        } else {
            ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
        }
        return ret;
    }
    
    // Get storage info
    size_t total = 0, used = 0;
    ret = esp_spiffs_info(conf.partition_label, &total, &used);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get SPIFFS partition information");
    } else {
        ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
    }
    
    return ESP_OK;
}

esp_err_t um_storage_deinit(const char* partition_label)
{
    ESP_LOGI(TAG, "Unmounting SPIFFS");
    esp_err_t ret = esp_vfs_spiffs_unregister(partition_label);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to unmount SPIFFS (%s)", esp_err_to_name(ret));
    }
    return ret;
}

bool um_storage_file_exists(const char* file_path)
{
    struct stat st;
    return (stat(file_path, &st) == 0);
}

esp_err_t um_storage_read_file(const char* file_path, char* buffer, size_t buffer_size)
{
    if (buffer == NULL || buffer_size == 0) {
        ESP_LOGE(TAG, "Invalid buffer");
        return ESP_FAIL;
    }
    
    FILE* f = fopen(file_path, "r");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for reading: %s", file_path);
        return ESP_FAIL;
    }
    
    size_t bytes_read = fread(buffer, 1, buffer_size - 1, f);
    buffer[bytes_read] = '\0'; // Null-terminate
    
    fclose(f);
    
    ESP_LOGD(TAG, "Read %d bytes from %s", bytes_read, file_path);
    return ESP_OK;
}

esp_err_t um_storage_write_file(const char* file_path, const char* data)
{
    if (data == NULL) {
        ESP_LOGE(TAG, "Invalid data");
        return ESP_FAIL;
    }
    
    FILE* f = fopen(file_path, "w");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for writing: %s", file_path);
        return ESP_FAIL;
    }
    
    size_t bytes_written = fprintf(f, "%s", data);
    fclose(f);
    
    ESP_LOGD(TAG, "Wrote %d bytes to %s", bytes_written, file_path);
    return ESP_OK;
}

esp_err_t um_storage_append_file(const char* file_path, const char* data)
{
    if (data == NULL) {
        ESP_LOGE(TAG, "Invalid data");
        return ESP_FAIL;
    }
    
    FILE* f = fopen(file_path, "a");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for appending: %s", file_path);
        return ESP_FAIL;
    }
    
    size_t bytes_written = fprintf(f, "%s", data);
    fclose(f);
    
    ESP_LOGD(TAG, "Appended %d bytes to %s", bytes_written, file_path);
    return ESP_OK;
}

esp_err_t um_storage_delete_file(const char* file_path)
{
    if (unlink(file_path) != 0) {
        ESP_LOGE(TAG, "Failed to delete file: %s", file_path);
        return ESP_FAIL;
    }
    
    ESP_LOGD(TAG, "Deleted file: %s", file_path);
    return ESP_OK;
}

size_t um_storage_get_file_size(const char* file_path)
{
    struct stat st;
    if (stat(file_path, &st) == 0) {
        return st.st_size;
    }
    return 0;
}

esp_err_t um_storage_get_info(const char* partition_label, size_t* total, size_t* used)
{
    if (total == NULL || used == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    esp_err_t ret = esp_spiffs_info(partition_label, total, used);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get storage info");
    }
    
    return ret;
}

esp_err_t um_storage_list_files(const char* path)
{
    DIR* dir = opendir(path);
    if (dir == NULL) {
        ESP_LOGE(TAG, "Failed to open directory: %s", path);
        return ESP_FAIL;
    }
    
    struct dirent* entry;
    ESP_LOGI(TAG, "Files in %s:", path);
    
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_REG) {
            ESP_LOGI(TAG, "  FILE: %s", entry->d_name);
        } else if (entry->d_type == DT_DIR) {
            ESP_LOGI(TAG, "  DIR: %s", entry->d_name);
        }
    }
    
    closedir(dir);
    return ESP_OK;
}

esp_err_t um_storage_read_json(const char* file_path, char* buffer, size_t buffer_size)
{
    return um_storage_read_file(file_path, buffer, buffer_size);
}

esp_err_t um_storage_write_json(const char* file_path, const char* json_data)
{
    return um_storage_write_file(file_path, json_data);
}

esp_err_t um_storage_format(const char* partition_label)
{
    ESP_LOGW(TAG, "Formatting SPIFFS partition");
    esp_err_t ret = esp_spiffs_format(partition_label);
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to format SPIFFS (%s)", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "SPIFFS formatted successfully");
    }
    
    return ret;
}