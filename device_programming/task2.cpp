#include "mbed.h"
#include "stdint.h" //This allow the use of integers of a known width
#include <array>

#define LM75_REG_TEMP (0x00) // Temperature Register
#define LM75_REG_CONF (0x01) // Configuration Register
#define LM75_ADDR     (0x90) // LM75 address

#define LM75_REG_TOS (0x03) // TOS Register
#define LM75_REG_THYST (0x02) // THYST Register

const float t_os = 28.0; // TOS temperature
const float t_hyst = 26.0; // THYST tempertuare
const int num_vals_to_store = 60; // past minute

int i = 0;  // number of measurements taken so far
std::array<float, num_vals_to_store> measurements;  // most recent 60 data points

DigitalOut greenled(LED1);  // good: temp < 28 (when rising) and temp < 26 (when falling)
DigitalOut blueled(LED2);  // error: unable to use I2C
DigitalOut redled(LED3);  // warning: temp > 28 (when rising) and temp > 26 (when falling)

I2C i2c(I2C_SDA, I2C_SCL);
InterruptIn lm75_int(D7); // Make sure you have the OS line connected to D7
Serial pc(SERIAL_TX, SERIAL_RX);
int16_t i16; // This variable needs to be 16 bits wide for the TOS and THYST conversion to work

void set_green() {
    redled = false; greenled = true;
}

void set_red() {
    redled = true; greenled = false;
}

void swap_red_green_leds() {
    redled = !redled;
    greenled = !greenled;
}

std::array<float, num_vals_to_store> left_shift_push(std::array<float, num_vals_to_store> arr,
        int len, float new_val) {

    for (int i = 0; i < len - 1; i++)
        arr[i] = arr[i + 1];

    arr[len - 1] = new_val;
    return arr;
}

int main()
{
        char data_write[3];
        char data_read[3];

        /* Configure the Temperature sensor device STLM75:
           - Thermostat mode Interrupt
           - Fault tolerance: 0
           - Interrupt mode means that the line will trigger when you exceed TOS and stay triggered until a register is read - see data sheet
        */
        data_write[0] = LM75_REG_CONF;
        data_write[1] = 0b00;  // 00 = comparator mode; 10 = interrupt mode;
        int status = i2c.write(LM75_ADDR, data_write, 2, 0);
        if (status != 0) {while (1) {blueled = !blueled; wait(0.2);}}  // error - flash LED forever

        // This section of code sets the TOS register
        data_write[0] = LM75_REG_TOS;
        i16 = (int16_t)(t_os * 256) & 0xFF80;
        data_write[1] = (i16 >> 8) & 0xff;
        data_write[2] = i16 & 0xff;
        i2c.write(LM75_ADDR, data_write, 3, 0);

        //This section of codes set the THYST register
        data_write[0] = LM75_REG_THYST;
        i16 = (int16_t)(t_hyst * 256) & 0xFF80;
        data_write[1] = (i16 >> 8) & 0xff;
        data_write[2] = i16 & 0xff;
        i2c.write(LM75_ADDR, data_write, 3, 0);

        // This line attaches the interrupt.
        // The interrupt line is active low so we trigger on a falling edge

        // get initial temp
        data_write[0] = LM75_REG_TEMP;
        i2c.write(LM75_ADDR, data_write, 1, 1); // no stop
        i2c.read(LM75_ADDR, data_read, 2, 0);
        int16_t i16 = (data_read[0] << 8) | data_read[1];
        float temp = i16 / 256.0;

        // set initial colours correctly
        if (temp < t_os) {redled = false; greenled = true;} else {redled = true; greenled = false;}
        blueled = false;

        // set function callbacks
        //lm75_int.fall(&swap_red_green_leds);  // if using interrupt mode, try this pair
        //lm75_int.rise(NULL);

        lm75_int.fall(&set_red);  // if using comparator mode, try this pair
        lm75_int.rise(&set_green);

        // enter main loop
        while (1)
        {
                // Read temperature register
                data_write[0] = LM75_REG_TEMP;
                i2c.write(LM75_ADDR, data_write, 1, 1); // no stop
                i2c.read(LM75_ADDR, data_read, 2, 0);

                // Calculate temperature value in Celsius
                int16_t i16 = (data_read[0] << 8) | data_read[1];
                // Read data as twos complement integer so sign is correct
                float temp = i16 / 256.0;

                // Send temp over serial
                pc.printf("Measurement #%d - Temperature = %.3f\r\n", i, temp);

                // push into array
                if (i < num_vals_to_store) {
                    measurements[i] = temp;
                    pc.printf("Last 3 values: %.3f %.3f %.3f\r\n\n",
                        measurements[i],
                        measurements[max(i-1, 0)],
                        measurements[max(i-2, 0)]);
                } else {
                    measurements = left_shift_push(measurements, num_vals_to_store, temp);
                    pc.printf("Last 3 values: %.3f %.3f %.3f\r\n\n",
                        measurements[max(num_vals_to_store - 1, 0)],
                        measurements[max(num_vals_to_store - 2, 0)],
                        measurements[max(num_vals_to_store - 3, 0)]);
                }

                if (i < 2147483647) {
                    i++;  // incremement counter
                } else {
                    i = num_vals_to_store;  // reset to prevent buffer overflow
                }

                wait(1.0);
        }
}