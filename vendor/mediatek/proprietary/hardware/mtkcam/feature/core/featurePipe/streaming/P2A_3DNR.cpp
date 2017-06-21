/* Copyright Statement:
 *
 * This software/firmware and related documentation ("MediaTek Software") are
 * protected under relevant copyright laws. The information contained herein is
 * confidential and proprietary to MediaTek Inc. and/or its licensors. Without
 * the prior written permission of MediaTek inc. and/or its licensors, any
 * reproduction, modification, use or disclosure of MediaTek Software, and
 * information contained herein, in whole or in part, shall be strictly
 * prohibited.
 *
 * MediaTek Inc. (C) 2010. All rights reserved.
 *
 * BY OPENING THIS FILE, RECEIVER HEREBY UNEQUIVOCALLY ACKNOWLEDGES AND AGREES
 * THAT THE SOFTWARE/FIRMWARE AND ITS DOCUMENTATIONS ("MEDIATEK SOFTWARE")
 * RECEIVED FROM MEDIATEK AND/OR ITS REPRESENTATIVES ARE PROVIDED TO RECEIVER
 * ON AN "AS-IS" BASIS ONLY. MEDIATEK EXPRESSLY DISCLAIMS ANY AND ALL
 * WARRANTIES, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE OR
 * NONINFRINGEMENT. NEITHER DOES MEDIATEK PROVIDE ANY WARRANTY WHATSOEVER WITH
 * RESPECT TO THE SOFTWARE OF ANY THIRD PARTY WHICH MAY BE USED BY,
 * INCORPORATED IN, OR SUPPLIED WITH THE MEDIATEK SOFTWARE, AND RECEIVER AGREES
 * TO LOOK ONLY TO SUCH THIRD PARTY FOR ANY WARRANTY CLAIM RELATING THERETO.
 * RECEIVER EXPRESSLY ACKNOWLEDGES THAT IT IS RECEIVER'S SOLE RESPONSIBILITY TO
 * OBTAIN FROM ANY THIRD PARTY ALL PROPER LICENSES CONTAINED IN MEDIATEK
 * SOFTWARE. MEDIATEK SHALL ALSO NOT BE RESPONSIBLE FOR ANY MEDIATEK SOFTWARE
 * RELEASES MADE TO RECEIVER'S SPECIFICATION OR TO CONFORM TO A PARTICULAR
 * STANDARD OR OPEN FORUM. RECEIVER'S SOLE AND EXCLUSIVE REMEDY AND MEDIATEK'S
 * ENTIRE AND CUMULATIVE LIABILITY WITH RESPECT TO THE MEDIATEK SOFTWARE
 * RELEASED HEREUNDER WILL BE, AT MEDIATEK'S OPTION, TO REVISE OR REPLACE THE
 * MEDIATEK SOFTWARE AT ISSUE, OR REFUND ANY SOFTWARE LICENSE FEES OR SERVICE
 * CHARGE PAID BY RECEIVER TO MEDIATEK FOR SUCH MEDIATEK SOFTWARE AT ISSUE.
 *
 * The following software/firmware and/or related documentation ("MediaTek
 * Software") have been modified by MediaTek Inc. All revisions are subject to
 * any receiver's applicable license agreements with MediaTek Inc.
 */

#include "P2A_3DNR.h"
#include "P2ANode.h"

#include "DebugControl.h"
#define PIPE_CLASS_TAG "P2A_3DNR"
// #define PIPE_TRACE TRACE_P2A_3DNR
#define PIPE_TRACE 0
#include <featurePipe/core/include/PipeLog.h>

#include <mtkcam/drv/iopipe/PostProc/IHalPostProcPipe.h>

#include "hal/inc/camera_custom_3dnr.h"
#include <mtkcam/aaa/IspMgrIf.h>
#include <mtkcam/drv/IHalSensor.h>

using namespace NSCam::NSIoPipe::NSPostProc;
using NSImageio::NSIspio::EPortIndex_IMG3O;
using NSImageio::NSIspio::EPortIndex_VIPI;

#include <mtkcam/drv/iopipe/PostProc/IHalDpePipe.h>
using namespace NSCam::NSIoPipe::NSDpe;

namespace NSCam {
namespace NSCamFeature {
namespace NSFeaturePipe {

MBOOL P2ANode::do3dnrFlow(
    NSCam::NSIoPipe::NSPostProc::QParams &enqueParams,
    const RequestPtr &request,
    MRect &dst_resizer_rect,
    eis_region &eisInfo,
    MINT32 iso,
    MUINT32 requestNo)

{
    TRACE_FUNC_ENTER();

    MY_LOGD_IF(m3dnrLogLevel >= 2, "[P2A_3DNR::do3dnrFlow] "
        "imgiWidth: %d, imgiHeight: %d, "
        "dst_resizer_rect_w: %d, dst_resizer_rect_w: %d, iso: %d",
        (enqueParams.mvIn[0].mBuffer)->getImgSize().w, (enqueParams.mvIn[0].mBuffer)->getImgSize().h,
        dst_resizer_rect.s.w, dst_resizer_rect.s.h,
        iso
    );

    if (mb3dnrInitedSuccess != MTRUE)
    {
        MY_LOGE("[P2A_3DNR::do3dnrFlow] mInitedSuccess == FALSE");
        TRACE_FUNC_EXIT();
        return MFALSE;
    }

    MY_LOGD_IF(m3dnrLogLevel >= 2, "mkdbg: iso: %u, imgi: %p, img_vipi: %p",
        iso,
        enqueParams.mvIn[0].mBuffer,
        ((mPrevFullImg != NULL) ? mPrevFullImg->getImageBufferPtr(): NULL)
    );

    MERROR ret = OK;
    mpNr3dParam = NULL;

    MBOOL bDrvNR3DEnabled = 1;
    if (mUsageHint.is3DNRModeMaskEnable(
        IStreamingFeaturePipe::UsageHint::E3DNR_MODE_MASK_HAL_FORCE_SUPPORT) != 0)
    {
        bDrvNR3DEnabled = ::property_get_int32("camera.3dnr.drv.nr3d.enable", 1);
    }

    if (MTRUE != mp3dnr->prepare(requestNo, iso))
    {
        MY_LOGD_IF(m3dnrLogLevel >= 2, "3dnr prepare err");
    }

    mp3dnr->setCMVMode(request->isP2ACRZMode());
    if (MTRUE != mp3dnr->setGMV(requestNo, eisInfo.gmvX, eisInfo.gmvY,
        eisInfo.x_int, eisInfo.y_int))
    {
        MY_LOGD_IF(m3dnrLogLevel >= 2, "3dnr getGMV err");
    }
    if (MTRUE != mp3dnr->checkIMG3OSize(requestNo, dst_resizer_rect.s.w, dst_resizer_rect.s.h))
    {
        MY_LOGD_IF(m3dnrLogLevel >= 2, "3dnr checkIMG3OSize err");
    }

    {
        IImageBuffer *pIMGBufferVIPI = NULL;

        if (mPrevFullImg != NULL && ((pIMGBufferVIPI = mPrevFullImg->getImageBufferPtr()) != NULL))
        {
            if (MTRUE != mp3dnr->setVipiParams(MTRUE,
                  pIMGBufferVIPI->getImgSize().w, pIMGBufferVIPI->getImgSize().h,
                  //pImg3oBuf->getImgFormat(), pIMGBufferVIPI->getBufStridesInBytes(0) // --> replaced by the following line
                  pIMGBufferVIPI->getImgFormat(), pIMGBufferVIPI->getBufStridesInBytes(0))
                )
            {
                MY_LOGD_IF(m3dnrLogLevel >= 2, "skip configVipi flow");
            }
            else
            {
                MY_LOGD_IF(m3dnrLogLevel >= 2, "configVipi: address:%p, W/H(%d,%d)", pIMGBufferVIPI,
                pIMGBufferVIPI->getImgSize().w, pIMGBufferVIPI->getImgSize().h);
                /* config Input for VIPI: this part is done in prepareIO(..) */
            }
        }
        else
        {
            if (MTRUE != mp3dnr->setVipiParams(MFALSE/* vipi is NULL */, 0, 0, 0, 0))
            {
                MY_LOGD_IF(m3dnrLogLevel >= 2, "3dnr configVipi err");
            }
        }
    }

    if (MTRUE != mp3dnr->get3dnrParams(requestNo,
          dst_resizer_rect.s.w, dst_resizer_rect.s.h, mpNr3dParam))
    {
        MY_LOGD_IF(m3dnrLogLevel >= 2, "skip config3dnrParams flow");
    }

    MY_LOGD_IF(m3dnrLogLevel >= 2, "mpNr3dParam: onOff_onOfStX/Y(%d, %d), onSiz_onW/H(%d, %d), vipi_readW/H(%d, %d)",
        mpNr3dParam->onOff_onOfStX, mpNr3dParam->onOff_onOfStY,
        mpNr3dParam->onSiz_onWd, mpNr3dParam->onSiz_onHt,
        mpNr3dParam->vipi_readW, mpNr3dParam->vipi_readH);

    NR3D_Config_Param param;

    if ((MTRUE == mp3dnr->checkStateMachine(NR3D_STATE_WORKING)) && bDrvNR3DEnabled)
    {
        param.enable = bDrvNR3DEnabled;
        param.onRegion.p.x = mpNr3dParam->onOff_onOfStX;
        param.onRegion.p.y = mpNr3dParam->onOff_onOfStY;
        param.onRegion.s.w = mpNr3dParam->onSiz_onWd;
        param.onRegion.s.h = mpNr3dParam->onSiz_onHt;
        param.fullImgSize = dst_resizer_rect.s;

        if (mp3A)
        {
            // turn ON 'pull up ISO value to gain FPS'

            AE_Pline_Limitation_T params;
            params. bEnable = MTRUE;
            params. bEquivalent= MTRUE;
            // use property "camera.3dnr.forceisolimit" to control max_iso_increase_percentage
            // ex: setprop camera.3dnr.forceisolimit 200
            params. u4IncreaseISO_x100= get_3dnr_max_iso_increase_percentage();
            params. u4IncreaseShutter_x100= 100;
            mp3A ->send3ACtrl(E3ACtrl_SetAEPlineLimitation, (MINTPTR)&params, 0);

            MY_LOGD_IF(m3dnrLogLevel >= 2, "turn ON 'pull up ISO value to gain FPS': max: %d %%", get_3dnr_max_iso_increase_percentage());
        }
    }
    else
    {
        if (mp3A)
        {
            // turn OFF 'pull up ISO value to gain FPS'

            //mp3A->modifyPlineTableLimitation(MTRUE, MTRUE,  100, 100);
            AE_Pline_Limitation_T params;
            params. bEnable = MFALSE; // disable
            params. bEquivalent= MTRUE;
            params. u4IncreaseISO_x100= 100;
            params. u4IncreaseShutter_x100= 100;
            mp3A ->send3ACtrl(E3ACtrl_SetAEPlineLimitation, (MINTPTR)&params, 0);

            MY_LOGD_IF(m3dnrLogLevel >= 2, "turn OFF  'pull up ISO value to gain FPS'");
        }
    }

    if (enqueParams.mvTuningData.size() >0)
    {
        void *pIspPhyReg = (void*)enqueParams.mvTuningData[0];

        IHalSensorList* sensorlsit=MAKE_HalSensorList();
        ESensorDev_T tuningSensorDevType = ESensorDev_Main;

        if(sensorlsit)
        {
            tuningSensorDevType = (ESensorDev_T)sensorlsit->querySensorDevIdx(mSensorIndex);
        }

        // log keyword for auto test
        MY_LOGD_IF(mUsageHint.is3DNRModeMaskEnable(
        IStreamingFeaturePipe::UsageHint::E3DNR_MODE_MASK_HAL_FORCE_SUPPORT), "postProcessNR3D: EN(%d)", param.enable);
        MAKE_IspMgrIf().postProcessNR3D(tuningSensorDevType, param, pIspPhyReg);
    }

    TRACE_FUNC_EXIT();

    return OK;
}

MVOID P2ANode::dump_Qparam(QParams& rParams, const char *pSep)
{
    if (mUsageHint.is3DNRModeMaskEnable(
        IStreamingFeaturePipe::UsageHint::E3DNR_MODE_MASK_HAL_FORCE_SUPPORT) == 0)
        return;

    // start dump process
    MINT32 enableOption = ::property_get_int32("camera.3dnr.dump.qparam", 0);
    if (enableOption)
    {
        return;
    }
    if (m3dnrLogLevel == 0)
    {
        return;
    }

    TRACE_FUNC_ENTER();
    MUINT32 i =0;

    MY_LOGD_IF(m3dnrLogLevel >= 2, "%s_: rParams.mDequeSuccess: %d", pSep, rParams.mDequeSuccess);
    if (pSep != NULL && pSep[0] == 'd' && pSep[1] == 'd' && rParams.mDequeSuccess == 0)
    {
        MY_LOGD_IF(m3dnrLogLevel >= 2, "%s_!!! QPARAM DEQUE  FAIL !!!", pSep);
        return;
    }

    MY_LOGD_IF(m3dnrLogLevel >= 2, "%s_: rParams.mpfnCallback: %p", pSep, rParams.mpfnCallback);
    MY_LOGD_IF(m3dnrLogLevel >= 2, "%s_: rParams.mpCookie: %p", pSep, rParams.mpCookie);
#if 0
    MY_LOGD_IF(m3dnrLogLevel >= 2, "%s_: rParams.mpPrivaData: %p", pSep, rParams.mpPrivaData);
    MY_LOGD_IF(m3dnrLogLevel >= 2, "%s_: rParams.mvPrivaData.size(): %d", pSep, rParams.mvPrivaData.size());
#endif

    MY_LOGD_IF(m3dnrLogLevel >= 2, "%s_: rParams.mvTuningData.size(): %d", pSep, rParams.mvTuningData.size());

    // mvIn
    MY_LOGD_IF(m3dnrLogLevel >= 2, "%s_: rParams.mvIn.size(): %d", pSep, rParams.mvIn.size());
    for (i = 0; i < rParams.mvIn.size(); ++i)
    {
        MY_LOGD_IF(m3dnrLogLevel >= 2, "\t%s_: --- rParams.mvIn[#%d]: start --- ", pSep, i);
        Input tmp = rParams.mvIn[i];

        MY_LOGD_IF(m3dnrLogLevel >= 2, "\t%s_: mvIn[%d].portID.index: %d", pSep, i, tmp.mPortID.index);
        MY_LOGD_IF(m3dnrLogLevel >= 2, "\t%s_: mvIn[%d].portID.type: %d", pSep, i, tmp.mPortID.type);
        MY_LOGD_IF(m3dnrLogLevel >= 2, "\t%s_: mvIn[%d].portID.inout: %d", pSep, i, tmp.mPortID.inout);
        MY_LOGD_IF(m3dnrLogLevel >= 2, "\t%s_: mvIn[%d].portID.group: %d", pSep, i, tmp.mPortID.group);
        MY_LOGD_IF(m3dnrLogLevel >= 2, "\t%s_: mvIn[%d].portID.capbility: %d", pSep, i, tmp.mPortID.capbility);
        MY_LOGD_IF(m3dnrLogLevel >= 2, "\t%s_: mvIn[%d].portID.reserved: %d", pSep, i, tmp.mPortID.reserved);

        MY_LOGD_IF(m3dnrLogLevel >= 2, "\t%s_: Input.mBuffer: %p", pSep, tmp.mBuffer);

        if (tmp.mBuffer != NULL)
        {
            MY_LOGD_IF(m3dnrLogLevel >= 2, "\t%s_: mvIn[%d].mBuffer.getImgFormat(): %d", pSep, i, tmp.mBuffer->getImgFormat());
            MY_LOGD_IF(m3dnrLogLevel >= 2, "\t%s_: mvIn[%d].mBuffer.getImgSize(): w=%d, h=%d", pSep, i, tmp.mBuffer->getImgSize().w, tmp.mBuffer->getImgSize().h);
            MY_LOGD_IF(m3dnrLogLevel >= 2, "\t%s_: mvIn[%d].mBuffer.getImgBitsPerPixel(): %d", pSep, i, tmp.mBuffer->getImgBitsPerPixel());

            MY_LOGD_IF(m3dnrLogLevel >= 2, "\t%s_: mvIn[%d].mBuffer.getPlaneCount(): %d", pSep, i, tmp.mBuffer->getPlaneCount());
            for (int k =0; k < tmp.mBuffer->getPlaneCount(); ++k)
            {
                MY_LOGD_IF(m3dnrLogLevel >= 2, "\t%s_: mvIn[%d].mBuffer.getPlaneBitsPerPixel(%d): %d", pSep, i, k, tmp.mBuffer->getPlaneBitsPerPixel(k));
            }
            MY_LOGD_IF(m3dnrLogLevel >= 2, "\t%s_: mvIn[%d].mBuffer.getBitstreamSize(): %d", pSep, i, tmp.mBuffer->getBitstreamSize());
            //            virtual IImageBufferHeap*       getImageBufferHeap()                const   = 0;
            //            MY_LOGD_IF(m3dnrLogLevel >= 2, "\t%s_: mvIn[%d].mBuffer.getExtOffsetInBytes(): %d", pSep, i, tmp.mBuffer->getExtOffsetInBytes());
            //!!NOTES: if VA/PA is going to be used, remember to use lockBuf()/unlockBuf()
            //            (tmp.mBuffer)->lockBuf(PIPE_CLASS_TAG, eBUFFER_USAGE_SW_READ_OFTEN);
            //            (tmp.mBuffer)->unlockBuf(PIPE_CLASS_TAG);
        }

        MY_LOGD_IF(m3dnrLogLevel >= 2, "%s_: --- rParams.mvIn[#%d]: end --- ", pSep, i);
    }

    // mvOut
    MY_LOGD_IF(m3dnrLogLevel >= 2, "%s_: rParams.mvOut.size(): %d", pSep, rParams.mvOut.size());
    for (i = 0; i < rParams.mvOut.size(); ++i)
    {
        MY_LOGD_IF(m3dnrLogLevel >= 2, "\t%s_: --- rParams.mvOut[#%d]: start --- ", pSep, i);
        Output tmp = rParams.mvOut[i];

        MY_LOGD_IF(m3dnrLogLevel >= 2, "\t%s_: mvOut[%d].portID.index: %d", pSep, i, tmp.mPortID.index);
        MY_LOGD_IF(m3dnrLogLevel >= 2, "\t%s_: mvOut[%d].portID.type: %d", pSep, i, tmp.mPortID.type);
        MY_LOGD_IF(m3dnrLogLevel >= 2, "\t%s_: mvOut[%d].portID.inout: %d", pSep, i, tmp.mPortID.inout);
        MY_LOGD_IF(m3dnrLogLevel >= 2, "\t%s_: mvOut[%d].portID.group: %d", pSep, i, tmp.mPortID.group);
        MY_LOGD_IF(m3dnrLogLevel >= 2, "\t%s_: mvOut[%d].portID.capbility: %d", pSep, i, tmp.mPortID.capbility);
        MY_LOGD_IF(m3dnrLogLevel >= 2, "\t%s_: mvOut[%d].portID.reserved: %d", pSep, i, tmp.mPortID.reserved);

        MY_LOGD_IF(m3dnrLogLevel >= 2, "\t%s_: Input.mBuffer: %p", pSep, tmp.mBuffer);

        if (tmp.mBuffer != NULL)
        {
            MY_LOGD_IF(m3dnrLogLevel >= 2, "\t%s_: mvOut[%d].mBuffer.getImgFormat(): %d", pSep, i, tmp.mBuffer->getImgFormat());
            MY_LOGD_IF(m3dnrLogLevel >= 2, "\t%s_: mvOut[%d].mBuffer.getImgSize(): w=%d, h=%d", pSep, i, tmp.mBuffer->getImgSize().w, tmp.mBuffer->getImgSize().h);
            MY_LOGD_IF(m3dnrLogLevel >= 2, "\t%s_: mvOut[%d].mBuffer.getImgBitsPerPixel(): %d", pSep, i, tmp.mBuffer->getImgBitsPerPixel());

            MY_LOGD_IF(m3dnrLogLevel >= 2, "\t%s_: mvOut[%d].mBuffer.getPlaneCount(): %d", pSep, i, tmp.mBuffer->getPlaneCount());
            for (int k =0; k < tmp.mBuffer->getPlaneCount(); ++k)
            {
                MY_LOGD_IF(m3dnrLogLevel >= 2, "\t%s_: mvOut[%d].mBuffer.getPlaneBitsPerPixel(%d): %d", pSep, i, k, tmp.mBuffer->getPlaneBitsPerPixel(k));
            }
            MY_LOGD_IF(m3dnrLogLevel >= 2, "\t%s_: mvOut[%d].mBuffer.getBitstreamSize(): %d", pSep, i, tmp.mBuffer->getBitstreamSize());
            //            virtual IImageBufferHeap*       getImageBufferHeap()                const   = 0;
            //            MY_LOGD_IF(m3dnrLogLevel >= 2, "\t%s_: mvOut[%d].mBuffer.getExtOffsetInBytes(): %d", pSep, i, tmp.mBuffer->getExtOffsetInBytes());
            //!!NOTES: if VA/PA is going to be used, remember to use lockBuf()/unlockBuf()
            //            (tmp.mBuffer)->lockBuf(PIPE_CLASS_TAG, eBUFFER_USAGE_SW_READ_OFTEN);
            //            (tmp.mBuffer)->unlockBuf(PIPE_CLASS_TAG);
        }

        MY_LOGD_IF(m3dnrLogLevel >= 2, "%s_: --- rParams.mvOut[#%d]: end --- ", pSep, i);
    }

    TRACE_FUNC_EXIT();
}

MVOID P2ANode::dump_vOutImageBuffer(const QParams & params)
{
    // === default values initialized ===
    static int num_img3o_frame_to_dump = 0;
    static int dumped_frame_count = 0;
    static int is_dump_complete = 1;
    static int dump_round_count = 1;
    // ==================================

    // start dump process
    char EnableOption[PROPERTY_VALUE_MAX] = {'\0'};
    if (num_img3o_frame_to_dump == 0  || dumped_frame_count == num_img3o_frame_to_dump)
    {

        num_img3o_frame_to_dump = 0;
        if (mUsageHint.is3DNRModeMaskEnable(
            IStreamingFeaturePipe::UsageHint::E3DNR_MODE_MASK_HAL_FORCE_SUPPORT) != 0)
        {
            num_img3o_frame_to_dump = ::property_get_int32("camera.3dnr.dump.img3o", 0);
        }
//        MY_LOGW("(dumped_frame_count=%d, num_img3o_frame_to_dump =%d), no need to dump vOut frames",
  //          dumped_frame_count, num_img3o_frame_to_dump);
        return;
    }

    //debug: start
    char vOut0_frame_str[64];
    char vOutIMG3O_frame_str[64];

    // start from scratch
    if (is_dump_complete)
    {
        is_dump_complete = 0;
        dumped_frame_count = 0;
    }

    if (dumped_frame_count  < num_img3o_frame_to_dump )
    {
        int is_img3o_dumped = 0;
        MY_LOGD_IF(m3dnrLogLevel >= 2, "mvOut size = %d", params.mvOut.size());

        for (int i =0; i < params.mvOut.size(); ++i)
        {

//            MY_LOGD_IF(m3dnrLogLevel >= 2, "mkdbg: mvOut[%d].mPortID: %d", i, params.mvOut[i].mPortID);
            if (i == 0)
            {
                if (params.mDequeSuccess != 0)
                {
                    sprintf(vOut0_frame_str, "/sdcard/vOut0_frame-r%.2d_%.3d_%dx%d_OK.yuv",
                        dump_round_count, dumped_frame_count,
                        params.mvOut[0].mBuffer->getImgSize().w, params.mvOut[0].mBuffer->getImgSize().h
                        );
                }
                else
                {
                    sprintf(vOut0_frame_str, "/sdcard/vOut0_frame-r%.2d_%.3d_%dx%d_NG.yuv",
                        dump_round_count, dumped_frame_count,
                        params.mvOut[0].mBuffer->getImgSize().w, params.mvOut[0].mBuffer->getImgSize().h
                        );
                }
                params.mvOut[0].mBuffer->saveToFile(vOut0_frame_str);
                MY_LOGD_IF(m3dnrLogLevel >= 2, "params.mvOut[0] saved: %p", params.mvOut[0].mBuffer);
            }

            if (params.mvOut[i].mPortID == PortID(EPortType_Memory, EPortIndex_IMG3O, PORTID_OUT) )
            {
                if (params.mDequeSuccess != 0)
                {
                    sprintf(vOutIMG3O_frame_str, "/sdcard/vOutIMG3O_frame-r%.2d_%.3d_%dx%d_OK.yuv",
                        dump_round_count, dumped_frame_count,
                        params.mvOut[i].mBuffer->getImgSize().w, params.mvOut[i].mBuffer->getImgSize().h
                        );
                }
                else
                {
                    sprintf(vOutIMG3O_frame_str, "/sdcard/vOutIMG3O_frame-r%.2d_%.3d_%dx%d_NG.yuv",
                        dump_round_count, dumped_frame_count,
                        params.mvOut[i].mBuffer->getImgSize().w, params.mvOut[i].mBuffer->getImgSize().h
                        );
                }
                params.mvOut[i].mBuffer->saveToFile(vOutIMG3O_frame_str);
                MY_LOGD_IF(m3dnrLogLevel >= 2, "params.mvOut[%d] EPortIndex_IMG3O saved: %p", i, params.mvOut[i].mBuffer);
                is_img3o_dumped = 1;
            }
        }

        if (is_img3o_dumped == 0)
        {
            MY_LOGD_IF(m3dnrLogLevel >= 2, "mkdbg: !!err: no IMG3O buffer dumped");
            MY_LOGD_IF(m3dnrLogLevel >= 2, "mkdbg: !!err: no IMG3O buffer dumped");
        }
        ++dumped_frame_count;

        if (dumped_frame_count  >= num_img3o_frame_to_dump)
        {
            // when the dump is complete...
            is_dump_complete = 1;
            num_img3o_frame_to_dump = 0;
            MY_LOGD_IF(m3dnrLogLevel >= 2, "dump round %.2d finished ... (dumped_frame_count=%d, num_img3o_frame_to_dump =%d)",
                dump_round_count++, dumped_frame_count, num_img3o_frame_to_dump);
        }
    }
}


MVOID P2ANode::dump_imgiImageBuffer(const QParams & params)
{
    if (mUsageHint.is3DNRModeMaskEnable(
        IStreamingFeaturePipe::UsageHint::E3DNR_MODE_MASK_HAL_FORCE_SUPPORT) == 0)
        return;
#if 1
#warning [TODO] dump_imgiImageBuffer

  #if 0
    // start dump process
    char EnableOption[PROPERTY_VALUE_MAX] = {'\0'};
    property_get("camera.3dnr.dump.imgi", EnableOption, "0");
    if (EnableOption[0] == '1')
    {
        MY_LOGW_IF(m3dnrLogLevel >= 2, "(need to dump_imgiImageBuffer(..) ");
    }
    else
    {
        MY_LOGW_IF(m3dnrLogLevel >= 2, "(NO need to dump_imgiImageBuffer(..) ");
    }
  #endif
    return;

#else
    //debug: start
    char vIn0_frame_str[64];
    static int incoming_frame_count = 0;
    static int saved_frame_count = 0;

    if (saved_frame_count <= 5) // dump the first three frames
    //  if (is_frame_saved == 0) // dump the first one only
    {
        sprintf(vIn0_frame_str, "/sdcard/vIn0_frame-%.3d_%dx%d.raw",
            incoming_frame_count,
            params.mvIn[0].mBuffer->getImgSize().w, params.mvIn[0].mBuffer->getImgSize().h);

        params.mvIn[0].mBuffer->saveToFile(vIn0_frame_str);
        MY_LOGD_IF(m3dnrLogLevel >= 2, "params.mvIn[0] saved: %p", params.mvIn[0].mBuffer);
        ++saved_frame_count;
    }
    incoming_frame_count++;
#endif
}


} // namespace NSFeaturePipe
} // namespace NSCamFeature
} // namespace NSCam