#include "_Plugin_Helper.h"
#ifdef USES_P045

// #######################################################################################################
// #################################### Plugin 045: MPU6050 ####################################
// #######################################################################################################

// Based on the works of Nolan Gilley @ https://home-assistant.io/blog/2016/08/03/laundry-automation-update/
// falling under the following license CC-BY-SA, https://creativecommons.org/licenses/by-sa/2.0/
// and the works of Jeff Rowberg @ https://www.i2cdevlib.com/devices/mpu6050, specifically his I2C Functions
// in this plugin are based on or are a copy from the following two libraries:
// I2Cdev: https://github.com/jrowberg/i2cdevlib/tree/master/Arduino/I2Cdev
// MPU6050: https://github.com/jrowberg/i2cdevlib/tree/master/Arduino/MPU6050
// Which contain the following license information:
// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and
// associated documentation files (the "Software"), to deal in the Software without restriction,
// including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
// and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so,
// subject to the following conditions: The above copyright notice and this permission notice shall be
// included in all copies or substantial portions of the Software.


// This plugin enables the use of a MPU6050 sensor as e.g. used in the breakout-board GY-521.
// Using the webform you can set thresholds for the x-y-z axis and timeout values. If the thresholds are
// exceeded the sensor is on, if the thresholds are not met during the timeout period the sensor is off.

// Using this plugin you can get a notification from your home automation system when the monitored machine or
// device is no longer vibrating and thus this can be used as a signalling device for the end of a (dish)washer
// or dryer cycle.

// You can also use the plugin to read raw sensor values. You can use more then one instance of the plugin and
// you can set multiple movement alarms by giving each instance other threshold values if needed.

// Best practise: Create three custom sensors in your home controller (like domoticz) and let it plot the x, y and
// z range. Plot the sensor values while you use the washing machine and/or dryer. Also keep monitoring when they
// are not in use so you can determine the needed thresholds. When you have these you can select the movement
// detection function to setup the plugin for further use.

// The plugin can be used simultaneously with two MPU6050 devices by adding multiple instances.
// Originally released in the PlayGround as Plugin 118.

// Plugin var usage:
// Globals    - int16_t _axis[3][5] Array to store sensorvalues of the axis
//              _axis[0-2][x]  = x, y, z axis
//              _axis[x][0-4]  = min values, max values, range (max-min), a-values, g-values.
//            - long _timer = Timer to check values each 5 seconds

// Framework  - Settings.TaskDevicePluginConfig[x][0]     - Device address (0x68 | 0x69)
//              Settings.TaskDevicePluginConfig[x][1]     - Instance function
//              Settings.TaskDevicePluginConfig[x][2]     - ax threshold value
//              Settings.TaskDevicePluginConfig[x][3]     - ay threshold value
//              Settings.TaskDevicePluginConfig[x][4]     - az threshold value
//              Settings.TaskDevicePluginConfig[x][5]     - Minimal detection threshold value
//              Settings.TaskDevicePluginConfig[x][6]     - Detection threshold window value
//              Settings.TaskDevicePluginConfig[x][7]     - Last known status of switch
//              Settings.TaskDevicePluginConfigLong[x][0] - Minimal detection threshold counter
//              Settings.TaskDevicePluginConfigLong[x][1] - Detection threshold window counter


// FIXME TD-er: Reverted to old version before adding Plugin_task_data array
// See issue: https://github.com/letscontrolit/ESPEasy/issues/2381
// Commits:
// https://github.com/letscontrolit/ESPEasy/commit/af20984079d3e7aa59e08fd9b232f6d17ba3b523#diff-ec860ac195fffa61ec11dd419fefa5b9
// https://github.com/letscontrolit/ESPEasy/commit/6400c495e24f39ebac88eb634f29cfb73137fa2b#diff-ec860ac195fffa61ec11dd419fefa5b9


# include "src/PluginStructs/P045_data_struct.h"

# define PLUGIN_045
# define PLUGIN_ID_045                       45
# define PLUGIN_NAME_045                     "Gyro - MPU 6050"
# define PLUGIN_VALUENAME1_045               ""


boolean Plugin_045(uint8_t function, struct EventStruct *event, String& string)
{
  boolean success = false;

  switch (function)
  {
    case PLUGIN_DEVICE_ADD:
    {
      Device[++deviceCount].Number       = PLUGIN_ID_045;
      Device[deviceCount].Type           = DEVICE_TYPE_I2C;
      Device[deviceCount].VType          = Sensor_VType::SENSOR_TYPE_SINGLE;
      Device[deviceCount].ValueCount     = 1;    // Unfortunatly domoticz has no custom multivalue sensors.
      Device[deviceCount].SendDataOption = true; //   and I use Domoticz ... so there.
      Device[deviceCount].TimerOption    = true;
      Device[deviceCount].FormulaOption  = false;
      Device[deviceCount].PluginStats    = true;
      break;
    }

    case PLUGIN_GET_DEVICENAME:
    {
      string = F(PLUGIN_NAME_045);
      break;
    }

    case PLUGIN_GET_DEVICEVALUENAMES:
    {
      strcpy_P(ExtraTaskSettings.TaskDeviceValueNames[0], PSTR(PLUGIN_VALUENAME1_045));
      break;
    }

    case PLUGIN_I2C_HAS_ADDRESS:
    case PLUGIN_WEBFORM_SHOW_I2C_PARAMS:
    {
      const uint8_t i2cAddressValues[] = { 0x68, 0x69 };

      if (function == PLUGIN_WEBFORM_SHOW_I2C_PARAMS) {
        addFormSelectorI2C(F("i2c_addr"), 2, i2cAddressValues, PCONFIG(0));
        addFormNote(F("ADDR Low=0x68, High=0x69"));
      } else {
        success = intArrayContains(2, i2cAddressValues, event->Par1);
      }
      break;
    }

    case PLUGIN_WEBFORM_LOAD:
    {
      uint8_t choice = PCONFIG(1);
      {
        const __FlashStringHelper *options[10] = {
          F("Movement detection"),
          F("Range acceleration X"),
          F("Range acceleration Y"),
          F("Range acceleration Z"),
          F("Acceleration X"),
          F("Acceleration Y"),
          F("Acceleration Z"),
          F("G-force X"),
          F("G-force Y"),
          F("G-force Z")
        };
        addFormSelector(F("Function"), F("pfunction"), 10, options, nullptr, choice);
      }

      if (choice == 0) {
        // If this is instance function 0, setup webform for additional vars
        // Show some user information about the webform and what the vars mean.

        addFormNumericBox(F("Detection threshold X"), F("pthld_x"), PCONFIG(2), 0, 65535);
        addFormNumericBox(F("Detection threshold Y"), F("pthld_y"), PCONFIG(3), 0, 65535);
        addFormNumericBox(F("Detection threshold Z"), F("pthld_z"), PCONFIG(4), 0, 65535);
        addFormNote(F("A Detection threshold value of 0 disables movement detection for that axis."));

        addFormCheckBox(F("Detection on all 3 axes"), F("pmultiaxes"), PCONFIG_LONG(2) == 0);
        addFormNote(F("When unchecked, movement detection will trigger on ANY axis"));

        addFormNumericBox(F("Min. detection count"), F("pthld_counter"), PCONFIG(5), 0, 999999);
        addFormNumericBox(F("Detection window"),     F("pthld_window"),  PCONFIG(6), 0, 999999);

        addFormNote(F("Details are in the 'ReadTheDocs' documentation."));
      }
      success = true;
      break;
    }

    case PLUGIN_WEBFORM_SAVE:
    {
      // Save the vars
      PCONFIG(0)      = getFormItemInt(F("i2c_addr"));
      PCONFIG(1)      = getFormItemInt(F("pfunction"));
      PCONFIG(2)      = getFormItemInt(F("pthld_x"));
      PCONFIG(3)      = getFormItemInt(F("pthld_y"));
      PCONFIG(4)      = getFormItemInt(F("pthld_z"));
      PCONFIG(5)      = getFormItemInt(F("pthld_counter"));
      PCONFIG(6)      = getFormItemInt(F("pthld_window"));
      PCONFIG_LONG(2) = isFormItemChecked(F("pmultiaxes")) ? 0 : 1; // Inverted setting, default is backward compatible, 3 axis

      if (PCONFIG(6) < PCONFIG(5)) {
        PCONFIG(6) = PCONFIG(5);
      }
      success = true;
      break;
    }

    case PLUGIN_INIT:
    {
      // Initialize the MPU6050. This *can* be done multiple times per instance and device address.
      uint8_t devAddr = PCONFIG(0);

      if ((devAddr < 0x68) || (devAddr > 0x69)) { //  Just in case the address is not initialized, set it anyway.
        devAddr    = 0x68;
        PCONFIG(0) = devAddr;
      }

      initPluginTaskData(event->TaskIndex, new (std::nothrow) P045_data_struct(devAddr));
      P045_data_struct *P045_data =
        static_cast<P045_data_struct *>(getPluginTaskData(event->TaskIndex));

      if (nullptr != P045_data) {
        success = true;
      }

      // Reset vars
      PCONFIG(7) = 0;                   // Last known value of "switch" is off
      UserVar[event->BaseVarIndex] = 0; // Switch is off
      PCONFIG_LONG(0) = 0;              // Minimal detection counter is zero
      PCONFIG_LONG(1) = 0;              // Detection window counter is zero
      break;
    }

    case PLUGIN_ONCE_A_SECOND: // FIXME TD-er: Is this fast enough? Earlier comments in the code suggest 10x per sec.
    {
      P045_data_struct *P045_data =
        static_cast<P045_data_struct *>(getPluginTaskData(event->TaskIndex));

      if (nullptr != P045_data) {
        P045_data->loop();
        success = true;
      }
      break;
    }

    case PLUGIN_READ:
    {
      // Use const pointer here, as we don't want to change data, only read it
      const P045_data_struct *P045_data =
        static_cast<const P045_data_struct *>(getPluginTaskData(event->TaskIndex));

      if (nullptr != P045_data) {
        int _P045_Function = PCONFIG(1);

        switch (_P045_Function)
        {
          // Function 0 is for movement detection
          case 0:
          {
            // Check if all (enabled, so !=0) thresholds are exceeded, if one fails then thresexceed (thesholds exceeded) is reset to false;
            bool thresexceed   = true;
            uint8_t count      = 0; // Counter to check if not all thresholdvalues are set to 0 or disabled
            uint8_t threscount = 0; // Counter to check how many tresholds have been exceeded

            for (uint8_t i = 0; i < 3; i++)
            {
              // for each axis:
              if (PCONFIG(i + 2) != 0) {    // not disabled, check threshold
                if (P045_data->_axis[i][2] >= PCONFIG(i + 2)) { threscount++; } // Logic inverted
              } else { count++; }           // If disabled count + 1
            }

            if (PCONFIG_LONG(2) == 0) {     // All (enabled) axes triggered
              thresexceed = ((threscount > 0) && (count + threscount == 3));
            } else {
              thresexceed = threscount > 0; // At least 1 axis triggered
            }

            if (count == 3) { thresexceed = false; } // If we counted to three, all three axis are disabled.

            // If all enabled thresholds are exceeded the increase the counter
            if (thresexceed) { PCONFIG_LONG(0)++; }

            // And increase the window counter
            PCONFIG_LONG(1)++;

            if (PCONFIG_LONG(1) >= PCONFIG(6)) {
              // Detection window has passed.
              PCONFIG_LONG(1) = 0; // reset window counter

              // Did we count more times exceeded then the minimum detection value?
              if (PCONFIG_LONG(0) >= PCONFIG(5)) {
                UserVar[event->BaseVarIndex] = 1; // x times threshold exceeded within window.
              } else {
                UserVar[event->BaseVarIndex] = 0; // reset because x times threshold within window not met.
              }

              // Check if UserVar changed so we do not overload homecontroller with the same readings
              if (PCONFIG(7) != UserVar[event->BaseVarIndex]) {
                PCONFIG(7) = UserVar[event->BaseVarIndex];
                success    = true;
              } else {
                success = false;
              }
              PCONFIG_LONG(0) = 0; // reset threshold exceeded counter
            }

            // The default sensorType of the device is a single sensor value. But for detection movement we want it to be
            // a switch so we change the sensortype here. Looks like a legal thing to do because _P001_Switch does it as well.
            event->sensorType = Sensor_VType::SENSOR_TYPE_SWITCH;
            break;
          }

          // All other functions are reading values. So extract xyz value and wanted type from function number:
          default:                                            // [1-3]: range-values, [4-6]: a-values, [7-9]: g-values
          {
            uint8_t reqaxis = (_P045_Function - 1) % 3;       // xyz         -> eg: function 5(ay) (5-1) % 3 = 1           (y)
            uint8_t reqvar  = ((_P045_Function - 1) / 3) + 2; // range, a, g -> eg: function 9(gz) ((9-1) / 3 = 2) + 2 = 4 (g)
            UserVar[event->BaseVarIndex] = float(P045_data->_axis[reqaxis][reqvar]);
            success                      = true;
            break;
          }
        }
      }
      break;
    }
  }
  return success;
}

#endif // USES_P045
