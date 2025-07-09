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

/*
 * 该文件提供了基于yolov2的手部检测以及基于resnet18的手势识别，属于两个wk串行推理。
 * 该文件提供了手部检测和手势识别的模型加载、模型卸载、模型推理以及AI flag业务处理的API接口。
 * 若一帧图像中出现多个手，我们通过算法将最大手作为目标手送分类网进行推理，
 * 并将目标手标记为绿色，其他手标记为红色。
 *
 * This file provides hand detection based on yolov2 and gesture recognition based on resnet18,
 * which belongs to two wk serial inferences. This file provides API interfaces for model loading,
 * model unloading, model reasoning, and AI flag business processing for hand detection
 * and gesture recognition. If there are multiple hands in one frame of image,
 * we use the algorithm to use the largest hand as the target hand for inference,
 * and mark the target hand as green and the other hands as red.
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <math.h>

#include "sample_comm_nnie.h"
#include "sample_media_ai.h"
#include "ai_infer_process.h"
#include "yolov2_hand_detect.h"
#include "vgs_img.h"
#include "ive_img.h"
#include "misc_util.h"
#include "gpio_user.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif /* End of #ifdef __cplusplus */

#define HAND_FRM_WIDTH     640
#define HAND_FRM_HEIGHT    384
#define DETECT_OBJ_MAX     32
#define RET_NUM_MAX        4
#define DRAW_RETC_THICK    2    // Draw the width of the line
#define WIDTH_LIMIT        32
#define HEIGHT_LIMIT       32
#define IMAGE_WIDTH        224  // The resolution of the model IMAGE sent to the classification is 224*224
#define IMAGE_HEIGHT       224
#define MODEL_FILE_GESTURE    "/userdata/hand_gesture.wk" // darknet framework wk model

static int biggestBoxIndex1, biggestBoxIndex2;
static IVE_IMAGE_S img;
static DetectObjInfo objs[DETECT_OBJ_MAX] = {0};
static RectBox boxs[DETECT_OBJ_MAX] = {0};
static RectBox objBoxs[DETECT_OBJ_MAX] = {0};
static RectBox remainingBoxs[DETECT_OBJ_MAX] = {0};
// static RectBox cnnBoxs[DETECT_OBJ_MAX] = {0}; // Store the results of the classification network
static RecogNumInfo numInfo[RET_NUM_MAX] = {0};
static IVE_IMAGE_S imgIn;
static IVE_IMAGE_S imgDst;
static VIDEO_FRAME_INFO_S frmIn;
static VIDEO_FRAME_INFO_S frmDst;

/*
 * 加载手部检测和手势分类模型
 * Load hand detect and classify model
 */
HI_S32 Yolo2HandDetectResnetClassifyLoad(uintptr_t* model)
{
    SAMPLE_SVP_NNIE_CFG_S *self = NULL;
    HI_S32 ret;
    // ret = CnnCreate(&self, MODEL_FILE_GESTURE);
    // *model = ret < 0 ? 0 : (uintptr_t)self;
    HandDetectInit(); // Initialize the hand detection model
    SAMPLE_PRT("Load hand detect claasify model success\n");
    return ret;
}

/*
 * 卸载手部检测和手势分类模型
 * Unload hand detect and classify model
 */
HI_S32 Yolo2HandDetectResnetClassifyUnload(uintptr_t model)
{
    // CnnDestroy((SAMPLE_SVP_NNIE_CFG_S*)model);
    HandDetectExit(); // Uninitialize the hand detection model
    SAMPLE_PRT("Unload hand detect claasify model success\n");
    return 0;
}

/*
 * 获得最大的手
 * Get the maximum hand
 */
static void GetBiggestHandIndex(RectBox boxs[], int detectNum)
{
    if(detectNum == 0 || boxs == NULL) 
    {
        biggestBoxIndex1 = -1;
        biggestBoxIndex2 = -1;
        return;
    }
    else if(detectNum == 1) 
    {
        HI_S32 boxArea;
        HI_S32 handIndex = 0;
        HI_S32 boxWidth = boxs[handIndex].xmax - boxs[handIndex].xmin + 1;
        HI_S32 boxHeight = boxs[handIndex].ymax - boxs[handIndex].ymin + 1;
        HI_S32 biggestBoxArea = boxWidth * boxHeight;
        biggestBoxIndex1 = 0, biggestBoxIndex2 = -1;
        for (handIndex = 1; handIndex < detectNum; handIndex++) 
        {
            boxWidth = boxs[handIndex].xmax - boxs[handIndex].xmin + 1;
            boxHeight = boxs[handIndex].ymax - boxs[handIndex].ymin + 1;
            boxArea = boxWidth * boxHeight;
            if (biggestBoxArea < boxArea) 
            {
                biggestBoxArea = boxArea;
                biggestBoxIndex1 = handIndex;
            }
        }
        return;
    }
    else if(detectNum > 1) 
    {
        HI_S32 boxArea;
        HI_S32 handIndex = 0;
        HI_S32 boxWidth = boxs[handIndex].xmax - boxs[handIndex].xmin + 1;
        HI_S32 boxHeight = boxs[handIndex].ymax - boxs[handIndex].ymin + 1;
        HI_S32 biggestBoxArea1 = boxWidth * boxHeight, biggestBoxArea2 = -1;
        biggestBoxIndex1 = 0, biggestBoxIndex2 = -1;
        for (handIndex = 1; handIndex < detectNum; handIndex++) 
        {
            boxWidth = boxs[handIndex].xmax - boxs[handIndex].xmin + 1;
            boxHeight = boxs[handIndex].ymax - boxs[handIndex].ymin + 1;
            boxArea = boxWidth * boxHeight;
            if (biggestBoxArea1 < boxArea) 
            {
                // 当前框比最大的还大，原来的最大变成第二大
                biggestBoxIndex2 = biggestBoxIndex1;
                biggestBoxArea2 = biggestBoxArea1;
                
                biggestBoxIndex1 = handIndex;
                biggestBoxArea1 = boxArea;
            } 
            else if (biggestBoxIndex2 == -1 || biggestBoxArea2 < boxArea) 
            {
                // 当前框介于最大和第二大之间
                biggestBoxIndex2 = handIndex;
                biggestBoxArea2 = boxArea;
            }
        }
        return;
    }
}

/*
 * 手部检测和手势分类推理
 * Hand detect and classify calculation
 */
uint8_t angle1 = 90, angle2 = 140;
const short setpointX = 320;  // 目标X坐标
const short setpointY = 192;  // 目标Y坐标
HI_S32 Yolo2HandDetectResnetClassifyCal(uintptr_t model, VIDEO_FRAME_INFO_S *srcFrm, VIDEO_FRAME_INFO_S *dstFrm)
{
    SAMPLE_SVP_NNIE_CFG_S *self = (SAMPLE_SVP_NNIE_CFG_S*)model;
    HI_S32 resLen = 0;
    int objNum;
    int ret;
    int num = 0;

    ret = FrmToOrigImg((VIDEO_FRAME_INFO_S*)srcFrm, &img);
    if(ret != HI_SUCCESS)
    {
        printf("hand detect for YUV Frm to Img FAIL, ret=%#x\n", ret);
        return ret;
    }

    objNum = HandDetectCal(&img, objs); // Send IMG to the detection net for reasoning
    for (int i = 0; i < objNum; i++) 
    {
        // cnnBoxs[i] = objs[i].box;
        // RectBox *box = &objs[i].box;
        // RectBoxTran(box, HAND_FRM_WIDTH, HAND_FRM_HEIGHT, dstFrm->stVFrame.u32Width, dstFrm->stVFrame.u32Height);
        boxs[i] = objs[i].box;
        // printf("yolo2:{%d, %d, %d, %d}\n", boxs->xmin, boxs->ymin, boxs->xmax, boxs->ymax);
        // boxs[i] = *box;
    }

    GetBiggestHandIndex(boxs, objNum);

    if(objNum > 0)
    {
        short xPoint, yPoint;
        if(objNum == 1)
        {
            LED2_OFF();
            LED1_ON();
            xPoint = (boxs[biggestBoxIndex1].xmin + boxs[biggestBoxIndex1].xmax) / 2;
            yPoint = (boxs[biggestBoxIndex1].ymin + boxs[biggestBoxIndex1].ymax) / 2;
        }
        else
        {
            LED1_OFF();
            LED2_ON();
            xPoint = (boxs[biggestBoxIndex1].xmin + boxs[biggestBoxIndex1].xmax + boxs[biggestBoxIndex2].xmin + boxs[biggestBoxIndex2].xmax) / 4;
            yPoint = (boxs[biggestBoxIndex1].ymin + boxs[biggestBoxIndex1].ymax + boxs[biggestBoxIndex2].ymin + boxs[biggestBoxIndex2].ymax) / 4;
        }
        short deltaX = PID_Calculate(&servo1, setpointX, xPoint);
        short deltaY = PID_Calculate(&servo2, setpointY, yPoint);
        // printf("xPoint:%d, yPoint:%d, biggestIndex1:%d, biggestIndex2:%d, objNum:%d\n", 
        //                                         xPoint, yPoint, biggestBoxIndex1, biggestBoxIndex2, objNum);
        angle1 = (uint8_t)(angle1 + deltaX);
        angle2 = (uint8_t)(angle2 - deltaY);

        if(angle1 < 10) angle1 = 10;
        if(angle1 > 170) angle1 = 170;
        if(angle2 < 80) angle2 = 80;
        if(angle2 > 160) angle2 = 160;

        uint8_t uartSendBuf[4];
        uartSendBuf[0] = 0xA5;
        uartSendBuf[1] = angle1;
        uartSendBuf[2] = angle2;
        uartSendBuf[3] = (uint8_t)(uartSendBuf[0] + uartSendBuf[1] + uartSendBuf[2]);
        ret = Uart1Send(uartSendBuf, sizeof(uartSendBuf));
        if(ret != sizeof(uartSendBuf))
        {
            printf("uart send fail:%d\n", ret);
        }
    }
    else
    {
        LED1_OFF();
        LED2_OFF();
    }

    /*
     * 当检测到对象时，在DSTFRM中绘制一个矩形
     * When an object is detected, a rectangle is drawn in the DSTFRM
     */
    // if (biggestBoxIndex >= 0) {
    //     objBoxs[0] = boxs[biggestBoxIndex];
    //     MppFrmDrawRects(dstFrm, objBoxs, 1, RGB888_GREEN, DRAW_RETC_THICK); // Target hand objnum is equal to 1

    //     for (int j = 0; (j < objNum) && (objNum > 1); j++) {
    //         if (j != biggestBoxIndex) {
    //             remainingBoxs[num++] = boxs[j];
    //             /*
    //              * 其他手objnum等于objnum -1
    //              * Others hand objnum is equal to objnum -1
    //              */
    //             MppFrmDrawRects(dstFrm, remainingBoxs, objNum - 1, RGB888_RED, DRAW_RETC_THICK);
    //         }
    //     }

        /*
         * 裁剪出来的图像通过预处理送分类网进行推理
         * The cropped image is preprocessed and sent to the classification network for inference
         */
        // ret = ImgYuvCrop(&img, &imgIn, &cnnBoxs[biggestBoxIndex]);
        // SAMPLE_CHECK_EXPR_RET(ret < 0, ret, "ImgYuvCrop FAIL, ret=%#x\n", ret);

        // if ((imgIn.u32Width >= WIDTH_LIMIT) && (imgIn.u32Height >= HEIGHT_LIMIT)) {
        //     COMPRESS_MODE_E enCompressMode = srcFrm->stVFrame.enCompressMode;
        //     ret = OrigImgToFrm(&imgIn, &frmIn);
        //     frmIn.stVFrame.enCompressMode = enCompressMode;
        //     SAMPLE_PRT("crop u32Width = %d, img.u32Height = %d\n", imgIn.u32Width, imgIn.u32Height);
        //     ret = MppFrmResize(&frmIn, &frmDst, IMAGE_WIDTH, IMAGE_HEIGHT);
        //     ret = FrmToOrigImg(&frmDst, &imgDst);
        //     ret = CnnCalImg(self,  &imgDst, numInfo, sizeof(numInfo) / sizeof((numInfo)[0]), &resLen);
        //     SAMPLE_CHECK_EXPR_RET(ret < 0, ret, "CnnCalImg FAIL, ret=%#x\n", ret);
        //     HI_ASSERT(resLen <= sizeof(numInfo) / sizeof(numInfo[0]));
        //     HandDetectFlag(numInfo[0]);
        //     MppFrmDestroy(&frmDst);
        // }
        // IveImgDestroy(&imgIn);
    // }

    return ret;
}

void changeServoAngle(int8_t deltaAngle)
{
    uint8_t uartSendBuf[4];
    uartSendBuf[0] = 0xA5;
    uartSendBuf[1] = angle1;
    uartSendBuf[2] = angle2 - deltaAngle;
    uartSendBuf[3] = (uint8_t)(uartSendBuf[0] + uartSendBuf[1] + uartSendBuf[2]);
    if(Uart1Send(uartSendBuf, sizeof(uartSendBuf)) != sizeof(uartSendBuf))
    {
        printf("changeServoAngle fail\n");
    }
}

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* End of #ifdef __cplusplus */
