/*
 * Copyright (C) OpenTX
 *
 * Based on code named
 *   th9x - http://code.google.com/p/th9x
 *   er9x - http://code.google.com/p/er9x
 *   gruvin9x - http://code.google.com/p/gruvin9x
 *
 * License GPLv2: http://www.gnu.org/licenses/gpl-2.0.html
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "opentx.h"

const char *ghstRfProfileValue[GHST_RF_PROFILE_COUNT] = { "Auto", "Norm", "Race", "Pure", "Long", "Unused", "Race2", "Pure2" };
const char *ghstVtxBandName[GHST_VTX_BAND_COUNT] = { "- - -" , "IRC", "Race", "BandE", "BandB", "BandA" };

struct GhostSensor
{
  const uint16_t id;
  const char * name;
  const TelemetryUnit unit;
  const uint8_t precision;
};

// telemetry sensors ID
enum
{
  GHOST_ID_RX_RSSI = 0x0001,            // Rx-side RSSI
  GHOST_ID_RX_LQ = 0x0002,              // Rx-side link quality
  GHOST_ID_RX_SNR = 0x0003,             // Rx-side signal to noise
  GHOST_ID_FRAME_RATE = 0x0004,         // Tx-side frame rate
  GHOST_ID_TX_POWER = 0x0005,           // Tx-side power output
  GHOST_ID_RF_MODE = 0x0006,            // Tx-side frame rate
  GHOST_ID_TOTAL_LATENCY = 0x0007,      // Tx-side total latency
  GHOST_ID_VTX_FREQ = 0x0008,           // Vtx Frequency (in MHz)
  GHOST_ID_VTX_POWER = 0x0009,          // Vtx Power (in mW)
  GHOST_ID_VTX_CHAN = 0x000a,           // Vtx Channel
  GHOST_ID_VTX_BAND = 0x000b,           // Vtx Band

  GHOST_ID_PACK_VOLTS = 0x000c,         // Battery Pack Voltage
  GHOST_ID_PACK_AMPS = 0x000d,          // Battery Pack Current
  GHOST_ID_PACK_MAH = 0x000e,           // Battery Pack mAh consumed
};

const GhostSensor ghostSensors[] = {
  {GHOST_ID_RX_RSSI,         ZSTR_RSSI,             UNIT_DB,                0},
  {GHOST_ID_RX_LQ,           ZSTR_RX_QUALITY,       UNIT_PERCENT,           0},
  {GHOST_ID_RX_SNR,          ZSTR_RX_SNR,           UNIT_DB,                0},

  {GHOST_ID_FRAME_RATE,      ZSTR_FRAME_RATE,       UNIT_RAW,               0},
  {GHOST_ID_TX_POWER,        ZSTR_TX_POWER,         UNIT_MILLIWATTS,        0},
  {GHOST_ID_RF_MODE,         ZSTR_RF_MODE,          UNIT_TEXT,              0},
  {GHOST_ID_TOTAL_LATENCY,   ZSTR_TOTAL_LATENCY,    UNIT_RAW,               0},

  {GHOST_ID_VTX_FREQ,        ZSTR_VTX_FREQ,         UNIT_RAW,               0},
  {GHOST_ID_VTX_POWER,       ZSTR_VTX_PWR,          UNIT_RAW,               0},
  {GHOST_ID_VTX_CHAN,        ZSTR_VTX_CHAN,         UNIT_RAW,               0},
  {GHOST_ID_VTX_BAND,        ZSTR_VTX_BAND,         UNIT_TEXT,              0},

  {GHOST_ID_PACK_VOLTS,      ZSTR_BATT,             UNIT_VOLTS,             2},
  {GHOST_ID_PACK_AMPS,       ZSTR_CURR,             UNIT_AMPS,              2},
  {GHOST_ID_PACK_MAH,        ZSTR_CAPACITY,         UNIT_MAH,               0},

  {0x00,                     NULL,                  UNIT_RAW,               0},
};

const GhostSensor *getGhostSensor(uint8_t id)
{
  for (const GhostSensor * sensor = ghostSensors; sensor->id; sensor++) {
    if (id == sensor->id)
      return sensor;
  }
  return nullptr;
}

void processGhostTelemetryValue(uint8_t index, int32_t value)
{
  if (!TELEMETRY_STREAMING())
    return;

  const GhostSensor * sensor = getGhostSensor(index);
  setTelemetryValue(PROTOCOL_TELEMETRY_GHOST, sensor->id, 0, 0, value, sensor->unit, sensor->precision);
}

void processGhostTelemetryValueString(const GhostSensor * sensor, const char * str)
{
  if (TELEMETRY_STREAMING()) {
    setTelemetryText(PROTOCOL_TELEMETRY_GHOST, sensor->id, 0, 0, str);
  }
}

bool checkGhostTelemetryFrameCRC()
{
  uint8_t len = telemetryRxBuffer[1];
  uint8_t crc = crc8(&telemetryRxBuffer[2], len - 1);
  return (crc == telemetryRxBuffer[len + 1]);
}

uint16_t getTelemetryValue_u16(uint8_t index)
{
  return (telemetryRxBuffer[index] << 8) | telemetryRxBuffer[index + 1];
}

uint16_t getTelemetryValue_u16le(uint8_t index)
{
  return (telemetryRxBuffer[index + 1] << 8) | telemetryRxBuffer[index];
}

uint32_t getTelemetryValue_s32(uint8_t index)
{
  uint32_t val = 0;
  for (int i = 0; i < 4; ++i)
    val <<= 8, val |= telemetryRxBuffer[index + i];
  return val;
}

void processGhostTelemetryFrame()
{
  if (!checkGhostTelemetryFrameCRC()) {
    TRACE("[GS] CRC error");
    return;
  }

  uint8_t id = telemetryRxBuffer[2];
  switch(id) {
    case GHST_DL_LINK_STAT:
    {
#if defined(BLUETOOTH)
      if (g_eeGeneral.bluetoothMode == BLUETOOTH_TELEMETRY && bluetooth.state == BLUETOOTH_STATE_CONNECTED) {
        bluetooth.write(telemetryRxBuffer, telemetryRxBufferCount);
      }
#endif
      uint8_t rssiVal = min<uint8_t>(telemetryRxBuffer[3], 120); // RSSI is a negative value, but sent as a positive integer.
      uint8_t lqVal = min<uint8_t>(telemetryRxBuffer[4], 100);
      uint8_t snrVal = min<uint8_t>(telemetryRxBuffer[5], 100);

      processGhostTelemetryValue(GHOST_ID_RX_RSSI, - rssiVal);
      processGhostTelemetryValue(GHOST_ID_RX_LQ, lqVal);
      processGhostTelemetryValue(GHOST_ID_RX_SNR, snrVal);

      // give OpenTx the LQ value, not RSSI
      if (lqVal) {
        telemetryData.rssi.set(lqVal);
        telemetryStreaming = TELEMETRY_TIMEOUT10ms;
      }
      else {
        telemetryData.rssi.reset();
        telemetryStreaming = 0;
      }

      processGhostTelemetryValue(GHOST_ID_TX_POWER, getTelemetryValue_u16(6));
      processGhostTelemetryValue(GHOST_ID_FRAME_RATE, getTelemetryValue_u16(8));
      processGhostTelemetryValue(GHOST_ID_TOTAL_LATENCY, getTelemetryValue_u16(10));
      uint8_t rfModeEnum = min<uint8_t>(telemetryRxBuffer[12], GHST_RF_PROFILE_MAX);

      // RF mode string, one char at a time
      const GhostSensor * sensor = getGhostSensor(GHOST_ID_RF_MODE);
      const char * rfModeString = ghstRfProfileValue[rfModeEnum];
      processGhostTelemetryValueString(sensor, rfModeString);
      break;
    }

    case GHST_DL_VTX_STAT:
    {
#if defined(BLUETOOTH)
      if (g_eeGeneral.bluetoothMode == BLUETOOTH_TELEMETRY && bluetooth.state == BLUETOOTH_STATE_CONNECTED) {
        bluetooth.write(telemetryRxBuffer, telemetryRxBufferCount);
      }
#endif
      uint8_t vtxBandEnum = min<uint8_t>(telemetryRxBuffer[8], GHST_VTX_BAND_MAX);

      const GhostSensor * sensor = getGhostSensor(GHOST_ID_VTX_BAND);
      const char * vtxBandString = ghstVtxBandName[vtxBandEnum];

      processGhostTelemetryValue(GHOST_ID_VTX_FREQ, getTelemetryValue_u16(4));
      processGhostTelemetryValue(GHOST_ID_VTX_POWER, getTelemetryValue_u16(6));
      processGhostTelemetryValue(GHOST_ID_VTX_CHAN, min<uint8_t>(telemetryRxBuffer[9], 8));
      processGhostTelemetryValueString(sensor, vtxBandString);
      break;
    }

    case GHST_DL_MENU_DESC:
    {
      GhostMenuFrame * packet;
      GhostMenuData * lineData;
      packet = (GhostMenuFrame * ) telemetryRxBuffer;
      lineData = (GhostMenuData *) &reusableBuffer.ghostMenu.line[packet->lineIndex];
      lineData->splitLine = 0;
      reusableBuffer.ghostMenu.menuStatus = packet->menuStatus;
      lineData->lineFlags = packet->lineFlags;
      for (uint8_t i = 0; i < GHST_MENU_CHARS; i++) {
        if (packet->menuText[i] == 0x7C) {
          lineData->menuText[i] = '\0';
          lineData->splitLine = i + 1;
        }
        else {
          lineData->menuText[i] = packet->menuText[i];
        }
      }
      lineData->menuText[GHST_MENU_CHARS] = '\0';
      break;
    }

    case GHST_DL_PACK_STAT: {
#if defined(BLUETOOTH)
      if (g_eeGeneral.bluetoothMode == BLUETOOTH_TELEMETRY && bluetooth.state == BLUETOOTH_STATE_CONNECTED) {
          bluetooth.write(telemetryRxBuffer, telemetryRxBufferCount);
        }
#endif
      processGhostTelemetryValue(GHOST_ID_PACK_VOLTS, getTelemetryValue_u16le(3));
      processGhostTelemetryValue(GHOST_ID_PACK_AMPS, getTelemetryValue_u16le(5));
      processGhostTelemetryValue(GHOST_ID_PACK_MAH, getTelemetryValue_u16le(7) * 10);

      break;
    }
  }
}

void processGhostTelemetryData(uint8_t data)
{

#if defined(USB_SERIAL)
  if (getSelectedUsbMode() == USB_TELEMETRY_MODE) {
    usbSerialPutc(data);
  }
#endif

#if defined(AUX_SERIAL)
  if (g_eeGeneral.auxSerialMode == UART_MODE_TELEMETRY_MIRROR) {
    auxSerialPutc(data);
  }
#endif

#if defined(AUX2_SERIAL)
  if (g_eeGeneral.aux2SerialMode == UART_MODE_TELEMETRY_MIRROR) {
    aux2SerialPutc(data);
  }
#endif

  if (telemetryRxBufferCount == 0 && data != GHST_ADDR_RADIO) {
    TRACE("[GH] address 0x%02X error", data);
    return;
  }

  if (telemetryRxBufferCount < TELEMETRY_RX_PACKET_SIZE) {
    telemetryRxBuffer[telemetryRxBufferCount++] = data;
  }
  else {
    TRACE("[GH] array size %d error", telemetryRxBufferCount);
    telemetryRxBufferCount = 0;
  }

  if (telemetryRxBufferCount > 4) {
    uint8_t length = telemetryRxBuffer[1];
    if (length + 2 == telemetryRxBufferCount) {
      processGhostTelemetryFrame();
      telemetryRxBufferCount = 0;
    }
  }
}


void ghostSetDefault(int index, uint8_t id, uint8_t subId)
{
  TelemetrySensor &telemetrySensor = g_model.telemetrySensors[index];

  telemetrySensor.id = id;
  telemetrySensor.instance = subId;

  const GhostSensor * sensor = getGhostSensor(id);
  if (sensor) {
    TelemetryUnit unit = sensor->unit;
    uint8_t prec = min<uint8_t>(2, sensor->precision);
    telemetrySensor.init(sensor->name, unit, prec);
  }
  else
    telemetrySensor.init(id);

  storageDirty(EE_MODEL);
}
