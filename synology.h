/*
 * Copyright (C) 2014  Ben Peddell (klightspeed@killerwolves.net)
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

#ifndef SYNOLOGY_PIC_H
#define SYNOLOGY_PIC_H

/*
 * SYNOLOGY_PICCMD_* -- Command codes sent to PIC
 * SYNOLOGY_PICSTS_* -- Status codes received from PIC
 */
#define SYNOLOGY_PICSTS_POWER_BUTTON		0x30
#define SYNOLOGY_PICSTS_MEDIA_BUTTON		0x60
#define SYNOLOGY_PICSTS_RESET_BUTTON		0x61

#define SYNOLOGY_PICCMD_AUTOPOWER_ON		0x71
#define SYNOLOGY_PICCMD_AUTOPOWER_OFF		0x70

#define SYNOLOGY_PICCMD_POWER_LED_ON		0x34
#define SYNOLOGY_PICCMD_POWER_LED_2HZ		0x35
#define SYNOLOGY_PICCMD_POWER_LED_OFF		0x36

#define SYNOLOGY_PICCMD_BUZZER_SHORT		0x32
#define SYNOLOGY_PICCMD_BUZZER_LONG		0x33

#define SYNOLOGY_PICCMD_STATUS_GREEN_ON		0x38
#define SYNOLOGY_PICCMD_STATUS_ORANGE_ON	0x3A
#define SYNOLOGY_PICCMD_STATUS_OFF		0x37
#define SYNOLOGY_PICCMD_STATUS_GREEN_2HZ	0x39
#define SYNOLOGY_PICCMD_STATUS_ORANGE_2HZ	0x3B

#define SYNOLOGY_PICCMD_USB_LED_ON		0x40
#define SYNOLOGY_PICCMD_USB_LED_2HZ		0x41
#define SYNOLOGY_PICCMD_USB_LED_OFF		0x42

#define SYNOLOGY_PICCMD_RTC_ENABLE		0x73
#define SYNOLOGY_PICCMD_RTC_DISABLE		0x72

#endif
