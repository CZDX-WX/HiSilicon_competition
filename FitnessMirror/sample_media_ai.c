/*
 * Copyright (c) 2022 HiSilicon (Shanghai) Technologies CO., LIMITED.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/prctl.h>

#include "hi_mipi_tx.h"
#include "sdk.h"
#include "sample_comm.h"
#include "ai_infer_process.h"
#include "vgs_img.h"
#include "osd_img.h"
#include "posix_help.h"
#include "sample_media_ai.h"
#include "sample_audio.h"
#include "hand_classify.h"
#include "gpio_user.h"
#include "uart_user.h"
#include <arpa/inet.h>
#include <sys/socket.h>

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif /* End of #ifdef __cplusplus */

AicMediaInfo aicMediaInfo = { 0 };
AiPlugLib AiPlug = {0};
HI_S32 ai_fd = 0;

#define DEBUGMODE             0

#ifndef DEBUGMODE
    #error "DEBUGMODE is not defined"
#endif

#define OBSTACLE_FRM_WIDTH      640
#define OBSTACLE_FRM_HEIGHT     384

#define USLEEP_TIME             1000 // 1000: usleep time, in microseconds
#define G_MBUF_LENGTH           50 // 50: length of g_mbuf
#define ALIGN_DOWN_SIZE         2

// #define SERVER_IP               "192.168.137.1"
// #define SERVER_PORT             8888
// #define BROADCAST_PORT          9999
#define MTU_USER                60000

// extern uint8_t audioBusy;
uint8_t AiProcessStopFlag = 0;
uint8_t snapFlag = 0;
uint8_t AiFlag = 0;
// int8_t audioFlag = 0;

static unsigned char g_mBuf[G_MBUF_LENGTH];
static SampleVoModeMux g_sampleVoModeMux = {0};
static VO_PUB_ATTR_S stVoPubAttr = {0};
static VO_VIDEO_LAYER_ATTR_S  stLayerAttr    = {0};
static VO_CSC_S               stVideoCSC     = {0};
static RECT_S stDefDispRect  = {0, 0, 800, 480};
static SIZE_S stDefImageSize = {800, 480};

typedef struct StSampleUserVoConfigs {
    VO_SYNC_INFO_S stSyncInfo;
    VO_USER_INTFSYNC_ATTR_S stUserIntfSyncAttr;
    HI_U32 u32PreDiv;
    HI_U32 u32DevDiv;
    HI_U32 u32Framerate;
    combo_dev_cfg_t stcombo_dev_cfgl;
} SAMPLE_USER_VO_CONFIG_S;

HI_VOID SAMPLE_VOU_SYS_Exit(void)
{
    HI_MPI_SYS_Exit();
    HI_MPI_VB_Exit();
}

HI_VOID SAMPLE_VO_GetUserLayerAttr(VO_VIDEO_LAYER_ATTR_S *pstLayerAttr, SIZE_S *pstDevSize)
{
    pstLayerAttr->bClusterMode = HI_FALSE;
    pstLayerAttr->bDoubleFrame = HI_FALSE;
    pstLayerAttr->enDstDynamicRange = DYNAMIC_RANGE_SDR8;
    pstLayerAttr->enPixFormat = PIXEL_FORMAT_YVU_SEMIPLANAR_420;

    pstLayerAttr->stDispRect.s32X = 0;
    pstLayerAttr->stDispRect.s32Y = 0;
    pstLayerAttr->stDispRect.u32Height = pstDevSize->u32Height;
    pstLayerAttr->stDispRect.u32Width  = pstDevSize->u32Width;

    pstLayerAttr->stImageSize.u32Height = pstDevSize->u32Height;
    pstLayerAttr->stImageSize.u32Width = pstDevSize->u32Width;

    return;
}

HI_VOID SAMPLE_VO_GetUserChnAttr(VO_CHN_ATTR_S *pstChnAttr, SIZE_S *pstDevSize, HI_S32 VoChnNum)
{
    HI_S32 i;
    for (i = 0; i < VoChnNum; i++) {
        pstChnAttr[i].bDeflicker = HI_FALSE;
        pstChnAttr[i].u32Priority = 0;
        pstChnAttr[i].stRect.s32X = 0;
        pstChnAttr[i].stRect.s32Y = 0;
        pstChnAttr[i].stRect.u32Height = pstDevSize->u32Height;
        pstChnAttr[i].stRect.u32Width = pstDevSize->u32Width;
        }

    return;
}

/*
 * 打开MIPI Tx设备
 * Open MIPI Tx device
 */
HI_S32 SampleOpenMipiTxFd(HI_VOID)
{
    HI_S32 fd;

    fd = open("/dev/hi_mipi_tx", O_RDWR);
    if (fd < 0) {
        printf("open hi_mipi_tx dev failed\n");
    }
    return fd;
}

/*
 * 关闭MIPI Tx设备
 * Close MIPI Tx device
 */
HI_VOID SampleCloseMipiTxFd(HI_S32 fd)
{
    close(fd);
    return;
}

/*
 * 获取MIPI Tx配置信息
 * Get MIPI Tx config information
 */
HI_VOID SAMPLE_GetMipiTxConfig(combo_dev_cfg_t *pstMipiTxConfig)
{
    /*
     * 用户需要设置MIPI设备配置
     * User need set MIPI device config
     */
    pstMipiTxConfig->devno = 0;
    pstMipiTxConfig->lane_id[LANE_ID_SUBSCRIPT_0] = 0;
    pstMipiTxConfig->lane_id[LANE_ID_SUBSCRIPT_1] = 1;
    // -1: 2 lane mode configuration,lane_id[4] = {0, 1, -1, -1}
    pstMipiTxConfig->lane_id[LANE_ID_SUBSCRIPT_2] = -1;
    // -1: 2 lane mode configuration,lane_id[4] = {0, 1, -1, -1}
    pstMipiTxConfig->lane_id[LANE_ID_SUBSCRIPT_3] = -1;
    pstMipiTxConfig->output_mode = OUTPUT_MODE_DSI_VIDEO;
    pstMipiTxConfig->output_format = OUT_FORMAT_RGB_24_BIT;
    pstMipiTxConfig->video_mode = BURST_MODE;
    pstMipiTxConfig->sync_info.vid_pkt_size = 480; // 480: received packet size
    pstMipiTxConfig->sync_info.vid_hsa_pixels = 10; // 10: The number of pixels in the input line sync pulse area
    pstMipiTxConfig->sync_info.vid_hbp_pixels = 50; // 50: Number of pixels in blanking area after input
    pstMipiTxConfig->sync_info.vid_hline_pixels = 590; // 590: The total number of pixels detected per line
    pstMipiTxConfig->sync_info.vid_vsa_lines = 4; // 4: Number of frame sync pulse lines detected
    pstMipiTxConfig->sync_info.vid_vbp_lines = 20; // 20: Number of blanking area lines after frame sync pulse
    pstMipiTxConfig->sync_info.vid_vfp_lines = 20; // 20: Number of blanking area lines before frame sync pulse
    pstMipiTxConfig->sync_info.vid_active_lines = 800; // 800: VACTIVE rows
    pstMipiTxConfig->sync_info.edpi_cmd_size = 0; // 0: Write memory command bytes
    pstMipiTxConfig->phy_data_rate = 359; // 359: MIPI Tx output rate
    pstMipiTxConfig->pixel_clk = 29878; // 29878: pixel clock. The unit is KHz

    return;
}

/*
 * 设置MIPI Tx配置信息
 * Set MIPI Tx config information
 */
HI_S32 SAMPLE_SetMipiTxConfig(HI_S32 fd, combo_dev_cfg_t *pstMipiTxConfig)
{
    HI_S32 s32Ret = ioctl(fd, HI_MIPI_TX_SET_DEV_CFG, pstMipiTxConfig);
    if (s32Ret != HI_SUCCESS) {
        printf("MIPI_TX SET_DEV_CONFIG failed\n");
        SampleCloseMipiTxFd(fd);
        return s32Ret;
    }
    return s32Ret;
}

/*
 * 设置MIPI Tx设备属性
 * Set MIPI Tx device attr
 */
HI_S32 SampleSetMipiTxDevAttr(HI_S32 fd)
{
    HI_S32 s32Ret;
    combo_dev_cfg_t stMipiTxConfig;

    SAMPLE_GetMipiTxConfig(&stMipiTxConfig);
    s32Ret = SAMPLE_SetMipiTxConfig(fd, &stMipiTxConfig);

    return s32Ret;
}

/*
 * 初始化MIPI Tx设备
 * Init MIPI Tx device
 */
HI_S32 SAMPLE_USER_INIT_MIPITx(HI_S32 fd, cmd_info_t *pcmd_info)
{
    HI_S32 s32Ret = ioctl(fd, HI_MIPI_TX_SET_CMD, pcmd_info);
    if (s32Ret !=  HI_SUCCESS) {
        printf("MIPI_TX SET CMD failed\n");
        SampleCloseMipiTxFd(fd);
        return s32Ret;
    }

    return HI_SUCCESS;
}

/*
 * 配置MIPI Tx初始化序列
 * Config MIPI Tx initialization sequence
 */
HI_S32 SampleVoInitMipiTxScreen(HI_S32 fd)
{
    HI_S32 s32Ret;
    cmd_info_t cmd_info;
    memset_s(g_mBuf, G_MBUF_LENGTH, 0, G_MBUF_LENGTH);
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_0] = 0xFF;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_1] = 0x77;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_2] = 0x01;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_3] = 0x00;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_4] = 0x00;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_5] = 0x13;
    cmd_info.devno = 0;
    cmd_info.cmd_size = 6; // 6: command data size
    cmd_info.data_type = 0x29;
    cmd_info.cmd = g_mBuf;
    s32Ret = SAMPLE_USER_INIT_MIPITx(fd, &cmd_info);
    if (s32Ret != HI_SUCCESS) {
        return s32Ret;
    }
    usleep(USLEEP_TIME);

    cmd_info.devno = 0;
    cmd_info.cmd_size = 0x08ef;
    cmd_info.data_type = 0x23;
    cmd_info.cmd = NULL;
    s32Ret = SAMPLE_USER_INIT_MIPITx(fd, &cmd_info);
    if (s32Ret != HI_SUCCESS) {
        return s32Ret;
    }
    usleep(USLEEP_TIME);

    memset_s(g_mBuf, G_MBUF_LENGTH, 0, G_MBUF_LENGTH);
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_0] = 0xFF;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_1] = 0x77;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_2] = 0x01;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_3] = 0x00;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_4] = 0x00;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_5] = 0x10;
    cmd_info.devno = 0;
    cmd_info.cmd_size = 6; // 6: command data size
    cmd_info.data_type = 0x29;
    cmd_info.cmd = g_mBuf;
    s32Ret = SAMPLE_USER_INIT_MIPITx(fd, &cmd_info);
    if (s32Ret != HI_SUCCESS) {
        return s32Ret;
    }
    usleep(USLEEP_TIME);

    memset_s(g_mBuf, G_MBUF_LENGTH, 0, G_MBUF_LENGTH);
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_0] = 0xC0;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_1] = 0x63;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_2] = 0x00;
    cmd_info.devno = 0;
    cmd_info.cmd_size = 3; // 3: command data size
    cmd_info.data_type = 0x29;
    cmd_info.cmd = g_mBuf;
    s32Ret = SAMPLE_USER_INIT_MIPITx(fd, &cmd_info);
    if (s32Ret != HI_SUCCESS) {
        return s32Ret;
    }
    usleep(USLEEP_TIME);

    memset_s(g_mBuf, G_MBUF_LENGTH, 0, G_MBUF_LENGTH);
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_0] = 0xC1;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_1] = 0x10;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_2] = 0x02;
    cmd_info.devno = 0;
    cmd_info.cmd_size = 3; // 3: command data size
    cmd_info.data_type = 0x29;
    cmd_info.cmd = g_mBuf;
    s32Ret = SAMPLE_USER_INIT_MIPITx(fd, &cmd_info);
    if (s32Ret != HI_SUCCESS) {
        return s32Ret;
    }
    usleep(USLEEP_TIME);

    memset_s(g_mBuf, G_MBUF_LENGTH, 0, G_MBUF_LENGTH);
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_0] = 0xC2;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_1] = 0x01;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_2] = 0x08;
    cmd_info.devno = 0;
    cmd_info.cmd_size = 3; // 3: command data size
    cmd_info.data_type = 0x29;
    cmd_info.cmd = g_mBuf;
    s32Ret = SAMPLE_USER_INIT_MIPITx(fd, &cmd_info);
    if (s32Ret != HI_SUCCESS) {
        return s32Ret;
    }
    usleep(USLEEP_TIME);

    cmd_info.devno = 0;
    cmd_info.cmd_size = 0x18CC;
    cmd_info.data_type = 0x23;
    cmd_info.cmd = NULL;
    s32Ret = SAMPLE_USER_INIT_MIPITx(fd, &cmd_info);
    if (s32Ret != HI_SUCCESS) {
        return s32Ret;
    }
    usleep(USLEEP_TIME);

    memset_s(g_mBuf, G_MBUF_LENGTH, 0, G_MBUF_LENGTH);
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_0] = 0xB0;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_1] = 0x40;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_2] = 0xC9;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_3] = 0x8F;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_4] = 0x0D;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_5] = 0x11;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_6] = 0x07;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_7] = 0x02;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_8] = 0x09;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_9] = 0x09;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_10] = 0x1F;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_11] = 0x04;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_12] = 0x50;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_13] = 0x0F;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_14] = 0xE4;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_15] = 0x29;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_16] = 0xDF;
    cmd_info.devno = 0;
    cmd_info.cmd_size = 17; // 17: command data size
    cmd_info.data_type = 0x29;
    cmd_info.cmd = g_mBuf;
    s32Ret = SAMPLE_USER_INIT_MIPITx(fd, &cmd_info);
    if (s32Ret != HI_SUCCESS) {
        return s32Ret;
    }
    usleep(USLEEP_TIME);

    memset_s(g_mBuf, G_MBUF_LENGTH, 0, G_MBUF_LENGTH);
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_0] = 0xB1;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_1] = 0x40;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_2] = 0xCB;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_3] = 0xD3;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_4] = 0x11;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_5] = 0x8F;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_6] = 0x04;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_7] = 0x00;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_8] = 0x08;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_9] = 0x07;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_10] = 0x1C;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_11] = 0x06;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_12] = 0x53;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_13] = 0x12;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_14] = 0x63;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_15] = 0xEB;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_16] = 0xDF;
    cmd_info.devno = 0;
    cmd_info.cmd_size = 17; // 17: command data size
    cmd_info.data_type = 0x29;
    cmd_info.cmd = g_mBuf;
    s32Ret = SAMPLE_USER_INIT_MIPITx(fd, &cmd_info);
    if (s32Ret != HI_SUCCESS) {
        return s32Ret;
    }
    usleep(USLEEP_TIME);

    memset_s(g_mBuf, G_MBUF_LENGTH, 0, G_MBUF_LENGTH);
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_0] = 0xFF;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_1] = 0x77;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_2] = 0x01;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_3] = 0x00;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_4] = 0x00;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_5] = 0x11;
    cmd_info.devno = 0;
    cmd_info.cmd_size = 6; // 6: command data size
    cmd_info.data_type = 0x29;
    cmd_info.cmd = g_mBuf;
    s32Ret = SAMPLE_USER_INIT_MIPITx(fd, &cmd_info);
    if (s32Ret != HI_SUCCESS) {
        return s32Ret;
    }
    usleep(USLEEP_TIME);

    cmd_info.devno = 0;
    cmd_info.cmd_size = 0x65b0;
    cmd_info.data_type = 0x23;
    cmd_info.cmd = NULL;
    s32Ret = SAMPLE_USER_INIT_MIPITx(fd, &cmd_info);
    if (s32Ret != HI_SUCCESS) {
        return s32Ret;
    }
    usleep(USLEEP_TIME);

    cmd_info.devno = 0;
    cmd_info.cmd_size = 0x34b1;
    cmd_info.data_type = 0x23;
    cmd_info.cmd = NULL;
    s32Ret = SAMPLE_USER_INIT_MIPITx(fd, &cmd_info);
    if (s32Ret != HI_SUCCESS) {
        return s32Ret;
    }
    usleep(USLEEP_TIME);

    cmd_info.devno = 0;
    cmd_info.cmd_size = 0x87b2;
    cmd_info.data_type = 0x23;
    cmd_info.cmd = NULL;
    s32Ret = SAMPLE_USER_INIT_MIPITx(fd, &cmd_info);
    if (s32Ret != HI_SUCCESS) {
        return s32Ret;
    }
    usleep(USLEEP_TIME);

    cmd_info.devno = 0;
    cmd_info.cmd_size = 0x80b3;
    cmd_info.data_type = 0x23;
    cmd_info.cmd = NULL;
    s32Ret = SAMPLE_USER_INIT_MIPITx(fd, &cmd_info);
    if (s32Ret != HI_SUCCESS) {
        return s32Ret;
    }
    usleep(USLEEP_TIME);

    cmd_info.devno = 0;
    cmd_info.cmd_size = 0x49b5;
    cmd_info.data_type = 0x23;
    cmd_info.cmd = NULL;
    s32Ret = SAMPLE_USER_INIT_MIPITx(fd, &cmd_info);
    if (s32Ret != HI_SUCCESS) {
        return s32Ret;
    }
    usleep(USLEEP_TIME);

    cmd_info.devno = 0;
    cmd_info.cmd_size = 0x85b7;
    cmd_info.data_type = 0x23;
    cmd_info.cmd = NULL;
    s32Ret = SAMPLE_USER_INIT_MIPITx(fd, &cmd_info);
    if (s32Ret != HI_SUCCESS) {
        return s32Ret;
    }
    usleep(USLEEP_TIME);

    cmd_info.devno = 0;
    cmd_info.cmd_size = 0x20b8;
    cmd_info.data_type = 0x23;
    cmd_info.cmd = NULL;
    s32Ret = SAMPLE_USER_INIT_MIPITx(fd, &cmd_info);
    if (s32Ret != HI_SUCCESS) {
        return s32Ret;
    }
    usleep(USLEEP_TIME);

    cmd_info.devno = 0;
    cmd_info.cmd_size = 0x10b9;
    cmd_info.data_type = 0x23;
    cmd_info.cmd = NULL;
    s32Ret = SAMPLE_USER_INIT_MIPITx(fd, &cmd_info);
    if (s32Ret != HI_SUCCESS) {
        return s32Ret;
    }
    usleep(USLEEP_TIME);

    cmd_info.devno = 0;
    cmd_info.cmd_size = 0x78c1;
    cmd_info.data_type = 0x23;
    cmd_info.cmd = NULL;
    s32Ret = SAMPLE_USER_INIT_MIPITx(fd, &cmd_info);
    if (s32Ret != HI_SUCCESS) {
        return s32Ret;
    }
    usleep(USLEEP_TIME);

    cmd_info.devno = 0;
    cmd_info.cmd_size = 0x78c2;
    cmd_info.data_type = 0x23;
    cmd_info.cmd = NULL;
    s32Ret = SAMPLE_USER_INIT_MIPITx(fd, &cmd_info);
    if (s32Ret != HI_SUCCESS) {
        return s32Ret;
    }
    usleep(USLEEP_TIME);

    cmd_info.devno = 0;
    cmd_info.cmd_size = 0x88d0;
    cmd_info.data_type = 0x23;
    cmd_info.cmd = NULL;
    s32Ret = SAMPLE_USER_INIT_MIPITx(fd, &cmd_info);
    if (s32Ret != HI_SUCCESS) {
        return s32Ret;
    }
    usleep(USLEEP_TIME);
    usleep(100000);  // 100000: The process hangs for a period of time, in microseconds

    memset_s(g_mBuf, G_MBUF_LENGTH, 0, G_MBUF_LENGTH);
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_0] = 0xE0;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_1] = 0x00;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_2] = 0x19;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_3] = 0x02;
    cmd_info.devno = 0;
    cmd_info.cmd_size = 4; // 4: command data size
    cmd_info.data_type = 0x29;
    cmd_info.cmd = g_mBuf;
    s32Ret = SAMPLE_USER_INIT_MIPITx(fd, &cmd_info);
    if (s32Ret != HI_SUCCESS) {
        return s32Ret;
    }
    usleep(USLEEP_TIME);
    usleep(USLEEP_TIME);

    memset_s(g_mBuf, G_MBUF_LENGTH, 0, G_MBUF_LENGTH);
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_0] = 0xE1;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_1] = 0x05;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_2] = 0xA0;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_3] = 0x07;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_4] = 0xA0;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_5] = 0x04;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_6] = 0xA0;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_7] = 0x06;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_8] = 0xA0;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_9] = 0x00;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_10] = 0x44;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_11] = 0x44;
    cmd_info.devno = 0;
    cmd_info.cmd_size = 12; // 12: command data size
    cmd_info.data_type = 0x29;
    cmd_info.cmd = g_mBuf;
    s32Ret = SAMPLE_USER_INIT_MIPITx(fd, &cmd_info);
    if (s32Ret != HI_SUCCESS) {
        return s32Ret;
    }
    usleep(USLEEP_TIME);

    memset_s(g_mBuf, G_MBUF_LENGTH, 0, G_MBUF_LENGTH);
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_0] = 0xE2;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_1] = 0x00;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_2] = 0x00;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_3] = 0x00;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_4] = 0x00;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_5] = 0x00;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_6] = 0x00;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_7] = 0x00;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_8] = 0x00;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_9] = 0x00;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_10] = 0x00;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_11] = 0x00;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_12] = 0x00;
    cmd_info.devno = 0;
    cmd_info.cmd_size = 13; // 13: command data size
    cmd_info.data_type = 0x29;
    cmd_info.cmd = g_mBuf;
    s32Ret = SAMPLE_USER_INIT_MIPITx(fd, &cmd_info);
    if (s32Ret != HI_SUCCESS) {
        return s32Ret;
    }
    usleep(USLEEP_TIME);

    memset_s(g_mBuf, G_MBUF_LENGTH, 0, G_MBUF_LENGTH);
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_0] = 0xE3;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_1] = 0x00;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_2] = 0x00;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_3] = 0x33;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_4] = 0x33;
    cmd_info.devno = 0;
    cmd_info.cmd_size = 5; // 5: command data size
    cmd_info.data_type = 0x29;
    cmd_info.cmd = g_mBuf;
    s32Ret = SAMPLE_USER_INIT_MIPITx(fd, &cmd_info);
    if (s32Ret != HI_SUCCESS) {
        return s32Ret;
    }
    usleep(USLEEP_TIME);

    memset_s(g_mBuf, G_MBUF_LENGTH, 0, G_MBUF_LENGTH);
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_0] = 0xE4;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_1] = 0x44;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_2] = 0x44;
    cmd_info.devno = 0;
    cmd_info.cmd_size = 3; // 3: command data size
    cmd_info.data_type = 0x29;
    cmd_info.cmd = g_mBuf;
    s32Ret = SAMPLE_USER_INIT_MIPITx(fd, &cmd_info);
    if (s32Ret != HI_SUCCESS) {
        return s32Ret;
    }
    usleep(USLEEP_TIME);

    memset_s(g_mBuf, G_MBUF_LENGTH, 0, G_MBUF_LENGTH);
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_0] = 0xE5;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_1] = 0x0D;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_2] = 0x31;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_3] = 0xC8;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_4] = 0xAF;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_5] = 0x0F;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_6] = 0x33;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_7] = 0xC8;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_8] = 0xAF;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_9] = 0x09;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_10] = 0x2D;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_11] = 0xC8;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_12] = 0xAF;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_13] = 0x0B;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_14] = 0x2F;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_15] = 0xC8;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_16] = 0xAF;
    cmd_info.devno = 0;
    cmd_info.cmd_size = 17; // 17: command data size
    cmd_info.data_type = 0x29;
    cmd_info.cmd = g_mBuf;
    s32Ret = SAMPLE_USER_INIT_MIPITx(fd, &cmd_info);
    if (s32Ret != HI_SUCCESS) {
        return s32Ret;
    }
    usleep(USLEEP_TIME);

    memset_s(g_mBuf, G_MBUF_LENGTH, 0, G_MBUF_LENGTH);
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_0] = 0xE6;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_1] = 0x00;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_2] = 0x00;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_3] = 0x33;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_4] = 0x33;
    cmd_info.devno = 0;
    cmd_info.cmd_size = 5; // 5: command data size
    cmd_info.data_type = 0x29;
    cmd_info.cmd = g_mBuf;
    s32Ret = SAMPLE_USER_INIT_MIPITx(fd, &cmd_info);
    if (s32Ret != HI_SUCCESS) {
        return s32Ret;
    }
    usleep(USLEEP_TIME);

    memset_s(g_mBuf, G_MBUF_LENGTH, 0, G_MBUF_LENGTH);
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_0] = 0xE7;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_1] = 0x44;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_2] = 0x44;
    cmd_info.devno = 0;
    cmd_info.cmd_size = 3; // 3: command data size
    cmd_info.data_type = 0x29;
    cmd_info.cmd = g_mBuf;
    s32Ret = SAMPLE_USER_INIT_MIPITx(fd, &cmd_info);
    if (s32Ret != HI_SUCCESS) {
        return s32Ret;
    }
    usleep(USLEEP_TIME);

    memset_s(g_mBuf, G_MBUF_LENGTH, 0, G_MBUF_LENGTH);
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_0] = 0xE8;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_1] = 0x0C;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_2] = 0x30;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_3] = 0xC8;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_4] = 0xAF;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_5] = 0x0E;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_6] = 0x32;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_7] = 0xC8;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_8] = 0xAF;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_9] = 0x08;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_10] = 0x2C;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_11] = 0xC8;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_12] = 0xAF;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_13] = 0x0A;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_14] = 0x2E;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_15] = 0xC8;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_16] = 0xAF;
    cmd_info.devno = 0;
    cmd_info.cmd_size = 17; // 17: command data size
    cmd_info.data_type = 0x29;
    cmd_info.cmd = g_mBuf;
    s32Ret = SAMPLE_USER_INIT_MIPITx(fd, &cmd_info);
    if (s32Ret != HI_SUCCESS) {
        return s32Ret;
    }
    usleep(USLEEP_TIME);

    memset_s(g_mBuf, G_MBUF_LENGTH, 0, G_MBUF_LENGTH);
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_0] = 0xEB;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_1] = 0x02;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_2] = 0x00;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_3] = 0xE4;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_4] = 0xE4;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_5] = 0x44;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_6] = 0x00;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_7] = 0x40;
    cmd_info.devno = 0;
    cmd_info.cmd_size = 8; // 8: command data size
    cmd_info.data_type = 0x29;
    cmd_info.cmd = g_mBuf;
    s32Ret = SAMPLE_USER_INIT_MIPITx(fd, &cmd_info);
    if (s32Ret != HI_SUCCESS) {
        return s32Ret;
    }
    usleep(USLEEP_TIME);

    memset_s(g_mBuf, G_MBUF_LENGTH, 0, G_MBUF_LENGTH);
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_0] = 0xEC;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_1] = 0x3C;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_2] = 0x00;
    cmd_info.devno = 0;
    cmd_info.cmd_size = 3; // 3: command data size
    cmd_info.data_type = 0x29;
    cmd_info.cmd = g_mBuf;
    s32Ret = SAMPLE_USER_INIT_MIPITx(fd, &cmd_info);
    if (s32Ret != HI_SUCCESS) {
        return s32Ret;
    }
    usleep(USLEEP_TIME);

    memset_s(g_mBuf, G_MBUF_LENGTH, 0, G_MBUF_LENGTH);
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_0] = 0xED;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_1] = 0xAB;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_2] = 0x89;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_3] = 0x76;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_4] = 0x54;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_5] = 0x01;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_6] = 0xFF;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_7] = 0xFF;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_8] = 0xFF;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_9] = 0xFF;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_10] = 0xFF;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_11] = 0xFF;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_12] = 0x10;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_13] = 0x45;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_14] = 0x67;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_15] = 0x98;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_16] = 0xBA;
    cmd_info.devno = 0;
    cmd_info.cmd_size = 17; // 17: command data size
    cmd_info.data_type = 0x29;
    cmd_info.cmd = g_mBuf;
    s32Ret = SAMPLE_USER_INIT_MIPITx(fd, &cmd_info);
    if (s32Ret != HI_SUCCESS) {
        return s32Ret;
    }
    usleep(USLEEP_TIME);

    memset_s(g_mBuf, G_MBUF_LENGTH, 0, G_MBUF_LENGTH);
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_0] = 0xEF;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_1] = 0x08;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_2] = 0x08;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_3] = 0x08;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_4] = 0x45;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_5] = 0x3F;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_6] = 0x54;
    cmd_info.devno = 0;
    cmd_info.cmd_size = 7; // 7: command data size
    cmd_info.data_type = 0x29;
    cmd_info.cmd = g_mBuf;
    s32Ret = SAMPLE_USER_INIT_MIPITx(fd, &cmd_info);
    if (s32Ret != HI_SUCCESS) {
        return s32Ret;
    }
    usleep(USLEEP_TIME);

    memset_s(g_mBuf, G_MBUF_LENGTH, 0, G_MBUF_LENGTH);
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_0] = 0xFF;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_1] = 0x77;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_2] = 0x01;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_3] = 0x00;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_4] = 0x00;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_5] = 0x00;
    cmd_info.devno = 0;
    cmd_info.cmd_size = 6; // 6: command data size
    cmd_info.data_type = 0x29;
    cmd_info.cmd = g_mBuf;
    s32Ret = SAMPLE_USER_INIT_MIPITx(fd, &cmd_info);
    if (s32Ret != HI_SUCCESS) {
        return s32Ret;
    }
    usleep(USLEEP_TIME);

    memset_s(g_mBuf, G_MBUF_LENGTH, 0, G_MBUF_LENGTH);
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_0] = 0xFF;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_1] = 0x77;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_2] = 0x01;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_3] = 0x00;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_4] = 0x00;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_5] = 0x13;
    cmd_info.devno = 0;
    cmd_info.cmd_size = 6; // 6: command data size
    cmd_info.data_type = 0x29;
    cmd_info.cmd = g_mBuf;
    s32Ret = SAMPLE_USER_INIT_MIPITx(fd, &cmd_info);
    if (s32Ret != HI_SUCCESS) {
        return s32Ret;
    }
    usleep(USLEEP_TIME);

    memset_s(g_mBuf, G_MBUF_LENGTH, 0, G_MBUF_LENGTH);
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_0] = 0xE8;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_1] = 0x00;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_2] = 0x0E;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_3] = 0x11;
    cmd_info.devno = 0;
    cmd_info.cmd_size = 4; // 4: command data size
    cmd_info.data_type = 0x29;
    cmd_info.cmd = g_mBuf;
    s32Ret = SAMPLE_USER_INIT_MIPITx(fd, &cmd_info);
    if (s32Ret != HI_SUCCESS) {
        return s32Ret;
    }
    usleep(USLEEP_TIME);
    usleep(120000); // 120000: The process hangs for a period of time, in microseconds

    memset_s(g_mBuf, G_MBUF_LENGTH, 0, G_MBUF_LENGTH);
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_0] = 0xE8;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_1] = 0x00;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_2] = 0x0C;
    cmd_info.devno = 0;
    cmd_info.cmd_size = 3; // 3: command data size
    cmd_info.data_type = 0x29;
    cmd_info.cmd = g_mBuf;
    s32Ret = SAMPLE_USER_INIT_MIPITx(fd, &cmd_info);
    if (s32Ret != HI_SUCCESS) {
        return s32Ret;
    }
    usleep(USLEEP_TIME);
    usleep(10000); // 10000: The process hangs for a period of time, in microseconds

    memset_s(g_mBuf, G_MBUF_LENGTH, 0, G_MBUF_LENGTH);
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_0] = 0xE8;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_1] = 0x00;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_2] = 0x00;
    cmd_info.devno = 0;
    cmd_info.cmd_size = 3; // 3: command data size
    cmd_info.data_type = 0x29;
    cmd_info.cmd = g_mBuf;
    s32Ret = SAMPLE_USER_INIT_MIPITx(fd, &cmd_info);
    if (s32Ret != HI_SUCCESS) {
        return s32Ret;
    }
    usleep(USLEEP_TIME);
    usleep(10000); // 10000: The process hangs for a period of time, in microseconds

    memset_s(g_mBuf, G_MBUF_LENGTH, 0, G_MBUF_LENGTH);
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_0] = 0xFF;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_1] = 0x77;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_2] = 0x01;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_3] = 0x00;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_4] = 0x00;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_5] = 0x00;
    cmd_info.devno = 0;
    cmd_info.cmd_size = 6; // 6: command data size
    cmd_info.data_type = 0x29;
    cmd_info.cmd = g_mBuf;
    s32Ret = SAMPLE_USER_INIT_MIPITx(fd, &cmd_info);
    if (s32Ret != HI_SUCCESS) {
        return s32Ret;
    }
    usleep(USLEEP_TIME);
    usleep(10000); // 10000: The process hangs for a period of time, in microseconds

    cmd_info.devno = 0;
    cmd_info.cmd_size = 0x11;
    cmd_info.data_type = 0x05;
    cmd_info.cmd = NULL;
    s32Ret = SAMPLE_USER_INIT_MIPITx(fd, &cmd_info);
    if (s32Ret != HI_SUCCESS) {
        return s32Ret;
    }
    usleep(USLEEP_TIME);
    usleep(150000); // 150000: The process hangs for a period of time, in microseconds

    cmd_info.devno = 0;
    cmd_info.cmd_size = 0x29;
    cmd_info.data_type = 0x05;
    cmd_info.cmd = NULL;
    s32Ret = SAMPLE_USER_INIT_MIPITx(fd, &cmd_info);
    if (s32Ret != HI_SUCCESS) {
        return s32Ret;
    }
    usleep(USLEEP_TIME);
    usleep(50000); // 50000: The process hangs for a period of time, in microseconds

    return HI_SUCCESS;
}

/*
 * 启用MIPI Tx设备
 * Enable MIPI Tx device
 */
HI_S32 SAMPLE_VO_ENABLE_MIPITx(HI_S32 fd)
{
    HI_S32 s32Ret = ioctl(fd, HI_MIPI_TX_ENABLE);
    if (s32Ret != HI_SUCCESS) {
        printf("MIPI_TX enable failed\n");
        return s32Ret;
    }

    return s32Ret;
}

/*
 * 禁用MIPI Tx设备
 * Disable MIPI Tx device
 */
HI_S32 SAMPLE_VO_DISABLE_MIPITx(HI_S32 fd)
{
    HI_S32 s32Ret = ioctl(fd, HI_MIPI_TX_DISABLE);
    if (s32Ret != HI_SUCCESS) {
        printf("MIPI_TX disable failed\n");
        return s32Ret;
    }

    return s32Ret;
}

/*
 * 设置VO至MIPI通路，获取MIPI设备
 * Set VO config to MIPI, get MIPI device
 */
HI_S32 SAMPLE_VO_CONFIG_MIPI(HI_S32* mipiFD)
{
    HI_S32 s32Ret;
    HI_S32  fd;

    /*
     * 打开MIPI FD
     * Open MIPI FD
     */
    fd = SampleOpenMipiTxFd();
    if (fd < 0) {
        return HI_FAILURE;
    }
	*mipiFD = fd;

    /*
     * 设置MIPI Tx设备属性
     * Set MIPI Tx device attribution
     */
    s32Ret = SampleSetMipiTxDevAttr(fd);
    if (s32Ret != HI_SUCCESS) {
        return s32Ret;
    }

    usleep(10000); // 10000: The process hangs for a period of time, in microseconds
    system("cd /sys/class/gpio/;echo 5 > export;echo out > gpio5/direction;echo 1 > gpio5/value");
    usleep(200000); // 200000: The process hangs for a period of time, in microseconds
    system("echo 0 > /sys/class/gpio/gpio5/value");
    usleep(200000); // 200000: The process hangs for a period of time, in microseconds
    system("echo 1 > /sys/class/gpio/gpio5/value");
    usleep(20000); // 20000: The process hangs for a period of time, in microseconds

    /*
     * 配置MIPI Tx初始化序列
     * Config MIPI Tx initialization sequence
     */
    s32Ret = SampleVoInitMipiTxScreen(fd);
    if (s32Ret != HI_SUCCESS) {
        return s32Ret;
    }

    /*
     * 启用MIPI Tx设备
     * Enable MIPI Tx device
     */
    s32Ret = SAMPLE_VO_ENABLE_MIPITx(fd);
    if (s32Ret != HI_SUCCESS) {
        return s32Ret;
    }

    return s32Ret;
}

/*
 * 获得mipi设备的宽和高
 * Get mipi device Height and width
 */
HI_S32 SampleCommVoGetWhMipi(VO_INTF_SYNC_E enIntfSync, HI_U32* pu32W, HI_U32* pu32H, HI_U32* pu32Frm)
{
    switch (enIntfSync) {
        case VO_OUTPUT_1080P24:
            *pu32W = 1920; // 1920: VO_OUTPUT_1080P24-Width
            *pu32H = 1080; // 1080: VO_OUTPUT_1080P24-Height
            *pu32Frm = 24; // 24: VO_OUTPUT_1080P24-Frame rate
            break;
        case VO_OUTPUT_1080P25:
            *pu32W = 1920; // 1920: VO_OUTPUT_1080P25-Width
            *pu32H = 1080; // 1080: VO_OUTPUT_1080P25-Height
            *pu32Frm = 25; // 25: VO_OUTPUT_1080P25-Frame rate
            break;
        case VO_OUTPUT_1080P30:
            *pu32W = 1920; // 1920: VO_OUTPUT_1080P30-Width
            *pu32H = 1080; // 1080: VO_OUTPUT_1080P30-Height
            *pu32Frm = 30; // 30: VO_OUTPUT_1080P30-Frame rate
            break;
        case VO_OUTPUT_720P50:
            *pu32W = 1280; // 1280: VO_OUTPUT_720P50-Width
            *pu32H = 720; // 720: VO_OUTPUT_720P50-Height
            *pu32Frm = 50; // 50: VO_OUTPUT_720P50-Frame rate
            break;
        case VO_OUTPUT_720P60:
            *pu32W = 1280; // 1280: VO_OUTPUT_720P60-Width
            *pu32H = 720; // 720: VO_OUTPUT_720P60-Height
            *pu32Frm = 60; // 60: VO_OUTPUT_720P60-Frame rate
            break;
        case VO_OUTPUT_1080P50:
            *pu32W = 1920; // 1920: VO_OUTPUT_1080P50-Width
            *pu32H = 1080; // 1080: VO_OUTPUT_1080P50-Height
            *pu32Frm = 50; // 50: VO_OUTPUT_1080P50-Frame rate
            break;
        case VO_OUTPUT_1080P60:
            *pu32W = 1920; // 1920: VO_OUTPUT_1080P60-Width
            *pu32H = 1080; // 1080: VO_OUTPUT_1080P60-Height
            *pu32Frm = 60; // 60: VO_OUTPUT_1080P60-Frame rate
            break;
        case VO_OUTPUT_USER:
            *pu32W = 800; // 800: VO_OUTPUT_USER-Width
            *pu32H = 480; // 480: VO_OUTPUT_USER-Height
            *pu32Frm = 20; // 60: VO_OUTPUT_USER-Frame rate
            break;
        default:
            SAMPLE_PRT("vo enIntfSync %d not support, please config self!\n", enIntfSync);
            return HI_FAILURE;
    }

    return HI_SUCCESS;
}

HI_S32 SampleCommVoStartDevMipi(VO_DEV VoDev, VO_PUB_ATTR_S* pstPubAttr)
{
    HI_S32 s32Ret;
    VO_USER_INTFSYNC_INFO_S stUserInfo = {0};

    stUserInfo.bClkReverse = HI_TRUE;
    stUserInfo.u32DevDiv = 1;
    stUserInfo.u32PreDiv = 1;
    stUserInfo.stUserIntfSyncAttr.enClkSource = VO_CLK_SOURCE_PLL;
    stUserInfo.stUserIntfSyncAttr.stUserSyncPll.u32Fbdiv = 244; // 244: PLL integer frequency multiplier coefficient
    stUserInfo.stUserIntfSyncAttr.stUserSyncPll.u32Frac = 0x1A36;
    stUserInfo.stUserIntfSyncAttr.stUserSyncPll.u32Refdiv = 4; // 4: PLL reference clock frequency division coefficient
    // 7: PLL first stage output frequency division coefficient
    stUserInfo.stUserIntfSyncAttr.stUserSyncPll.u32Postdiv1 = 7;
    // 7: PLL second stage output frequency division coefficient
    stUserInfo.stUserIntfSyncAttr.stUserSyncPll.u32Postdiv2 = 7;
    HI_U32 u32Framerate = 20; // 60: device frame rate

    /*
     * 配置视频输出设备的公共属性
     * Set the common properties of the video output device
     */
    s32Ret = HI_MPI_VO_SetPubAttr(VoDev, pstPubAttr);
    if (s32Ret != HI_SUCCESS) {
        SAMPLE_PRT("failed with %#x!\n", s32Ret);
        return HI_FAILURE;
    }

    /*
     * 设置设备用户时序下设备帧率
     * Set the device frame rate under the device user timing
     */
    s32Ret = HI_MPI_VO_SetDevFrameRate(VoDev, u32Framerate);
    if (s32Ret != HI_SUCCESS) {
        SAMPLE_PRT("failed with %#x!\n", s32Ret);
        return HI_FAILURE;
    }

    /*
     * 设置用户接口时序信息，用于配置时钟源、时钟大小和时钟分频比
     *
     * Set user interface timing information, used to configure clock source,
     * clock size and clock frequency division ratio
     */
    s32Ret = HI_MPI_VO_SetUserIntfSyncInfo(VoDev, &stUserInfo);
    if (s32Ret != HI_SUCCESS) {
        SAMPLE_PRT("failed with %#x!\n", s32Ret);
        return HI_FAILURE;
    }

    /*
     * 启用视频输出设备
     * Enable video output device
     */
    s32Ret = HI_MPI_VO_Enable(VoDev);
    if (s32Ret != HI_SUCCESS) {
        SAMPLE_PRT("failed with %#x!\n", s32Ret);
        return HI_FAILURE;
    }

    return s32Ret;
}

HI_S32 SampleCommVoStartChnModeMux(SAMPLE_VO_MODE_E enMode)
{
    int s32Ret;
    switch (enMode) {
        case VO_MODE_1MUX:
            g_sampleVoModeMux.u32WndNum = 1;
            g_sampleVoModeMux.u32Square = 1;
            break;
        case VO_MODE_2MUX:
            g_sampleVoModeMux.u32WndNum = 2; // 2: 2MUX-WndNum
            g_sampleVoModeMux.u32Square = 2; // 2: 2MUX-Square
            break;
        case VO_MODE_4MUX:
            g_sampleVoModeMux.u32WndNum = 4; // 4: 4MUX-WndNum
            g_sampleVoModeMux.u32Square = 2; // 2: 4MUX-Square
            break;
        case VO_MODE_8MUX:
            g_sampleVoModeMux.u32WndNum = 8; // 8: 8MUX-WndNum
            g_sampleVoModeMux.u32Square = 3; // 3: 8MUX-Square
            break;
        case VO_MODE_9MUX:
            g_sampleVoModeMux.u32WndNum = 9; // 9: 9MUX-WndNum
            g_sampleVoModeMux.u32Square = 3; // 3: 9MUX-Square
            break;
        case VO_MODE_16MUX:
            g_sampleVoModeMux.u32WndNum = 16; // 16: 16MUX-WndNum
            g_sampleVoModeMux.u32Square = 4; // 4: 16MUX-Square
            break;
        case VO_MODE_25MUX:
            g_sampleVoModeMux.u32WndNum = 25; // 25: 25MUX-WndNum
            g_sampleVoModeMux.u32Square = 5; // 5: 25MUX-Square
            break;
        case VO_MODE_36MUX:
            g_sampleVoModeMux.u32WndNum = 36; // 36: 36MUX-WndNum
            g_sampleVoModeMux.u32Square = 6; // 6: 36MUX-Square
            break;
        case VO_MODE_49MUX:
            g_sampleVoModeMux.u32WndNum = 49; // 49: 49MUX-WndNum
            g_sampleVoModeMux.u32Square = 7; // 7: 49MUX-Square
            break;
        case VO_MODE_2X4:
            g_sampleVoModeMux.u32WndNum = 8; // 8: 2X4-WndNum
            g_sampleVoModeMux.u32Square = 3; // 3: 2X4-Square
            g_sampleVoModeMux.u32Row    = 4; // 4: 2X4-Row
            g_sampleVoModeMux.u32Col    = 2; // 2: 2X4-Col
            break;
        default:
            SAMPLE_PRT("failed with %#x!\n", s32Ret);
            return HI_FAILURE;
    }

    return HI_SUCCESS;
}

HI_S32 SampleCommVoStartChnMipi(VO_LAYER VoLayer, SAMPLE_VO_MODE_E enMode)
{
    HI_S32 i;
    HI_S32 s32Ret    = HI_SUCCESS;
    HI_U32 u32Width  = 0;
    HI_U32 u32Height = 0;
    VO_CHN_ATTR_S         stChnAttr;
    VO_VIDEO_LAYER_ATTR_S stLayerAttr;

    s32Ret = SampleCommVoStartChnModeMux(enMode);
    SAMPLE_CHECK_EXPR_RET(s32Ret != HI_SUCCESS, s32Ret, "for SampleCommVoStartChnModeMux FAIL, s32Ret=%x\n", s32Ret);

    /*
     * 获取视频层属性
     * Get video layer properties
     */
    s32Ret = HI_MPI_VO_GetVideoLayerAttr(VoLayer, &stLayerAttr);
    SAMPLE_CHECK_EXPR_RET(s32Ret != HI_SUCCESS, s32Ret, "for HI_MPI_VO_GetVideoLayerAttr FAIL, s32Ret=%x\n", s32Ret);
    u32Width  = stLayerAttr.stImageSize.u32Width;
    u32Height = stLayerAttr.stImageSize.u32Height;
    SAMPLE_PRT("enMode:%d, u32Width:%d, u32Height:%d, u32Square:%d\n", enMode,
        u32Width, u32Height, g_sampleVoModeMux.u32Square);

    for (i = 0; i < g_sampleVoModeMux.u32WndNum; i++) {
        if (enMode == VO_MODE_1MUX  || enMode == VO_MODE_2MUX  || enMode == VO_MODE_4MUX  ||
            enMode == VO_MODE_8MUX  || enMode == VO_MODE_9MUX  || enMode == VO_MODE_16MUX ||
            enMode == VO_MODE_25MUX || enMode == VO_MODE_36MUX || enMode == VO_MODE_49MUX) {
            stChnAttr.stRect.s32X       = HI_ALIGN_DOWN((u32Width / g_sampleVoModeMux.u32Square) *
                (i % g_sampleVoModeMux.u32Square), ALIGN_DOWN_SIZE);
            stChnAttr.stRect.s32Y       = HI_ALIGN_DOWN((u32Height / g_sampleVoModeMux.u32Square) *
                (i / g_sampleVoModeMux.u32Square), ALIGN_DOWN_SIZE);
            stChnAttr.stRect.u32Width   = HI_ALIGN_DOWN(u32Width / g_sampleVoModeMux.u32Square, ALIGN_DOWN_SIZE);
            stChnAttr.stRect.u32Height  = HI_ALIGN_DOWN(u32Height / g_sampleVoModeMux.u32Square, ALIGN_DOWN_SIZE);
            stChnAttr.u32Priority       = 0;
            stChnAttr.bDeflicker        = HI_FALSE;
        } else if (enMode == VO_MODE_2X4) {
            stChnAttr.stRect.s32X       = HI_ALIGN_DOWN((u32Width / g_sampleVoModeMux.u32Col) *
                (i % g_sampleVoModeMux.u32Col), ALIGN_DOWN_SIZE);
            stChnAttr.stRect.s32Y       = HI_ALIGN_DOWN((u32Height / g_sampleVoModeMux.u32Row) *
                (i / g_sampleVoModeMux.u32Col), ALIGN_DOWN_SIZE);
            stChnAttr.stRect.u32Width   = HI_ALIGN_DOWN(u32Width / g_sampleVoModeMux.u32Col, ALIGN_DOWN_SIZE);
            stChnAttr.stRect.u32Height  = HI_ALIGN_DOWN(u32Height / g_sampleVoModeMux.u32Row, ALIGN_DOWN_SIZE);
            stChnAttr.u32Priority       = 0;
            stChnAttr.bDeflicker        = HI_FALSE;
        }

        /*
         * 配置指定视频输出通道的属性
         * Set properties for the specified video output channel
         */
        s32Ret = HI_MPI_VO_SetChnAttr(VoLayer, i, &stChnAttr);
        SAMPLE_CHECK_EXPR_RET(s32Ret != HI_SUCCESS, s32Ret, "for HI_MPI_VO_SetChnAttr FAIL, s32Ret=%x\n", s32Ret);

        /*
         * 设置指定视频输出通道的旋转角度
         * Set video output channel rotation angle
         */
        s32Ret = HI_MPI_VO_SetChnRotation(VoLayer, i, ROTATION_90);
        SAMPLE_CHECK_EXPR_RET(s32Ret != HI_SUCCESS, s32Ret, "for HI_MPI_VO_SetChnRotation FAIL, s32Ret=%x\n", s32Ret);

        /*
         * 启用指定的视频输出通道
         * Enables the specified video output channel
         */
        s32Ret = HI_MPI_VO_EnableChn(VoLayer, i);
        SAMPLE_CHECK_EXPR_RET(s32Ret != HI_SUCCESS, s32Ret, "for HI_MPI_VO_EnableChn FAIL, s32Ret=%x\n", s32Ret);
    }

    return HI_SUCCESS;
}

static HI_VOID StVoPubAttrCfg(SAMPLE_VO_CONFIG_S *pstVoConfig)
{
    HI_ASSERT(pstVoConfig);
    /*
     * 定义视频输出公共属性结构体
     * Define the video output public attribute structure
     */
    stVoPubAttr.enIntfType  = VO_INTF_MIPI;
    stVoPubAttr.enIntfSync  = VO_OUTPUT_USER;
    stVoPubAttr.stSyncInfo.bSynm = 0;
    stVoPubAttr.stSyncInfo.bIop = 1;
    stVoPubAttr.stSyncInfo.u8Intfb = 0;

    stVoPubAttr.stSyncInfo.u16Hmid = 1;
    stVoPubAttr.stSyncInfo.u16Bvact = 1;
    stVoPubAttr.stSyncInfo.u16Bvbb = 1;
    stVoPubAttr.stSyncInfo.u16Bvfb = 1;

    stVoPubAttr.stSyncInfo.bIdv = 0;
    stVoPubAttr.stSyncInfo.bIhs = 0;
    stVoPubAttr.stSyncInfo.bIvs = 0;

    stVoPubAttr.stSyncInfo.u16Hact = 480; // 480: Horizontal effective area. Unit: pixel
    stVoPubAttr.stSyncInfo.u16Hbb = 60; // 60: Horizontal blanking of the rear shoulder. Unit: pixel
    stVoPubAttr.stSyncInfo.u16Hfb = 50; // 50: Horizontal blanking of the front shoulder. Unit: pixel
    stVoPubAttr.stSyncInfo.u16Hpw = 10; // 10: The width of the horizontal sync signal. Unit: pixel
    stVoPubAttr.stSyncInfo.u16Vact = 800; // 800: Vertical effective area. Unit: line
    stVoPubAttr.stSyncInfo.u16Vbb = 24; // 24: Vertical blanking of the rear shoulder.  Unit: line
    stVoPubAttr.stSyncInfo.u16Vfb = 20; // 20: Vertical blanking of the front shoulder.  Unit: line
    stVoPubAttr.stSyncInfo.u16Vpw = 4; // 4: The width of the vertical sync signal. Unit: line
    stVoPubAttr.u32BgColor  = pstVoConfig->u32BgColor;
}

static HI_VOID StLayerAttrCfg(SAMPLE_VO_CONFIG_S *pstVoConfig)
{
    HI_ASSERT(pstVoConfig);
    stLayerAttr.bClusterMode     = HI_FALSE;
    stLayerAttr.bDoubleFrame    = HI_FALSE;
    stLayerAttr.enPixFormat       = pstVoConfig->enPixFormat;

    stLayerAttr.stDispRect.s32X = 0;
    stLayerAttr.stDispRect.s32Y = 0;
    stLayerAttr.enDstDynamicRange     = pstVoConfig->enDstDynamicRange;
}

/*
 * 启动VO到MIPI lcd通路
 * Start VO to MIPI lcd
 */
HI_S32 SampleCommVoStartMipi(SAMPLE_VO_CONFIG_S *pstVoConfig)
{
    HI_S32 s32Ret;

    HI_ASSERT(pstVoConfig);
    StVoPubAttrCfg(pstVoConfig);
    s32Ret = SampleCommVoStartDevMipi(pstVoConfig->VoDev, &stVoPubAttr);
    SAMPLE_CHECK_EXPR_RET(s32Ret != HI_SUCCESS, s32Ret, "StartDevMipi FAIL, ret=%x\n", s32Ret);
    /*
     * 获得MIPI设备的宽和高
     * Get MIPI device Height and width
     */
    s32Ret = SampleCommVoGetWhMipi(stVoPubAttr.enIntfSync,
        &stLayerAttr.stDispRect.u32Width, &stLayerAttr.stDispRect.u32Height, &stLayerAttr.u32DispFrmRt);
    SAMPLE_CHECK_EXPR_GOTO(s32Ret != HI_SUCCESS, FAIL, "VoGetWhMipi fail, err(%#x)\n", s32Ret);

    StLayerAttrCfg(pstVoConfig);

    /*
     * 如果存在变化，设置显示矩形框
     * Set display rectangle if changed
     */
    if (memcmp(&pstVoConfig->stDispRect, &stDefDispRect, sizeof(RECT_S)) != 0) {
        memcpy_s(&stLayerAttr.stDispRect, sizeof(stLayerAttr.stDispRect),
            &pstVoConfig->stDispRect, sizeof(RECT_S));
    }

    /*
     * 如果存在变化，设置图片大小
     * Set image size if changed
     */
    if (memcmp(&pstVoConfig->stImageSize, &stDefImageSize, sizeof(SIZE_S)) != 0) {
        memcpy_s(&stLayerAttr.stImageSize, sizeof(stLayerAttr.stImageSize),
            &pstVoConfig->stImageSize, sizeof(SIZE_S));
    }
    stLayerAttr.stImageSize.u32Width  = stLayerAttr.stDispRect.u32Width = 480; // 480: video layer canvas Width
    stLayerAttr.stImageSize.u32Height = stLayerAttr.stDispRect.u32Height = 800; // 800: video layer canvas Height

    if (pstVoConfig->u32DisBufLen) {
        /*
         * 设置显示缓冲的长度
         * Set buffer length
         */
        s32Ret = HI_MPI_VO_SetDisplayBufLen(pstVoConfig->VoDev, pstVoConfig->u32DisBufLen);
        SAMPLE_CHECK_EXPR_GOTO(s32Ret != HI_SUCCESS, FAIL, "HI_MPI_VO_SetDisplayBufLen fail, err(%#x)\n", s32Ret);
    }
    if (VO_PART_MODE_MULTI == pstVoConfig->enVoPartMode) {
        /*
         * 设置视频层的分割模式
         * Set the segmentation mode of the video layer
         */
        s32Ret = HI_MPI_VO_SetVideoLayerPartitionMode(pstVoConfig->VoDev, pstVoConfig->enVoPartMode);
        SAMPLE_CHECK_EXPR_GOTO(s32Ret != HI_SUCCESS, FAIL, "SetVideoLayerMode fail, err(%#x)\n", s32Ret);
    }

    s32Ret = SAMPLE_COMM_VO_StartLayer(pstVoConfig->VoDev, &stLayerAttr); // start layer
    SAMPLE_CHECK_EXPR_GOTO(s32Ret != HI_SUCCESS, FAIL, "VO_StartLayer fail, err(%#x)\n", s32Ret);

    if (VO_INTF_MIPI == pstVoConfig->enVoIntfType) {
        s32Ret = HI_MPI_VO_GetVideoLayerCSC(pstVoConfig->VoDev, &stVideoCSC); // get video layerCSC
        SAMPLE_CHECK_EXPR_GOTO(s32Ret != HI_SUCCESS, FAIL, "GetVideoLayerCSC fail, err(%#x)\n", s32Ret);
        stVideoCSC.enCscMatrix = VO_CSC_MATRIX_BT709_TO_RGB_PC;
        s32Ret = HI_MPI_VO_SetVideoLayerCSC(pstVoConfig->VoDev, &stVideoCSC); // Set video layer CSC
        SAMPLE_CHECK_EXPR_GOTO(s32Ret != HI_SUCCESS, FAIL, "SetVideoLayerCSC fail, err(%#x)\n", s32Ret);
    }

    s32Ret = SampleCommVoStartChnMipi(pstVoConfig->VoDev, pstVoConfig->enVoMode); // start vo channels
    SAMPLE_CHECK_EXPR_GOTO(s32Ret != HI_SUCCESS, FAIL1, "VoStartChnMipi fail, err(%#x)\n", s32Ret);
    return HI_SUCCESS;

FAIL1:
    SAMPLE_COMM_VO_StopLayer(pstVoConfig->VoDev);
FAIL:
    SAMPLE_COMM_VO_StopDev(pstVoConfig->VoDev);
    return s32Ret;
}

/*
 * 初始化vi配置
 * Init ViCfg
 */
void ViCfgInit(ViCfg* self)
{
    HI_ASSERT(self);
    if (memset_s(self, sizeof(*self), 0, sizeof(*self)) != EOK) {
        HI_ASSERT(0);
    }

    SAMPLE_COMM_VI_GetSensorInfo(self);
    self->s32WorkingViNum = 1;
    self->as32WorkingViId[0] = 0;

    self->astViInfo[0].stSnsInfo.MipiDev =
        SAMPLE_COMM_VI_GetComboDevBySensor(self->astViInfo[0].stSnsInfo.enSnsType, 0);
    self->astViInfo[0].stSnsInfo.s32BusId = 0;
}

/*
 * 设置VI设备信息
 * Set VI device information
 */
void ViCfgSetDev(ViCfg* self, int devId, WDR_MODE_E wdrMode)
{
    HI_ASSERT(self);
    HI_ASSERT((int)wdrMode < WDR_MODE_BUTT);

    self->astViInfo[0].stDevInfo.ViDev = devId;
    self->astViInfo[0].stDevInfo.enWDRMode = (int)wdrMode < 0 ? WDR_MODE_NONE : wdrMode;
}

/*
 * 设置VI的PIPE信息
 * Set the PIPE information of the VI
 */
void ViCfgSetPipe(ViCfg* self, int pipe0Id, int pipe1Id, int pipe2Id, int pipe3Id)
{
    HI_ASSERT(self);

    self->astViInfo[0].stPipeInfo.aPipe[APIPE0] = pipe0Id;
    self->astViInfo[0].stPipeInfo.aPipe[APIPE1] = pipe1Id;
    self->astViInfo[0].stPipeInfo.aPipe[APIPE2] = pipe2Id;
    self->astViInfo[0].stPipeInfo.aPipe[APIPE3] = pipe3Id;
}

/*
 * 设置VI通道
 * Set up the VI channel
 */
void ViCfgSetChn(ViCfg* self, int chnId, PIXEL_FORMAT_E pixFormat,
    VIDEO_FORMAT_E videoFormat, DYNAMIC_RANGE_E dynamicRange)
{
    HI_ASSERT(self);
    HI_ASSERT((int)pixFormat < PIXEL_FORMAT_BUTT);
    HI_ASSERT((int)videoFormat < VIDEO_FORMAT_BUTT);
    HI_ASSERT((int)dynamicRange < DYNAMIC_RANGE_BUTT);

    self->astViInfo[0].stChnInfo.ViChn = chnId;
    self->astViInfo[0].stChnInfo.enPixFormat =
        (int)pixFormat < 0 ? PIXEL_FORMAT_YVU_SEMIPLANAR_420 : pixFormat;
    self->astViInfo[0].stChnInfo.enVideoFormat =
        (int)videoFormat < 0 ? VIDEO_FORMAT_LINEAR : videoFormat;
    self->astViInfo[0].stChnInfo.enDynamicRange =
        (int)dynamicRange < 0 ? DYNAMIC_RANGE_SDR8 : dynamicRange;
}

/*
 * 初始化VPSS配置
 * Init VpssCfg
 */
void VpssCfgInit(VpssCfg* self)
{
    HI_ASSERT(self);
    if (memset_s(self, sizeof(*self), 0, sizeof(*self)) != EOK) {
        HI_ASSERT(0);
    }
    self->grpId = -1;
    self->chnNum = 0;
}

/*
 * 设置VPSS组
 * Set up VPSS Group
 */
void VpssCfgSetGrp(VpssCfg* self,
    int grpId, const VPSS_GRP_ATTR_S* grpAttr, int maxWidth, int maxHeight)
{
    HI_ASSERT(self);
    HI_ASSERT(grpId >= 0);

    self->grpId = grpId;

    if (grpAttr) {
        self->grpAttr = *grpAttr;
    } else { // Set as default
        self->grpAttr.u32MaxW = maxWidth < 0 ? 0 : maxWidth;
        self->grpAttr.u32MaxH = maxHeight < 0 ? 0 : maxHeight;
        self->grpAttr.enPixelFormat = PIXEL_FORMAT_YVU_SEMIPLANAR_420;
        self->grpAttr.enDynamicRange = DYNAMIC_RANGE_SDR8;
        self->grpAttr.stFrameRate.s32SrcFrameRate = -1;
        self->grpAttr.stFrameRate.s32DstFrameRate = -1;
        self->grpAttr.bNrEn = HI_TRUE;
    }
}

/*
 * 增加一个VPSS通道
 * Add a VPSS channel
 */
VPSS_CHN_ATTR_S* VpssCfgAddChn(VpssCfg* self,
    int chnId, const VPSS_CHN_ATTR_S* chnAttr, int width, int height)
{
    HI_ASSERT(self);
    HI_ASSERT(self->chnNum < (sizeof(self->chnCfgs) / sizeof((self->chnCfgs)[0])));
    static const uint32_t depthDef = 2;
    VpssChnCfg *chnCfg = &self->chnCfgs[self->chnNum];

    if (chnAttr) {
        chnCfg->attr = *chnAttr;
    } else { // Set as default
        chnCfg->attr.u32Width = width < 0 ? 0 : width;
        chnCfg->attr.u32Height = height < 0 ? 0 : height;
        chnCfg->attr.enChnMode = VPSS_CHN_MODE_USER;
        chnCfg->attr.enVideoFormat = VIDEO_FORMAT_LINEAR;
        chnCfg->attr.enPixelFormat = PIXEL_FORMAT_YVU_SEMIPLANAR_420;
        chnCfg->attr.enDynamicRange = DYNAMIC_RANGE_SDR8;
        chnCfg->attr.enCompressMode = COMPRESS_MODE_NONE;
        chnCfg->attr.stFrameRate.s32SrcFrameRate = -1;
        chnCfg->attr.stFrameRate.s32DstFrameRate = -1;
        chnCfg->attr.u32Depth = depthDef;
        chnCfg->attr.bMirror = HI_FALSE;
        chnCfg->attr.bFlip = HI_FALSE;
        chnCfg->attr.stAspectRatio.enMode = ASPECT_RATIO_NONE;
    }
    chnCfg->id = chnId;

    self->chnNum++;
    return &chnCfg->attr;
}

/*
 * 根据VPSS配置来启动VPSS
 * Start VPSS according to VPSS config
 */
int VpssStart(const VpssCfg* cfg)
{
    HI_ASSERT(cfg);
    VPSS_GRP grpId = cfg->grpId;
    HI_S32 ret;

    /*
     * 创建一个VPSS GROUP
     * Create a VPSS GROUP
     */
    ret = HI_MPI_VPSS_CreateGrp(grpId, &cfg->grpAttr);
    if (ret != 0) {
        SAMPLE_PRT("HI_MPI_VPSS_CreateGrp(%d) FAIL, ret=%#x\n", grpId, ret);
        return ret;
    }

    for (int i = 0; i < cfg->chnNum; i++) {
        /*
         * 设置VPSS通道属性
         * Set VPSS channel properties
         */
        ret = HI_MPI_VPSS_SetChnAttr(grpId, cfg->chnCfgs[i].id, &cfg->chnCfgs[i].attr);
        if (ret != 0) {
            SAMPLE_PRT("HI_MPI_VPSS_SetChnAttr(%d) FAIL, ret=%#x\n", cfg->chnCfgs[i].id, ret);
            return ret;
        }

        /*
         * 启用VPSS通道
         * Enable VPSS channel
         */
        ret = HI_MPI_VPSS_EnableChn(grpId, cfg->chnCfgs[i].id);
        if (ret != 0) {
            SAMPLE_PRT("HI_MPI_VPSS_EnableChn(%d) FAIL, ret=%#x\n", cfg->chnCfgs[i].id, ret);
            return ret;
        }
    }

    /*
     * 启用VPSS GROUP
     * Enable VPSS GROUP
     */
    ret = HI_MPI_VPSS_StartGrp(grpId);
    if (ret != 0) {
        SAMPLE_PRT("HI_MPI_VPSS_StartGrp(%d) FAIL, ret=%#x\n", grpId, ret);
        return ret;
    }
    return HI_SUCCESS;
}

/*Terminate VI started with ViCfg*/
int ViStop(const ViCfg* viCfg)
{
    return SAMPLE_COMM_VI_StopVi((ViCfg*)viCfg);
}

/*Terminate VPSS started with VpssCfg*/
int VpssStop(const VpssCfg* cfg)
{
    HI_ASSERT(cfg);
    VPSS_GRP grpId = cfg->grpId;
    HI_S32 ret;

    for (int i = 0; i < cfg->chnNum; i++) {
        /*Disable VPSS channel*/
        ret = HI_MPI_VPSS_DisableChn(grpId, cfg->chnCfgs[i].id);
        if (ret != 0) {
            SAMPLE_PRT("HI_MPI_VPSS_DisableChn(%d, %d) FAIL, ret=%#x\n", grpId, cfg->chnCfgs[i].id, ret);
            return ret;
        }
    }

    /*Stop VPSS GROUP*/
    ret = HI_MPI_VPSS_StopGrp(grpId);
    if (ret != 0) {
        SAMPLE_PRT("HI_MPI_VPSS_StopGrp(%d) FAIL, ret=%#x\n", grpId, ret);
        return ret;
    }

    /*Destroy a VPSS GROUP*/
    ret = HI_MPI_VPSS_DestroyGrp(grpId);
    if (ret != 0) {
        SAMPLE_PRT("HI_MPI_VPSS_DestroyGrp(%d) FAIL, ret=%#x\n", grpId, ret);
        return ret;
    }
    return HI_SUCCESS;
}

/*Start VI according to ViCfg*/
int ViStart(const ViCfg* viCfg)
{
    static const uint32_t frmRateDef = 30;
    SAMPLE_SNS_TYPE_E snsType = viCfg->astViInfo[0].stSnsInfo.enSnsType;
    ISP_CTRL_PARAM_S ispCtrlParam;
    HI_U32 frmRate;
    HI_S32 ret;

    ret = SAMPLE_COMM_VI_SetParam((ViCfg*)viCfg);
    if (ret != 0) {
        SAMPLE_PRT("SAMPLE_COMM_VI_SetParam FAIL, ret=%#x\n", ret);
        return ret;
    }

    /*Get the frame rate through the Sensor model*/
    SAMPLE_COMM_VI_GetFrameRateBySensor(snsType, &frmRate);
    ret = HI_MPI_ISP_GetCtrlParam(viCfg->astViInfo[0].stPipeInfo.aPipe[0], &ispCtrlParam);
    if (ret != 0) {
        SAMPLE_PRT("HI_MPI_ISP_GetCtrlParam FAIL, ret=%#x\n", ret);
        return ret;
    }

    ispCtrlParam.u32StatIntvl = frmRate / frmRateDef;
    ret = HI_MPI_ISP_SetCtrlParam(viCfg->astViInfo[0].stPipeInfo.aPipe[0], &ispCtrlParam);
    if (ret != 0) {
        SAMPLE_PRT("HI_MPI_ISP_SetCtrlParam FAIL, ret=%#x\n", ret);
        return ret;
    }

    ret = SAMPLE_COMM_VI_StartVi((ViCfg*)viCfg);
    if (ret != 0) {
        SAMPLE_PRT("SAMPLE_COMM_VI_StartVi FAIL, ret=%#x\n", ret);
        return ret;
    }

    return HI_SUCCESS;
}

/*Bind VI to VPSS*/
int ViBindVpss(VI_PIPE viPipe, VI_CHN viChn, VPSS_GRP vpssGrp)
{
    MPP_CHN_S srcChn;
    MPP_CHN_S dstChn;

    srcChn.enModId = HI_ID_VI;
    srcChn.s32DevId = viPipe;
    srcChn.s32ChnId = viChn;

    dstChn.enModId = HI_ID_VPSS;
    dstChn.s32DevId = vpssGrp;
    dstChn.s32ChnId = 0;

    int ret = HI_MPI_SYS_Bind(&srcChn, &dstChn);
    if (ret != 0) {
        SAMPLE_PRT("HI_MPI_SYS_Bind(VI:%d, VPSS:%d) FAIL, ret=%#x\n", viChn, vpssGrp, ret);
        return ret;
    }

    return ret;
}

/*New MppSess*/
static MppSess* MppSessNew(void)
{
    MppSess *sess = (MppSess*)malloc(sizeof(*sess));
    if (sess == NULL) {
        SAMPLE_PRT("%s: malloc failed!\n", __FUNCTION__);
        HI_ASSERT(sess);
    }

    if (memset_s(sess, sizeof(*sess), 0, sizeof(*sess)) != EOK) {
        HI_ASSERT(0);
    }

    sess->vpssGrp = -1;
    sess->vpssChn0 = -1;
    sess->vpssChn1 = -1;

    return sess;
}

/*Create and start {VI->VPSS}MppSess*/
int ViVpssCreate(MppSess** sess, const ViCfg* viCfg, const VpssCfg* vpssCfg)
{
    HI_ASSERT(sess && viCfg && vpssCfg);
    *sess = NULL;
    int res;
    int ret;

    ret = ViStart(viCfg);
    SAMPLE_CHECK_EXPR_GOTO(ret != HI_SUCCESS, FAIL1,
        "vi start fail, err(%#x)\n", ret);

    ret = VpssStart(vpssCfg);
    SAMPLE_CHECK_EXPR_GOTO(ret != HI_SUCCESS, FAIL2,
        "vpss start fail, err(%#x)\n", ret);

    VI_PIPE pipeId = viCfg->astViInfo[0].stPipeInfo.aPipe[0];
    VI_CHN chnId = viCfg->astViInfo[0].stChnInfo.ViChn;
    ret = ViBindVpss(pipeId, chnId, vpssCfg->grpId);
    SAMPLE_CHECK_EXPR_GOTO(ret != HI_SUCCESS, FAIL3,
        "vi bind vpss fail, err(%#x)\n", ret);

    MppSess *self = MppSessNew(); // todo:realease malloc
    *sess = self;
    self->viCfg = *viCfg;
    self->vpssCfg = *vpssCfg;
    self->used |= MPP_VI;
    self->used |= MPP_VPSS;
    self->vpssGrp = vpssCfg->grpId;
    self->vpssChn0 = vpssCfg->chnCfgs[0].id;
    self->vpssChn1 = vpssCfg->chnNum > 1 ? vpssCfg->chnCfgs[1].id : -1;
    return 0;

    FAIL3:
        res = VpssStop(vpssCfg);
        SAMPLE_PRT("ViVpssCreate\n");
        if (res != 0) {
            SAMPLE_PRT("VpssStop FAIL, ret=%#x\n", res);
        }
    FAIL2:
        ViStop(viCfg);
    FAIL1:
        return ret;
}


static HI_VOID Pause(HI_VOID)
{
    printf("---------------press Enter key to exit!---------------\n");
    (void)getchar();
}

HI_VOID ViPramCfg(HI_VOID)
{
    ViCfgInit(&aicMediaInfo.viCfg);
    ViCfgSetDev(&aicMediaInfo.viCfg, 0, -1);
    ViCfgSetPipe(&aicMediaInfo.viCfg, 0, -1, -1, -1);
    aicMediaInfo.viCfg.astViInfo[0].stPipeInfo.enMastPipeMode = 0;
    ViCfgSetChn(&aicMediaInfo.viCfg, 0, -1, -1, -1);
    aicMediaInfo.viCfg.astViInfo[0].stChnInfo.enCompressMode = 1;
}

static HI_VOID StVbParamCfg(VbCfg *self)
{
    memset_s(&aicMediaInfo.vbCfg, sizeof(VB_CONFIG_S), 0, sizeof(VB_CONFIG_S));
    // 2: The number of buffer pools that can be accommodated in the entire system
    self->u32MaxPoolCnt              = 2;

    /*Get picture buffer size*/
    aicMediaInfo.u32BlkSize = COMMON_GetPicBufferSize(aicMediaInfo.stSize.u32Width, aicMediaInfo.stSize.u32Height,
        SAMPLE_PIXEL_FORMAT, DATA_BITWIDTH_8, COMPRESS_MODE_SEG, DEFAULT_ALIGN);
    self->astCommPool[0].u64BlkSize  = aicMediaInfo.u32BlkSize;
    // 10: Number of cache blocks per cache pool. Value range: (0, 10240]
    self->astCommPool[0].u32BlkCnt   = 10;

    /*Get raw buffer size*/
    aicMediaInfo.u32BlkSize = VI_GetRawBufferSize(aicMediaInfo.stSize.u32Width, aicMediaInfo.stSize.u32Height,
        PIXEL_FORMAT_RGB_BAYER_16BPP, COMPRESS_MODE_NONE, DEFAULT_ALIGN);
    self->astCommPool[1].u64BlkSize  = aicMediaInfo.u32BlkSize;
    // 4: Number of cache blocks per cache pool. Value range: (0, 10240]
    self->astCommPool[1].u32BlkCnt   = 4;
}

static HI_VOID StVoParamCfg(VoCfg *self)
{
    SAMPLE_COMM_VO_GetDefConfig(self);
    self->enDstDynamicRange = DYNAMIC_RANGE_SDR8;

    self->enVoIntfType = VO_INTF_MIPI; /* set VO int type */
    self->enIntfSync = VO_OUTPUT_USER; /* set VO output information */

    self->enPicSize = aicMediaInfo.enPicSize;
}

static HI_VOID VpssParamCfg(HI_VOID)
{
    VpssCfgInit(&aicMediaInfo.vpssCfg);
    VpssCfgSetGrp(&aicMediaInfo.vpssCfg, AIC_VPSS_GRP, NULL,
        aicMediaInfo.stSize.u32Width, aicMediaInfo.stSize.u32Width);
    aicMediaInfo.vpssCfg.grpAttr.enPixelFormat = PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    VpssCfgAddChn(&aicMediaInfo.vpssCfg, AIC_VPSS_ZOUT_CHN, NULL, AICSTART_VI_OUTWIDTH, AICSTART_VI_OUTHEIGHT);
    HI_ASSERT(!aicMediaInfo.viSess);
}


static HI_VOID HandDetectAiProcess(VIDEO_FRAME_INFO_S frm, VO_LAYER voLayer, VO_CHN voChn)
{
    int ret = 0;
    VIDEO_FRAME_INFO_S resizeFrm;

    ret = MppFrmResize(&frm, &resizeFrm, OBSTACLE_FRM_WIDTH, OBSTACLE_FRM_HEIGHT);  //vgs
    if(ret < 0)
    {
        printf("MppFrmResize error.\n");
    }

    ret = Yolo2HandDetectResnetClassifyCal(AiPlug.model, &resizeFrm, &frm);
    SAMPLE_CHECK_EXPR_GOTO(ret < 0, RELEASE, "obstacle detect plug cal FAIL, ret=%#x\n", ret);

#if DEBUGMODE == 1
    if(ret > 0)
    {
        ret = HI_MPI_VO_SendFrame(voLayer, voChn, &frm, 0);
        SAMPLE_CHECK_EXPR_GOTO(ret != HI_SUCCESS, RELEASE, "HI_MPI_VO_SendFrame fail, Error(%#x)\n", ret);
    }
#endif
    
    MppFrmDestroy(&resizeFrm);

RELEASE:
    MppFrmDestroy(&resizeFrm);
    ret = HI_MPI_VPSS_ReleaseChnFrame(aicMediaInfo.vpssGrp, aicMediaInfo.vpssChn0, &frm);
    if (ret != HI_SUCCESS) {
        SAMPLE_PRT("Error(%#x),HI_MPI_VPSS_ReleaseChnFrame failed,Grp(%d) chn(%d)!\n",
            ret, aicMediaInfo.vpssGrp, aicMediaInfo.vpssChn0);
    }
}

uint8_t FPS = 0;
uint8_t jpegFlag = 0;
static HI_VOID* GetVpssChnFrameHandDetect(void)
{
    int ret;
    VIDEO_FRAME_INFO_S frm;
    HI_S32 s32MilliSec = 2000;
    VO_LAYER voLayer = 0;
    VO_CHN voChn = 0;
    uint8_t vencChn = 0;
    VENC_RECV_PIC_PARAM_S stRecv;
    stRecv.s32RecvPicNum = 1;

    ret = Yolo2HandDetectResnetClassifyLoad(&AiPlug.model);
    if (ret < 0) 
    {
        AiPlug.model = 0;
        printf("load yolo model err, ret=%#x\n", ret);
        pthread_exit(NULL);
        return HI_NULL;
    }
    else
    {
        printf("Load yolo model success\n");
    }
    SAMPLE_PRT("vpssGrp:%d, vpssChn0:%d\n", aicMediaInfo.vpssGrp, aicMediaInfo.vpssChn0);

    while (AiProcessStopFlag == 0) 
    {
        if (jpegFlag == 0) 
        {
            if(remove("p1.jpg") == 0)
            {
                stRecv.s32RecvPicNum = 1;
                HI_MPI_VENC_StartRecvFrame(vencChn, &stRecv);
                usleep(1000);
                VENC_GetPic(vencChn, "p1.jpg");
                usleep(1000);
                HI_MPI_VENC_StopRecvFrame(vencChn);
                usleep(500);
                jpegFlag = 1;
                FPS++;
            }
            else
            {
                printf("delete jpg fail\n");
            }
        }

        if(AiFlag == 0)
        {
            ret = HI_MPI_VPSS_GetChnFrame(aicMediaInfo.vpssGrp, aicMediaInfo.vpssChn0, &frm, s32MilliSec);
            if (ret != 0) 
            {
                SAMPLE_PRT("HI_MPI_VPSS_GetChnFrame FAIL, err=%#x, grp=%d, chn=%d\n", ret, aicMediaInfo.vpssGrp, aicMediaInfo.vpssChn0);
                ret = HI_MPI_VPSS_ReleaseChnFrame(aicMediaInfo.vpssGrp, aicMediaInfo.vpssChn0, &frm);
                if (ret != HI_SUCCESS) 
                {
                    SAMPLE_PRT("Error(%#x),HI_MPI_VPSS_ReleaseChnFrame failed,Grp(%d) chn(%d)!\n",ret, aicMediaInfo.vpssGrp, aicMediaInfo.vpssChn0);
                }
                usleep(1500);
                continue;
            }
            else
            {
                HandDetectAiProcess(frm, voLayer, voChn);
            }
        }
        usleep(1500);
    }
    pthread_exit(NULL);
    return HI_NULL;
}

static HI_VOID* UDP_TransferTrd(void)
{
    int ret = 0;
    long filesize = 0;
    while(AiProcessStopFlag == 0)
    {
        if(jpegFlag == 1)
        {
            FILE *jpgFile = fopen("p1.jpg", "rb");
            if (jpgFile == NULL)
            {
                jpegFlag = 0;
                printf("failed to open jpgFile\r\n");
            }
            else
            {
                /*get file size*/
                fseek(jpgFile, 0, SEEK_END);
                filesize = ftell(jpgFile);
                fseek(jpgFile, 0, SEEK_SET);
                if (filesize <= 0)
                {
                    jpegFlag = 0;
                    printf("failed to get file size: %d\r\n", filesize);
                }
                else
                {
                    uint8_t *udpSendBuf = (uint8_t*)malloc(filesize);
                    if(udpSendBuf == NULL)
                    {
                        printf("failed to allocate memory\r\n");
                        fclose(jpgFile);
                        jpegFlag = 0;
                        continue;
                    }
                    size_t read_size = fread(udpSendBuf, 1, filesize, jpgFile);
                    if(read_size != filesize) 
                    {
                        printf("failed to read file\r\n");
                        free(udpSendBuf);
                        fclose(jpgFile);
                        jpegFlag = 0;
                        continue;
                    }
                    jpegFlag = 0;

                    ret = udpSend(udpSendBuf, filesize);
                    if(ret == filesize)
                    {
                        printf("send %dB ok\n", ret);
                    }
                    else
                    {
                        printf("send fail, jpgsize: %d B, ret%d\n", filesize, ret);
                    }

                    free(udpSendBuf);
                }
                fclose(jpgFile);
            }
            // jpegFlag = 0;
        }
        usleep(1000);
    }
    pthread_exit(NULL);
}

static HI_VOID* timerSleep(void)
{
    while(AiProcessStopFlag == 0)
    {
        usleep(1000000);
        printf("FPS:%u\n" ,FPS);
        FPS = 0;
    }
    pthread_exit(NULL);
}

uint8_t JpegAndAiTrd(pthread_t *aiThreadid)
{
    uint8_t ret;
    ret = pthread_create(aiThreadid, NULL, GetVpssChnFrameHandDetect, NULL);
    if(ret < 0)
    {
        printf("aiThread create fail.\n");
        return 1;
    }
    return ret;
}

static HI_VOID* UDP_ReceiverTrd(void)
{
    int ret = 0;
    uint8_t ackState = 0;
    while(AiProcessStopFlag == 0)
    {
        usleep(100000);
        ackState = udpAckRecv();
        if(ackState == 0xFF)
        {
            printf("udpAckRecv fail\n");
            continue;
        }
        else
        {
            usleep(2000);
            printf("udpRecv: %u\n", ackState);
            if(udpAckSend(ackState) < 1)
            {
                printf("udpAckSend fail\n");
                udpAckSend(ackState);
            }

            if(ackState == 0)
            {
                AiFlag = 1;
                changeServoAngle(-10);
                LED2_ON();
                LED1_ON();
            }
            else if(ackState == 1)
            {
                AiFlag = 0;
                LED2_OFF();
                LED1_OFF();
            }
            else if(ackState > 1)
            {
                if(Play_audioFile(ackState) == 0)
                {
                    printf("audio playing:%u\n", ackState);
                }
                else
                {
                    printf("audio play fail\n");
                }
            }
            else
            {
                printf("something busy!\n");
            }
        }
    }
    pthread_exit(NULL);
}

static HI_S32 PauseDoUnloadYoloModel(HI_VOID)
{
    HI_S32 s32Ret = HI_SUCCESS;
    /*When exiting the operation, the model should be unloaded*/
    s32Ret = Yolo2HandDetectResnetClassifyUnload(AiPlug.model);
    SAMPLE_CHECK_EXPR_RET(s32Ret != HI_SUCCESS, s32Ret, "unload yolo model err:%x\n", s32Ret);

    return s32Ret;
}

uint8_t aiVision_Init(void)
{
    HI_S32 s32Ret;
    VPSS_LDC_ATTR_S ldcdata = {0};
    VENC_CHN vencChn[1] = {0};
    SIZE_S picsize;
    VENC_RECV_PIC_PARAM_S stRecvParam;

    /*Config VI parameter*/
    ViPramCfg();
    
    /*Obtain enPicSize through the Sensor type*/
    s32Ret = SAMPLE_COMM_VI_GetSizeBySensor(aicMediaInfo.viCfg.astViInfo[0].stSnsInfo.enSnsType, &aicMediaInfo.enPicSize);
    if(s32Ret != HI_SUCCESS)
    {
        printf("get pic size by sensor fail, s32Ret=%#x\n", s32Ret);
        return 1;
    }
    /*Get picture size(w*h), according enPicSize*/
    s32Ret = SAMPLE_COMM_SYS_GetPicSize(aicMediaInfo.enPicSize, &aicMediaInfo.stSize);
    if(s32Ret != HI_SUCCESS)
    {
        printf("get picture size failed, s32Ret=%#x\n", s32Ret);
        return 1;
    }
    SAMPLE_PRT("AIC: snsMaxSize=%ux%u\n", aicMediaInfo.stSize.u32Width, aicMediaInfo.stSize.u32Height);

    /*Config VB parameter*/
    StVbParamCfg(&aicMediaInfo.vbCfg);

    /*VB init & MPI system init*/
    s32Ret = SAMPLE_COMM_SYS_Init(&aicMediaInfo.vbCfg);
    if(s32Ret != HI_SUCCESS)
    {
        printf("system init failed, s32Ret=%#x\n", s32Ret);
        return 1;
    }

#if DEBUGMODE == 1
    /*Set VO config to MIPI, get MIPI device*/
    system("cd /sys/class/gpio/;echo 55 > export;echo out > gpio55/direction;echo 1 > gpio55/value");
    s32Ret = SAMPLE_VO_CONFIG_MIPI(&ai_fd);
    SAMPLE_CHECK_EXPR_GOTO(s32Ret != HI_SUCCESS, EXIT, "CONFIG MIPI FAIL.s32Ret:0x%x\n", s32Ret);
#endif

    /*Config VPSS parameter*/
    VpssParamCfg();
    s32Ret = ViVpssCreate(&aicMediaInfo.viSess, &aicMediaInfo.viCfg, &aicMediaInfo.vpssCfg);
    SAMPLE_CHECK_EXPR_GOTO(s32Ret != HI_SUCCESS, EXIT1, "ViVpss Sess create FAIL, ret=%#x\n", s32Ret);
    aicMediaInfo.vpssGrp = AIC_VPSS_GRP;
    aicMediaInfo.vpssChn0 = AIC_VPSS_ZOUT_CHN;

#if DEBUGMODE == 1
   /*Config VO parameter*/
    StVoParamCfg(&aicMediaInfo.voCfg);
    /*Start VO to MIPI lcd*/
    s32Ret = SampleCommVoStartMipi(&aicMediaInfo.voCfg);
    SAMPLE_CHECK_EXPR_GOTO(s32Ret != HI_SUCCESS, EXIT1, "start vo FAIL. s32Ret: 0x%x\n", s32Ret);
    /*VPSS bind VO*/
    s32Ret = SAMPLE_COMM_VPSS_Bind_VO(aicMediaInfo.vpssGrp, aicMediaInfo.vpssChn0, aicMediaInfo.voCfg.VoDev, 0);
    SAMPLE_CHECK_EXPR_GOTO(s32Ret != HI_SUCCESS, EXIT2, "vo bind vpss FAIL. s32Ret: 0x%x\n", s32Ret);
    SAMPLE_PRT("vpssGrp:%d, vpssChn:%d\n", aicMediaInfo.vpssGrp, aicMediaInfo.vpssChn0);
#endif

    picsize.u32Width = 800;
    picsize.u32Height = 700;
    stRecvParam.s32RecvPicNum = 1;

    remove("p1.jpg");
    SAMPLE_COMM_VENC_SnapStart(vencChn[0], &picsize, HI_FALSE);
    usleep(10000);
    SAMPLE_COMM_VPSS_Bind_VENC(aicMediaInfo.vpssGrp, aicMediaInfo.vpssChn0, vencChn[0]);
    usleep(10000);
    HI_MPI_VENC_StartRecvFrame(vencChn[0], &stRecvParam);
    usleep(5000);
    VENC_GetPic(vencChn[0], "p1.jpg");
    usleep(5000);
    HI_MPI_VENC_StopRecvFrame(vencChn[0]);
    usleep(5000);

    return 0;

EXIT2:
    SAMPLE_COMM_VO_StopVO(&aicMediaInfo.voCfg);
EXIT1:
    VpssStop(&aicMediaInfo.vpssCfg);
    SAMPLE_COMM_VI_UnBind_VPSS(aicMediaInfo.viCfg.astViInfo[0].stPipeInfo.aPipe[0], aicMediaInfo.viCfg.astViInfo[0].stChnInfo.ViChn, aicMediaInfo.vpssGrp);
    ViStop(&aicMediaInfo.viCfg);
    free(aicMediaInfo.viSess);
EXIT:
    SAMPLE_COMM_SYS_Exit();
    return 1;
}

void aiVision_DeInit(void)
{
    PauseDoUnloadYoloModel();
    UDPclient_DeInit();
    Uart1Close();
    SAMPLE_COMM_VPSS_UnBind_VENC(aicMediaInfo.vpssGrp, aicMediaInfo.vpssChn0, 0);
#if DEBUGMODE == 1
    SAMPLE_COMM_VPSS_UnBind_VO(aicMediaInfo.vpssGrp, aicMediaInfo.vpssChn0, aicMediaInfo.voCfg.VoDev, 0);
    SAMPLE_VO_DISABLE_MIPITx(ai_fd);
    SampleCloseMipiTxFd(ai_fd);
    system("echo 0 > /sys/class/gpio/gpio55/value");
    SAMPLE_COMM_VO_StopVO(&aicMediaInfo.voCfg);
#endif
    VpssStop(&aicMediaInfo.vpssCfg);
    SAMPLE_COMM_VI_UnBind_VPSS(aicMediaInfo.viCfg.astViInfo[0].stPipeInfo.aPipe[0], aicMediaInfo.viCfg.astViInfo[0].stChnInfo.ViChn, aicMediaInfo.vpssGrp);
    ViStop(&aicMediaInfo.viCfg);
    free(aicMediaInfo.viSess);
    SAMPLE_COMM_SYS_Exit();
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////
PIDController servo1, servo2;   //x, y
int main(void)
{
    int ret;
    pthread_t t_aiVision;
    pthread_t t_timerSleep;
    pthread_t t_udpTransfer;
    pthread_t t_udpReceiver;

    sdk_init();

    /* GPIO_Init */
    ret = GPIO_Init();
    if(ret != 0)
    {
        printf("GPIO Init fail\n");
        SAMPLE_COMM_SYS_Exit();
        sdk_exit();
        return 0;
    }
    usleep(1000);

    /* Uart1Init */
    ret = Uart1Init();
    if(ret != 0)
    {
        printf("UART1 Init fail\n");
        SAMPLE_COMM_SYS_Exit();
        sdk_exit();
        return 0;
    }
    usleep(1000);

    /* UDPclient_Init */
    ret = UDPclient_Init();
    if(ret != 0)
    {
        printf("udp Init fail\n");
        SAMPLE_COMM_SYS_Exit();
        sdk_exit();
        return 0;
    }
    usleep(1000);

    /* AudioInit & PID_Init */
    PID_Init(&servo1, 0.0165f, 0.037f, 0.035f);
    PID_Init(&servo2, 0.014f, 0.037f, 0.035f);
    usleep(1000);

    /* aiVision_Init */
    aiVision_Init();
    usleep(1000);

    /* main trd */
    JpegAndAiTrd(&t_aiVision);

    /* other trd */
    ret = pthread_create(t_timerSleep, NULL, timerSleep, NULL);
    if(ret < 0)
    {
        printf("timerSleep create fail.\n");
    }
    ret = pthread_create(t_udpTransfer, NULL, UDP_TransferTrd, NULL);
    if(ret < 0)
    {
        printf("UDP_TransferTrd create fail.\n");
    }
    ret = pthread_create(t_udpReceiver, NULL, UDP_ReceiverTrd, NULL);
    if(ret < 0)
    {
        printf("UDP_ReceiverTrd create fail.\n");
    }

    if(Play_audioFile(30) == 0)
    {
        printf("sys audio playing!\n");
    }
    else
    {
        printf("sys audio play fail!\n");
    }

    /* stop? */
    Pause();
    AiProcessStopFlag = 1;
    pthread_join(t_aiVision, NULL);
    aiVision_DeInit();
    sdk_exit();

    return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////
// 初始化PI控制器
void PID_Init(PIDController* pid, float Kp, float Ki, float Kd) 
{
    pid->Kp = Kp;
    pid->Ki = Ki;
    pid->Kd = Kd;
    pid->integral = 0;
    pid->lastError = 0;
}

// PI控制计算函数
short PID_Calculate(PIDController* pid, short setpoint, short feedback) 
{
    // 计算误差
    short error = setpoint - feedback;
    // 计算积分项（带抗饱和）
    pid->integral += error;
    // 防止积分饱和
    if (pid->integral > 10) 
    {
        pid->integral = 10;
    } 
    else if (pid->integral < -10)
    {
        pid->integral = -10;
    }
    // 计算微分项
    short derivative = error - pid->lastError;
    // 计算PID输出
    short output = (short)(round(pid->Kp * error + pid->Ki * pid->integral + pid->Kd * derivative));
    // 保存当前误差用于下次计算
    pid->lastError = error;
    return output;
}

int sockfd;
uint16_t serverPort = 8888;
uint16_t clientPort = 9999;
struct sockaddr_in serverAddr, clientAddr;
int UDPclient_Init(void) 
{
    int ret = 0;
    char serverIP[INET_ADDRSTRLEN];
    FILE *ipFile = fopen("ip.txt", "r");
    if (ipFile == NULL) 
    {
        printf("open ip file fail!\n");
        return 1;
    }
    if (fscanf(ipFile, "ip:%s", serverIP) == 1) 
    {
        printf("ip:%s\n", serverIP);
    } 
    else 
    {
        printf("scan ip file fail!\n");
    }
    fclose(ipFile);

    // 创建UDP套接字
    if((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) 
    {
        perror("socket creation failed");
        return sockfd;
    }

    // 图片发送地址
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(serverPort);
    serverAddr.sin_addr.s_addr = inet_addr(serverIP);
    if (serverAddr.sin_addr.s_addr == INADDR_NONE) 
    {
        printf("invalid IP address: %s\n", serverIP);
        close(sockfd);
        return 1;
    }

    // 绑定应答端口
    memset(&clientAddr, 0, sizeof(clientAddr));
    clientAddr.sin_family = AF_INET;
    clientAddr.sin_port = htons(clientPort);
    clientAddr.sin_addr.s_addr = INADDR_ANY;
    if(bind(sockfd, (struct sockaddr*)&clientAddr, sizeof(clientAddr)) < 0) 
    {
        perror("bind failed");
        close(sockfd);
        return 1;
    }
    printf("Server IP: %s - Server Port: %d - ack Port: %d\n", 
                inet_ntoa(serverAddr.sin_addr), ntohs(serverAddr.sin_port) , ntohs(clientAddr.sin_port));
    
    return 0;
}

int udpSend(uint32_t pBuffer, uint32_t bufLength)
{
    int ret = 0, i = 0;
    uint16_t lastPackLength = 0, packCount = 0;
    if(bufLength <= MTU_USER)
    {
        ret = sendto(sockfd, pBuffer, bufLength, 0, (const struct sockaddr *)&serverAddr, sizeof(serverAddr));
        return ret;
    }
    else
    {
        lastPackLength = bufLength % MTU_USER;
        packCount = bufLength / MTU_USER;
        for(i=0; i<packCount; i++)
        {
            ret += sendto(sockfd, pBuffer + ret, MTU_USER, 0, (const struct sockaddr *)&serverAddr, sizeof(serverAddr));
            usleep(50);
        }
        ret += sendto(sockfd, pBuffer + ret, lastPackLength, 0, (const struct sockaddr *)&serverAddr, sizeof(serverAddr));
        return ret;
    }

}

int udpAckSend(uint8_t ackState)
{
    int ret = 0;
    char ackBuf[8];
    ret = sprintf(ackBuf, "ACK%u", ackState);
    ret = sendto(sockfd, ackBuf, ret, 0, (const struct sockaddr *)&clientAddr, sizeof(clientAddr));
    return ret;
}

uint8_t udpAckRecv(void)
{
    uint8_t ackState = 0;
    char recvBuffer[8];
    socklen_t clientLen = sizeof(struct sockaddr_in);
    if(recvfrom(sockfd, recvBuffer, sizeof(recvBuffer), MSG_WAITALL, (const struct sockaddr *)&clientAddr, &clientLen) < 1)
    {
        return 0xFF;
    }
    ackState = atoi(recvBuffer);
    // printf("udpRecv%d: %s-%c-%u\n", ntohs(clientAddr.sin_port), recvBuffer, recvBuffer[0], ackState);
    return ackState;
}

void getLocalIpPort(void)
{
    if(connect(sockfd, (const struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0)
	{
		close(sockfd);
        printf("\r\nconnect error\r\n");
        printf("\r\nconnect error\r\n");
        printf("\r\nconnect error\r\n");
	}

    struct sockaddr_in localAddr;      //本地客户端的地址信息
    socklen_t localAddrLength = sizeof(localAddr);
    if(getsockname(sockfd, (const struct sockaddr *)&localAddr, &localAddrLength) == 0)
    {
        printf("localSockName: ip=[%s], port=%d.\r\n", inet_ntoa(localAddr.sin_addr), ntohs(localAddr.sin_port));
    }
    else
    {
        printf("getsockname failed!\r\n");
    }
}

void UDPclient_DeInit(void) 
{
    close(sockfd);
}

// // 获取ip
// // 绑定到所有接口的9999端口
// serverAddr.sin_family = AF_INET;
// serverAddr.sin_addr.s_addr = INADDR_ANY;  // 监听所有网络接口
// serverAddr.sin_port = htons(BROADCAST_PORT);
// if (bind(sockfd, (const struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0) 
// {
//     perror("bind failed");
//     close(sockfd);
//     return -1;
// }
// printf("Listening for UDP broadcasts on port %d...\n", BROADCAST_PORT);

// // 阻塞接收
// ret = recvfrom(sockfd, buffer, sizeof(buffer), MSG_WAITALL, (struct sockaddr *)&senderAddr, &senderLen);
// if (ret < 0) 
// {
//     perror("recvfrom failed");
//     close(sockfd);
//     return -1;
// }
// inet_ntop(AF_INET, &(senderAddr.sin_addr), sender_ip, INET_ADDRSTRLEN);
// printf("Received broadcast from %s : %d - Message: %s\n", sender_ip, ntohs(senderAddr.sin_port), buffer);
// usleep(100);

// // 配置服务器地址
// memset(&serverAddr, 0, sizeof(serverAddr));
// serverAddr.sin_family = AF_INET;
// serverAddr.sin_port = htons(SERVER_PORT);
// serverAddr.sin_addr.s_addr = inet_addr(sender_ip);

// getLocalIpPort();

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* End of #ifdef __cplusplus */


