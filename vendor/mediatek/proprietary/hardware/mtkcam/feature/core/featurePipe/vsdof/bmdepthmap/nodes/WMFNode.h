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
/**
 * @file WMFNode.h
 * @brief WMFNode inside the DepthMapPipe
*/
#ifndef _MTK_CAMERA_INCLUDE_DEPTHMAP_FEATURE_PIPE_WMF_NODE_H
#define _MTK_CAMERA_INCLUDE_DEPTHMAP_FEATURE_PIPE_WMF_NODE_H


// Standard C header file
#include <queue>
// Android system/core header file
#include <utils/KeyedVector.h>
// mtkcam custom header file

// mtkcam global header file
#include <mtkcam/drv/def/dpecommon.h>
#include <mtkcam/drv/iopipe/PostProc/IDpeStream.h>
#include <mtkcam/drv/iopipe/PostProc/IHalDpePipe.h>
#include <DpBlitStream.h>
// Module header file
#include <featurePipe/core/include/WaitQueue.h>
// Local header file
#include "../DepthMapPipeNode.h"
#include "../DepthMapPipe_Common.h"

/*******************************************************************************
* Namespace start.
********************************************************************************/
namespace NSCam {
namespace NSCamFeature {
namespace NSFeaturePipe_DepthMap_BM {

using namespace NSCam::NSIoPipe::NSDpe;
using namespace NSCam::NSIoPipe;
using std::queue;

class WMFNode: public DepthMapPipeNode
{
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  Instantiation.
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
public:
    WMFNode(
        const char *name,
        DepthMapPipeNodeID nodeID,
        sp<DepthMapFlowOption> pFlowOpt,
        const DepthMapPipeSetting& setting);
    virtual ~WMFNode();

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  DepthMapPipeNode Public Operations.
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
public:
    virtual MBOOL onData(DataID id, DepthMapRequestPtr &request);
    virtual MBOOL onInit();
    virtual MBOOL onUninit();
    virtual MBOOL onThreadLoop();
    virtual MBOOL onThreadStart();
    virtual MBOOL onThreadStop();
    // need manual onFlush to clear internal data
    virtual MVOID onFlush();

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  DepthMapPipeNode Protected Operations.
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
protected:
    virtual const char* onDumpBIDToName(DepthMapBufferID BID);

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  WMFNode Private Operations.
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
private:
    struct EnqueBIDConfig
    {
        DepthMapBufferID srcImgBID;
        DepthMapBufferID srcDepthBID;
        DepthMapBufferID interalImgBID;
        DepthMapBufferID outDepthBID;
    };
    /**
     * @brief Prepare WMF enque config
     * @param [in] pRequest Current effect request
     * @param [in] enqueConfig enque buffer id config
     * @param [out] rWMFConfig WMF enque params
     * @return
     * - MTRUE indicates success.
     * - MFALSE indicates failure.
     */
    MVOID prepareWMFEnqueConfig(
                            sp<BaseBufferHandler> pBufferHandler,
                            const EnqueBIDConfig& enqueConfig,
                            WMFEConfig &rWMFConfig);
    /**
     * @brief WMF internal thread loop : first pass
     */
    MBOOL threadLoop_WMFFirstPass(DepthMapRequestPtr& pRequest);
    /**
     * @brief WMF success callback function : first pass
     */
    static MVOID onWMFEnqueDone_FirstPass(WMFEParams& rParams);
     /**
     * @brief WMF enqued-operation finished handle function : first pass
     * @param [in] rParams dequed WMFEParams
     * @param [in] pEnqueCookie EnqueCookie instance
     */
    MVOID handleWMFEnqueDone_FirstPass(
                                WMFEParams& rParams,
                                EnqueCookieContainer* pEnqueCookie);
    /**
     * @brief WMF internal thread loop : second pass
     */
    MBOOL threadLoop_WMFSecPass(DepthMapRequestPtr& pRequest);
    /**
     * @brief WMF success callback function : second pass
     */
    static MVOID onWMFEnqueDone_SecPass(WMFEParams& rParams);
    /**
     * @brief WMF enqued-operation finished handle function : second pass
     * @param [in] rParams dequed WMFEParams
     * @param [in] pEnqueCookie EnqueCookie instance
     */
    MVOID handleWMFEnqueDone_SecPass(
                                WMFEParams& rParams,
                                EnqueCookieContainer* pEnqueCookie);
    /**
     * @brief build image buffer into the DPEBufInfo struction
     * @param [in] dmaPort DPE DMA port
     * @param [in] dmaPort DPE DMA port
     * @param [out] rBufInfo DPEBufInfo
     * @return
     * - MTRUE indicates success.
     * - MFALSE indicates failure.
     */
    MBOOL setupDPEBufInfo(
                    DPEDMAPort dmaPort,
                    IImageBuffer* pImgBuf,
                    DPEBufInfo& rBufInfo);
    /**
     * @brief cleanup function - release hal/resources
     */
    MVOID cleanUp();
    /**
     * @brief debug input params
     */
    MVOID debugWMFParam(WMFEConfig &config);
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  WMFNode Private Function
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
private:
    WaitQueue<DepthMapRequestPtr> mJobQueue;
    // second pass WMF operation Queue
    WaitQueue<DepthMapRequestPtr> mWMFSecQueue;
    // Record the request id of whose MY_SL buffer is ready
    queue<MUINT32> mMYSLReqIDQueue;
    Mutex mMYSLMutex;
    // DPE stream
    IDpeStream* mpDPEStream;
    // TBLI buffer pool
    android::sp<ImageBufferPool> mpTbliImgBufPool;
    // tuning buffer : Tbli buffer
    SmartImageBuffer mpTbliImgBuf;
    // tuning parameter: filter size
    WMFEFILTERSIZE  eWMFFilterSize;
    // MDP dpStream
    DpBlitStream* mpDpStream = NULL;
};


}; //NSFeaturePipe_DepthMap_BM
}; //NSCamFeature
}; //NSCam

#endif