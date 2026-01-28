## Examples
```c
// Initialize with default settings
if (um_storage_init("/spiffs", NULL, 5, true) != ESP_OK) {
    return;
}

// Write JSON config
const char* config_json = "{\"wifi\":{\"ssid\":\"MyWiFi\",\"password\":\"secret123\"}}";
um_storage_write_json("/spiffs/config.json", config_json);

// Read JSON config
char buffer[256];
um_storage_read_json("/spiffs/config.json", buffer, sizeof(buffer));
printf("Config: %s\n", buffer);

// Check if file exists
if (um_storage_file_exists("/spiffs/config.json")) {
    printf("Config file exists\n");
}

// List all files
um_storage_list_files("/spiffs");

// Get storage info
size_t total, used;
um_storage_get_info(NULL, &total, &used);
printf("Storage: %d/%d bytes used\n", used, total);

// Cleanup
um_storage_deinit(NULL);
```