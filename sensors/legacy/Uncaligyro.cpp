/* Copyright Statement:
 *
 * This software/firmware and related documentation ("MediaTek Software") are
 * protected under relevant copyright laws. The information contained herein
 * is confidential and proprietary to MediaTek Inc. and/or its licensors.
 * Without the prior written permission of MediaTek inc. and/or its licensors,
 * any reproduction, modification, use or disclosure of MediaTek Software,
 * and information contained herein, in whole or in part, shall be strictly prohibited.
 */
/* MediaTek Inc. (C) 2012. All rights reserved.
 *
 * BY OPENING THIS FILE, RECEIVER HEREBY UNEQUIVOCALLY ACKNOWLEDGES AND AGREES
 * THAT THE SOFTWARE/FIRMWARE AND ITS DOCUMENTATIONS ("MEDIATEK SOFTWARE")
 * RECEIVED FROM MEDIATEK AND/OR ITS REPRESENTATIVES ARE PROVIDED TO RECEIVER ON
 * AN "AS-IS" BASIS ONLY. MEDIATEK EXPRESSLY DISCLAIMS ANY AND ALL WARRANTIES,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE OR NONINFRINGEMENT.
 * NEITHER DOES MEDIATEK PROVIDE ANY WARRANTY WHATSOEVER WITH RESPECT TO THE
 * SOFTWARE OF ANY THIRD PARTY WHICH MAY BE USED BY, INCORPORATED IN, OR
 * SUPPLIED WITH THE MEDIATEK SOFTWARE, AND RECEIVER AGREES TO LOOK ONLY TO SUCH
 * THIRD PARTY FOR ANY WARRANTY CLAIM RELATING THERETO. RECEIVER EXPRESSLY ACKNOWLEDGES
 * THAT IT IS RECEIVER'S SOLE RESPONSIBILITY TO OBTAIN FROM ANY THIRD PARTY ALL PROPER LICENSES
 * CONTAINED IN MEDIATEK SOFTWARE. MEDIATEK SHALL ALSO NOT BE RESPONSIBLE FOR ANY MEDIATEK
 * SOFTWARE RELEASES MADE TO RECEIVER'S SPECIFICATION OR TO CONFORM TO A PARTICULAR
 * STANDARD OR OPEN FORUM. RECEIVER'S SOLE AND EXCLUSIVE REMEDY AND MEDIATEK'S ENTIRE AND
 * CUMULATIVE LIABILITY WITH RESPECT TO THE MEDIATEK SOFTWARE RELEASED HEREUNDER WILL BE,
 * AT MEDIATEK'S OPTION, TO REVISE OR REPLACE THE MEDIATEK SOFTWARE AT ISSUE,
 * OR REFUND ANY SOFTWARE LICENSE FEES OR SERVICE CHARGE PAID BY RECEIVER TO
 * MEDIATEK FOR SUCH MEDIATEK SOFTWARE AT ISSUE.
 *
 * The following software/firmware and/or related documentation ("MediaTek Software")
 * have been modified by MediaTek Inc. All revisions are subject to any receiver's
 * applicable license agreements with MediaTek Inc.
 */

#include <fcntl.h>
#include <errno.h>
#include <math.h>
#include <poll.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/select.h>
#include <string.h>

#include <utils/Timers.h>
#include <cutils/log.h>

#include "Uncaligyro.h"
#include <utils/SystemClock.h>
#ifdef LOG_TAG
#undef LOG_TAG
#define LOG_TAG "UNGYRO"
#endif

#define IGNORE_EVENT_TIME 350000000
#define SYSFS_PATH           "/sys/class/input"


/*****************************************************************************/
UncaligyroSensor::UncaligyroSensor()
    : SensorBase(NULL, "m_uncali_gyro_input"),  // UNGYRO_INPUTDEV_NAME
      mEnabled(0),
      mInputReader(32) {
    mPendingEvent.version = sizeof(sensors_event_t);
    mPendingEvent.sensor = ID_GYROSCOPE_UNCALIBRATED;
    mPendingEvent.type = SENSOR_TYPE_GYROSCOPE_UNCALIBRATED;
    mPendingEvent.acceleration.status = SENSOR_STATUS_ACCURACY_HIGH;
    memset(mPendingEvent.data, 0x00, sizeof(mPendingEvent.data));
    mEnabledTime = 0;
    mDataDiv = 1;
    mPendingEvent.timestamp = 0;
    input_sysfs_path_len = 0;
    input_sysfs_path[PATH_MAX];
    memset(input_sysfs_path, 0, PATH_MAX);
    m_uncali_gyro_last_ts = 0;
    m_uncali_gyro_delay = 0;

    char datapath[64]={"/sys/class/misc/m_uncali_gyro_misc/ungyroactive"};
    int fd = -1;
    char buf[64]={0};
    int len;

    mdata_fd = FindDataFd();
    if (mdata_fd >= 0) {
        strcpy(input_sysfs_path, "/sys/class/misc/m_uncali_gyro_misc/");
        input_sysfs_path_len = strlen(input_sysfs_path);
    } else {
        ALOGE("couldn't find input device ");
        return;
    }
    ALOGD("uncali_gyro misc path =%s", input_sysfs_path);

    fd = open(datapath, O_RDWR);
    if (fd >= 0) {
        len = read(fd, buf, sizeof(buf)-1);
        if (len <= 0) {
            ALOGD("read div err, len = %d", len);
        } else {
            buf[len] = '\0';
            sscanf(buf, "%d", &mDataDiv);
            ALOGD("read div buf(%s), mdiv %d", datapath, mDataDiv);
        }
        close(fd);
    } else {
        ALOGE("open ungyro misc path %s fail ", datapath);
    }
}

UncaligyroSensor::~UncaligyroSensor() {
    if (mdata_fd >= 0)
        close(mdata_fd);
}
int UncaligyroSensor::FindDataFd() {
    int fd = -1;
    int num = -1;
    char buf[64]={0};
    const char *devnum_dir = NULL;
    char buf_s[64] = {0};
    int len;

    devnum_dir = "/sys/class/misc/m_uncali_gyro_misc/ungyrodevnum";

    fd = open(devnum_dir, O_RDONLY);
    if (fd >= 0) {
        len = read(fd, buf, sizeof(buf)-1);
        close(fd);
        if (len <= 0) {
            ALOGD("read devnum err, len = %d", len);
            return -1;
        } else {
            buf[len] = '\0';
            sscanf(buf, "%d\n", &num);
        }
    } else {
        return -1;
    }
    sprintf(buf_s, "/dev/input/event%d", num);
    fd = open(buf_s, O_RDONLY);
    ALOGE_IF(fd < 0, "couldn't find input device");
    return fd;
}

int UncaligyroSensor::enable(int32_t handle, int en) {
    int fd = -1;
    int flags = en ? 1 : 0;

    ALOGD("Gyro enable: handle:%d, en:%d\r\n", handle, en);
    strcpy(&input_sysfs_path[input_sysfs_path_len], "ungyroactive");
    ALOGD("path:%s \r\n", input_sysfs_path);
    fd = open(input_sysfs_path, O_RDWR);
    if (fd < 0) {
         ALOGD("no Gyro enable control attr\r\n");
         return -1;
    }

    mEnabled = flags;
    char buf[2]={0};
    buf[1] = 0;
    if (flags) {
        buf[0] = '1';
        mEnabledTime = getTimestamp() + IGNORE_EVENT_TIME;
        m_uncali_gyro_last_ts = 0;
    } else {
         buf[0] = '0';
    }
    write(fd, buf, sizeof(buf));
    close(fd);

    ALOGD("Gyro enable(%d) done", mEnabled);
    return 0;
}

int UncaligyroSensor::setDelay(int32_t handle, int64_t ns) {
    int fd = -1;
    ALOGD("setDelay: (handle=%d, ns=%lld)", handle, ns);
    m_uncali_gyro_delay = ns;
    strcpy(&input_sysfs_path[input_sysfs_path_len], "ungyrodelay");
    fd = open(input_sysfs_path, O_RDWR);
    if (fd < 0) {
         ALOGD("no UNGYRO setDelay control attr\r\n");
         return -1;
    }
    char buf[80]={0};
    sprintf(buf, "%lld", ns);
    write(fd, buf, strlen(buf)+1);
    close(fd);
    return 0;
}

int UncaligyroSensor::batch(int handle, int flags, int64_t samplingPeriodNs, int64_t maxBatchReportLatencyNs) {
    int fd;
    int flag;
    ALOGE("UNGYRO batch: handle:%d, en:%d,samplingPeriodNs:%lld, maxBatchReportLatencyNs:%lld\r\n",
        handle, flags, samplingPeriodNs, maxBatchReportLatencyNs);

    // Don't change batch status if dry run.
    if (flags & SENSORS_BATCH_DRY_RUN)
        return 0;
    if (maxBatchReportLatencyNs == 0) {
        flag = 0;
    } else {
        flag = 1;
    }

    strcpy(&input_sysfs_path[input_sysfs_path_len], "ungyrobatch");
    ALOGD("path:%s \r\n", input_sysfs_path);
    fd = open(input_sysfs_path, O_RDWR);
    if (fd < 0) {
          ALOGD("no uncali_gyro batch control attr\r\n");
          return -1;
    }

    char buf[2]={0};
    buf[1] = 0;
    if (flag) {
         buf[0] = '1';
    } else {
         buf[0] = '0';
    }
    write(fd, buf, sizeof(buf));
    close(fd);

    ALOGD("UNGYRO batch(%d) done", flag);
    return 0;
}

int UncaligyroSensor::flush(int handle) {
    ALOGD("handle=%d\n", handle);
    return -errno;
}

int UncaligyroSensor::readEvents(sensors_event_t* data, int count) {
    // ALOGE("fwq read Event 1\r\n");
    if (count < 1)
        return -EINVAL;

    ssize_t n = mInputReader.fill(mdata_fd);
    if (n < 0)
        return n;
    int numEventReceived = 0;
    input_event const* event;

    while (count && mInputReader.readEvent(&event)) {
        int type = event->type;
        // ALOGE("fwq1....\r\n");
        if (type == EV_ABS) {
            processEvent(event->code, event->value);
            // ALOGE("fwq2....\r\n");
        } else if (type == EV_SYN) {
            // ALOGE("fwq3....\r\n");
            int64_t time = android::elapsedRealtimeNano();
            mPendingEvent.timestamp = time;
            if (mEnabled) {
                 // ALOGE("fwq4....\r\n");
                 if (mPendingEvent.timestamp >= mEnabledTime) {
                    // ALOGE("fwq5....\r\n");
                    float delta_mod =
                        (float)(mPendingEvent.timestamp - m_uncali_gyro_last_ts) / (float)(m_uncali_gyro_delay);
                    if (m_uncali_gyro_delay == 1000000000)
                        delta_mod = 0;
                    // ALOGE("fwq uncali_gyro-delta_mod=%f\r\n",delta_mod);
                    int loopcout = delta_mod;
                    // ALOGE("fwq uncali_gyro-loopcout=%d \r\n",loopcout);
                    if (loopcout >= 1 && loopcout < 100) {
                        for (int i = 0; i < loopcout; i++) {
                            mPendingEvent.timestamp = time - (loopcout-i) * m_uncali_gyro_delay;
                            // ALOGE("fwq_n uncali_gyro fack event [%lld ] \r\n",mPendingEvent.timestamp);
                            *data++ = mPendingEvent;
                            numEventReceived++;
                            count--;
                            if (0 == count)
                                break;
                        }
                    }
                    if (count != 0) {
                        mPendingEvent.timestamp = time;
                        *data++ = mPendingEvent;
                        numEventReceived++;
                    }
                 }
                 if (count != 0)
                    count--;
            }
            m_uncali_gyro_last_ts = mPendingEvent.timestamp;
        } else if (type != EV_ABS) {
            if (EVENT_TYPE_UNCALI_GYRO_UPDATE != event->code) {
                ALOGE("UncaligyroSensor: unknown event (type=%d, code=%d)",
                    type, event->code);
            }
        }
        mInputReader.next();
    }
    // ALOGE("fwq read Event 2\r\n");
    return numEventReceived;
}

void UncaligyroSensor::processEvent(int code, int value) {
    /* ALOGD("processEvent code=%d,value=%d\r\n",code, value); */
    switch (code) {
       case EVENT_TYPE_UNCALI_GYRO_X:
                mPendingEvent.uncalibrated_gyro.x_uncalib = (float)value / 7506;
                break;
       case EVENT_TYPE_UNCALI_GYRO_Y:
                mPendingEvent.uncalibrated_gyro.y_uncalib = (float)value / 7506;
                break;
       case EVENT_TYPE_UNCALI_GYRO_Z:
                mPendingEvent.uncalibrated_gyro.z_uncalib = (float)value / 7506;
                break;
       case EVENT_TYPE_UNCALI_GYRO_X_BIAS:
                mPendingEvent.uncalibrated_gyro.x_bias = (float)value / 7506;
                break;
       case EVENT_TYPE_UNCALI_GYRO_Y_BIAS:
                mPendingEvent.uncalibrated_gyro.y_bias = (float)value / 7506;
                break;
       case EVENT_TYPE_UNCALI_GYRO_Z_BIAS:
                mPendingEvent.uncalibrated_gyro.z_bias = (float)value / 7506;
                break;
    }
}
