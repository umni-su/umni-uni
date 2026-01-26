# UMNI DIO Component
Digital Input/Output component for PCF8574 in ESP-IDF projects.

Features
- Output Control: Up to 8 digital outputs with persistent storage in NVS
- Input Monitoring: Up to 6 digital inputs with interrupt-based change detection
- Flexible Configuration: Each I/O can be individually enabled/disabled via Kconfig or sdkconfig.defaults
- Custom Pin Mapping: Flexible mapping between logical channels and physical PCF8574 pins

## API Reference

Initialization

`um_dio_init()` - Initialize the DIO module

`um_dio_deinit()` - Clean up resources

Output Functions

`um_dio_set_output(channel, state)` - Set single output

`um_dio_get_output(channel, &state)` - Read single output state

`um_dio_toggle_output(channel)` - Toggle output

`um_dio_set_all_outputs(bitmask)` - Set all outputs

`um_dio_get_all_outputs(&bitmask)` - Get all outputs state

Input Functions

`um_dio_get_input(channel, &state)` - Read single input

`um_dio_get_all_inputs(&bitmask)` - Get all inputs state

```c
// Application code:
void monitor_inputs(void)
{
    uint8_t prev_inputs = 0;
    
    while (1) {
        uint8_t current_inputs;
        um_dio_get_all_inputs(&current_inputs);
        
        if (current_inputs != prev_inputs) {
            printf("Inputs changed: 0x%02X -> 0x%02X\n", 
                   prev_inputs, current_inputs);
            prev_inputs = current_inputs;
        }
        
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}
```