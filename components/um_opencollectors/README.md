```
char* json = um_oc_config_get_json();
// Использовать json...
free(json); // Обязательно освободить!
{
    "channels": [
        {
            "channel": 0,
            "label": "Relay 1",
            "active": true,
            "state": true,
        }
    ],
    "count": 1
}

// Обновить канал 0 (OC1): новое имя и активность
esp_err_t err = um_oc_config_update_channel(0, "Cooling Fan", true);

// Не менять имя, только активность
err = um_oc_config_update_channel(0, NULL, false);

// После изменений сохранить в файл
err = um_oc_config_save();

```