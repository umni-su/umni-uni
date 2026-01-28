/**
 * @file um_storage.h
 * @brief Simple SPIFFS storage component for ESP-IDF
 * @version 1.0.0
 */

#ifndef UM_STORAGE_H
#define UM_STORAGE_H

#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize SPIFFS storage
 * 
 * @param base_path Mount point (e.g., "/spiffs")
 * @param partition_label Partition label (NULL for default)
 * @param max_files Maximum number of open files
 * @param format_if_mount_failed Format if mount fails
 * @return esp_err_t ESP_OK on success
 */
esp_err_t um_storage_init(const char* base_path, const char* partition_label,
                          int max_files, bool format_if_mount_failed);

/**
 * @brief Deinitialize SPIFFS storage
 * 
 * @param partition_label Partition label (NULL for default)
 * @return esp_err_t ESP_OK on success
 */
esp_err_t um_storage_deinit(const char* partition_label);

/**
 * @brief Check if file exists
 * 
 * @param file_path Full file path
 * @return true if file exists
 * @return false if file doesn't exist
 */
bool um_storage_file_exists(const char* file_path);

/**
 * @brief Read file content
 * 
 * @param file_path Full file path
 * @param buffer Buffer to store data
 * @param buffer_size Buffer size
 * @return esp_err_t ESP_OK on success, ESP_FAIL on error
 */
esp_err_t um_storage_read_file(const char* file_path, char* buffer, size_t buffer_size);

/**
 * @brief Write data to file
 * 
 * @param file_path Full file path
 * @param data Data to write
 * @return esp_err_t ESP_OK on success, ESP_FAIL on error
 */
esp_err_t um_storage_write_file(const char* file_path, const char* data);

/**
 * @brief Append data to file
 * 
 * @param file_path Full file path
 * @param data Data to append
 * @return esp_err_t ESP_OK on success, ESP_FAIL on error
 */
esp_err_t um_storage_append_file(const char* file_path, const char* data);

/**
 * @brief Delete file
 * 
 * @param file_path Full file path
 * @return esp_err_t ESP_OK on success, ESP_FAIL on error
 */
esp_err_t um_storage_delete_file(const char* file_path);

/**
 * @brief Get file size
 * 
 * @param file_path Full file path
 * @return size_t File size in bytes, 0 on error
 */
size_t um_storage_get_file_size(const char* file_path);

/**
 * @brief Get storage information
 * 
 * @param partition_label Partition label (NULL for default)
 * @param total Total bytes (output)
 * @param used Used bytes (output)
 * @return esp_err_t ESP_OK on success
 */
esp_err_t um_storage_get_info(const char* partition_label, size_t* total, size_t* used);

/**
 * @brief List all files in directory
 * 
 * @param path Directory path
 * @return esp_err_t ESP_OK on success
 */
esp_err_t um_storage_list_files(const char* path);

/**
 * @brief Read JSON file (for your configs)
 * 
 * @param file_path Full file path
 * @param buffer Buffer to store JSON
 * @param buffer_size Buffer size
 * @return esp_err_t ESP_OK on success
 */
esp_err_t um_storage_read_json(const char* file_path, char* buffer, size_t buffer_size);

/**
 * @brief Write JSON file (for your configs)
 * 
 * @param file_path Full file path
 * @param json_data JSON data to write
 * @return esp_err_t ESP_OK on success
 */
esp_err_t um_storage_write_json(const char* file_path, const char* json_data);

/**
 * @brief Format storage
 * 
 * @param partition_label Partition label (NULL for default)
 * @return esp_err_t ESP_OK on success
 */
esp_err_t um_storage_format(const char* partition_label);

#ifdef __cplusplus
}
#endif

#endif // UM_STORAGE_H