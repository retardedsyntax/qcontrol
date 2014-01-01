/*
 * Copyright (C) 2014  Ian Campbell (ijc@hellion.org.uk)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef QNAP_PIC_H
#define QNAP_PIC_H

/*
 * QNAP_PICCMD_* -- Command codes sent to PIC
 * QNAP_PICSTS_* -- Status codes received from PIC
 */
#define QNAP_PICCMD_FAN_STOP			0x30
#define QNAP_PICCMD_FAN_SILENCE			0x31
#define QNAP_PICCMD_FAN_LOW			0x32
#define QNAP_PICCMD_FAN_MEDIUM			0x33
#define QNAP_PICCMD_FAN_HIGH			0x34
#define QNAP_PICCMD_FAN_FULL			0x35

#define QNAP_PICSTS_SYS_TEMP_71_79		0x38
#define QNAP_PICSTS_SYS_TEMP_80			0x39
#define QNAP_PICSTS_TEMP_WARM_TO_HOT		0x3a
#define QNAP_PICSTS_TEMP_HOT_TO_WARM		0x3b
#define QNAP_PICSTS_TEMP_COLD_TO_WARM		0x3c
#define QNAP_PICSTS_TEMP_WARM_TO_COLD		0x3d

#define QNAP_PICSTS_POWER_BUTTON		0x40
#define QNAP_PICSTS_POWER_LOSS_POWER_OFF	0x43

#define QNAP_PICCMD_AUTOPOWER_ON		0x48
#define QNAP_PICCMD_AUTOPOWER_OFF		0x49

#define QNAP_PICCMD_POWER_LED_OFF		0x4b
#define QNAP_PICCMD_POWER_LED_2HZ		0x4c
#define QNAP_PICCMD_POWER_LED_ON		0x4d
#define QNAP_PICCMD_POWER_LED_1HZ		0x4e

#define QNAP_PICCMD_BUZZER_SHORT		0x50
#define QNAP_PICCMD_BUZZER_LONG			0x51

#define QNAP_PICCMD_STATUS_RED_2HZ		0x54
#define QNAP_PICCMD_STATUS_GREEN_2HZ		0x55
#define QNAP_PICCMD_STATUS_GREEN_ON		0x56
#define QNAP_PICCMD_STATUS_RED_ON		0x57
#define QNAP_PICCMD_STATUS_BOTH_2HZ		0x58
#define QNAP_PICCMD_STATUS_OFF			0x59
#define QNAP_PICCMD_STATUS_GREEN_1HZ		0x5a
#define QNAP_PICCMD_STATUS_RED_1HZ		0x5b
#define QNAP_PICCMD_STATUS_BOTH_1HZ		0x5c

#define QNAP_PICCMD_USB_LED_ON			0x60
#define QNAP_PICCMD_USB_LED_8HZ			0x61
#define QNAP_PICCMD_USB_LED_OFF			0x62

#define QNAP_PICCMD_WDT_OFF			0x67

#define QNAP_PICSTS_FAN1_ERROR			0x73
#define QNAP_PICSTS_FAN1_NORMAL			0x74
#define QNAP_PICSTS_FAN2_ERROR			0x75
#define QNAP_PICSTS_FAN2_NORMAL			0x76
#define QNAP_PICSTS_FAN3_ERROR			0x77
#define QNAP_PICSTS_FAN3_NORMAL			0x78
#define QNAP_PICSTS_FAN4_ERROR			0x79
#define QNAP_PICSTS_FAN4_NORMAL			0x7a

#define QNAP_PICSTS_SYS_TEMP_0			0x80
#define QNAP_PICSTS_SYS_TEMP_70			0xc6

#define QNAP_PICCMD_WOL_ENABLE			0xf2
#define QNAP_PICCMD_WOL_DISABLE			0xf3
#define QNAP_PICCMD_EUP_DISABLE			0xf4
#define QNAP_PICCMD_EUP_ENABLE			0xf5

#endif
