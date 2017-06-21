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

#ifndef _MTK_PLATFORM_HARDWARE_INCLUDE_MTKCAM_IOPIPE_POSTPROC_FEATURESTREAM_H_
#define _MTK_PLATFORM_HARDWARE_INCLUDE_MTKCAM_IOPIPE_POSTPROC_FEATURESTREAM_H_
//

#include "HalPipeWrapper_FrmB.h"
#include <mtkcam/iopipe/PostProc/IFeatureStream.h>
#include <utils/threads.h>
#include <list>
#include <vector>
#include <mtkcam/hal/IHalSensor.h>
#include "isp_datatypes_FrmB.h"


using namespace NSImageio_FrmB;
using namespace NSIspio_FrmB;
namespace NSImageio_FrmB{
namespace NSIspio_FrmB{
    class IPostProcPipe;
};
};

/******************************************************************************
 *
 ******************************************************************************/
namespace NSCam {
namespace NSIoPipe {
namespace NSPostProc_FrmB {

/******************************************************************************
 *
 * @class INormalStream
 * @brief Post-Processing Pipe Interface for Normal Stream.
 * @details
 * The data path will be Mem --> ISP--XDP --> Mem.
 *
 ******************************************************************************/
class FeatureStream : public IFeatureStream
{
    friend  class IFeatureStream;
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  IPipe Interface.
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
public:     ////                    Instantiation.
    /**
     * @brief destroy the pipe instance
     *
     * @details
     *
     * @note
     */
    virtual MVOID                   destroyInstance(char const* szCallerName);
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  Implementations.
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
protected:
                                    FeatureStream(EFeatureStreamTag streamTag,MUINT32 openedSensorIndex, MBOOL isV3);
public:
    virtual                         ~FeatureStream();
public:
    /**
     * @brief init the pipe
     *
     * @details
     *
     * @note
     *
     * @return
     * - MTRUE indicates success;
     * - MFALSE indicates failure, and an error code can be retrived by getLastErrorCode().
     */
    virtual MBOOL                   init();

    /**
     * @brief uninit the pipe
     *
     * @details
     *
     * @note
     *
     * @return
     * - MTRUE indicates success;
     * - MFALSE indicates failure, and an error code can be retrived by getLastErrorCode().
     */
    virtual MBOOL                   uninit();
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  Buffer Operations.
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    public:     ////

        /**
     * @brief En-queue a request into the pipe.
     *
     * @details
     *
     * @note
     *
     * @param[in] rParams: Reference to a request of QParams structure.
     *
     * @return
     * - MTRUE indicates success;
     * - MFALSE indicates failure, and an error code can be retrived by \n
     *   getLastErrorCode().
     */
    virtual MBOOL                   enque(
                                        QParams const& rParams
                                    );
   /**
     * @brief notify start video record for slow motion support.
     *
     * @param[in] wd: width
     * @param[in] ht: height
     *
     * @details
     *
     * @note
     *
     * @return
     *      - [true]
     */
    virtual MBOOL                    startVideoRecord(MINT32 wd,MINT32 ht, MINT32 fps=120);
    /**
     * @brief notify stop video record for slow motion support.
     *
     * @details
     *
     * @note
     *
     * @return
     *      - [true]
     */
    virtual MBOOL                    stopVideoRecord();
    /**
     * @brief De-queue a result from the pipe.
     *
     * @details
     *
     * @note
     *
     * @param[in] rParams: Reference to a result of QParams structure.
     *
     * @param[in] i8TimeoutNs: timeout in nanoseconds \n
     *      If i8TimeoutNs > 0, a timeout is specified, and this call will \n
     *      be blocked until a result is ready. \n
     *      If i8TimeoutNs = 0, this call must return immediately no matter \n
     *      whether any buffer is ready or not. \n
     *      If i8TimeoutNs = -1, an infinite timeout is specified, and this call
     *      will block forever.
     *
     * @return
     * - MTRUE indicates success;
     * - MFALSE indicates failure, and an error code can be retrived by getLastErrorCode().
     */
    virtual MBOOL                   deque(
                                        QParams& rParams,
                                        MINT64 i8TimeoutNs = -1
                                    );
   /**
     * @brief Query dma position that before/behind isp resizer and other dma ports in the same crop group.
     *
     * @details
     *
     * @note
     *
     * @param[in] portIdx: port index.
     * @param[in] vGroupPortIdx: vector of index of the ports that belongs to the same group with portIdx.
     *
     * @return
     * - MTRUE indicates success;
     * - MFALSE indicates failure, and an error code can be retrived by getLastErrorCode().
     */
   virtual MBOOL                   queryGroupMember(
                                        MINT32 portIdx,
                                        MBOOL& beforeCRZ,
                                        android::Vector<MINT32>& vGroupPortIdx
                                    );
    /**
     * @brief get the last error code
     *
     * @details
     *
     * @note
     *
     * @return
     * - The last error code
     *
     */
    virtual MERROR                  getLastErrorCode() const {return 0;}
    /**
     * @brief get the supported crop paths
     *
     * @details
     *
     * @note
     *
     * @return
     * -
     *
     */
    virtual MBOOL                  queryCropInfo(android::Vector<MCropPathInfo>& mvCropPathInfo);


    /**
         * @brief send isp extra command
         *
         * @details
         *
         * @note
         *
         * @param[in] cmd: command
         * @param[in] arg1: arg1
         * @param[in] arg2: arg2
         *
         * @return
         * - MTRUE indicates success;
         * - MFALSE indicates failure, and an error code can be retrived by sendCommand().
         */
    virtual MBOOL                    sendCommand(
                                         MINT32 cmd,
                                         MINT32 arg1,
                                         MINT32 arg2);

    /**
     * @brief get a buffer for tuning data provider
     *
     * @details
     *
     * @note
     *
     * @param[out] size: get the tuning buffer size
     * @param[out] pTuningQueBuf : get the tuning buffer point, user need to map to isp_reg_t(isp_reg.h) before use it.
     *
     * @return
     * - MTRUE  indicates success;
     * - MFALSE indicates failure;
     */
    virtual MBOOL                   deTuningQue(unsigned int& size, void* &pTuningQueBuf);
    /**
     * @brief return the buffer which be got before
     *
     * @details
     *
     * @note
     *
     * @param[in] pTuningQueBuf : return the pTuningQueBuf
     *
     * @return
     * - MTRUE  indicates success;
     * - MFALSE indicates failure;
     */
    virtual MBOOL                   enTuningQue(void* pTuningQueBuf);

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  Variables.
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
private:
    HalPipeWrapper*         mpHalPipeWrapper;
    IPostProcPipe*          mpPostProcPipe;
    Mutex                   mModuleMtx;
    const char              *mpName;
    MUINT32                 mOpenedSensor;
    EFeatureStreamTag       mStreamTag;
    vector<QParams>         mDequeuedBufList;
    vector<CropPathInfo>    mCropPaths;
    IHalSensorList*         mHalSensorList;
    ESoftwareScenario       mSWScen;
    MBOOL                   misV3;
    MUINT32                 pixIdP2;
};


/******************************************************************************
 *
 ******************************************************************************/
};  //namespace NSPostProc_FrmB
};  //namespace NSIoPipe
};  //namespace NSCam
#endif  //_MTK_PLATFORM_HARDWARE_INCLUDE_MTKCAM_IOPIPE_POSTPROC_FEATURESTREAM_H_

