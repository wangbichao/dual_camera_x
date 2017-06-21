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
 * @file NodeBufferHandler.cpp
 * @brief VSDOF buffer handler
 */

// Standard C header file

// Android system/core header file

// mtkcam custom header file

// mtkcam global header file

// Module header file

// Local header file
#include "NodeBufferHandler.h"
// Logging header file
#define PIPE_CLASS_TAG "NodeBufferHandler"
#include <featurePipe/core/include/PipeLog.h>


/*******************************************************************************
* Namespace start.
********************************************************************************/
namespace NSCam {
namespace NSCamFeature {
namespace NSFeaturePipe_DepthMap_BM {

/*******************************************************************************
* Global Define
********************************************************************************/

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  Instantiation.
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
NodeBufferHandler::
NodeBufferHandler(
    sp<BaseBufferPoolMgr> pPoopMgr,
    const RequestBufferMap& reqBufferMap,
    const RequestBufferMap& metaBufferMap
)
: BaseBufferHandler(pPoopMgr)
, mRequestBufferIDMap(reqBufferMap)
, mRequestMetaIDMap(metaBufferMap)
{
    // init maps
    for(int nodeId=0;nodeId<eDPETHMAP_PIPE_NODE_SIZE;++nodeId)
    {
        DepthMapPipeNodeID id = static_cast<DepthMapPipeNodeID>(nodeId);
        // init the enque buffer map
        mEnqueBufferMap.add(id, BIDToSmartBufferMap());
        mEnqueGraphicBufferMap.add(id, BIDToGraphicBufferMap());
        mEnqueTuningBufferMap.add(id, BIDToTuningBufferMap());
    }
}

NodeBufferHandler::
~NodeBufferHandler()
{
    VSDOF_LOGD("[Destructo] +");
    for(ssize_t index=0;index<mEnqueBufferMap.size();++index)
    {
        BIDToSmartBufferMap map = mEnqueBufferMap.valueAt(index);
        map.clear();
    }
    for(ssize_t index=0;index<mEnqueGraphicBufferMap.size();++index)
    {
        BIDToGraphicBufferMap map = mEnqueGraphicBufferMap.valueAt(index);
        map.clear();
    }
    for(ssize_t index=0;index<mEnqueTuningBufferMap.size();++index)
    {
        BIDToTuningBufferMap map = mEnqueTuningBufferMap.valueAt(index);
        map.clear();
    }

    mEnqueBufferMap.clear();
    mEnqueGraphicBufferMap.clear();
    mEnqueTuningBufferMap.clear();
    mpDepthMapEffectReq = NULL;
    mpBufferPoolMgr = NULL;
    VSDOF_LOGD("[Destructo] -");
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  BaseBufferPoolMgr Public Operations.
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
MBOOL
NodeBufferHandler::
configRequest(
    DepthMapEffectRequest* pDepthReq
)
{
    mpDepthMapEffectReq = pDepthReq;
    mReqAttr = pDepthReq->getRequestAttr();

    // get the request buffer id map of the current request
    mActiveReqBufferIDMap = mRequestBufferIDMap.valueFor(mReqAttr.bufferScenario);
    mActiveReqMetaIDMap = mRequestMetaIDMap.valueFor(mReqAttr.bufferScenario);

    return MTRUE;
}

IImageBuffer*
NodeBufferHandler::
requestBuffer(
    DepthMapPipeNodeID srcNodeID,
    DepthMapBufferID bufferID
)
{
    VSDOF_LOGD("+  srcNodeID=%d bufferID=%d", srcNodeID, bufferID);
    IImageBuffer* pOutImgBuf = NULL;
    // check buffer is inside the request
    if(mActiveReqBufferIDMap.indexOfKey(bufferID) >= 0)
    {
        VSDOF_LOGD("inside request, srcNodeID=%d bufferID=%d", srcNodeID, bufferID);
        DepthNodeBufIOType iotype = mActiveReqBufferIDMap.valueFor(bufferID);
        mpDepthMapEffectReq->getRequestImageBuffer({.bufferID=bufferID, .ioType=iotype}, pOutImgBuf);
        return pOutImgBuf;
    }
    else
    {
        VSDOF_LOGD("request buffer mgr, srcNodeID=%d bufferID=%d", srcNodeID, bufferID);
        SmartImageBuffer smImgBuf = mpBufferPoolMgr->request(bufferID, mReqAttr.bufferScenario);
        if(smImgBuf.get() != NULL)
        {
            addEnquedBuffer(srcNodeID, bufferID, smImgBuf);
            return smImgBuf->mImageBuffer.get();
        }
        SmartGraphicBuffer smGraBuf = mpBufferPoolMgr->requestGB(bufferID, mReqAttr.bufferScenario);
        if(smGraBuf.get() != NULL)
        {
            addEnquedBuffer(srcNodeID, bufferID, smGraBuf);
            return smGraBuf->mImageBuffer.get();
        }

        return NULL;
    }
}

MBOOL
NodeBufferHandler::
addEnquedBuffer(
    DepthMapPipeNodeID srcNodeID,
    DepthMapBufferID bufferID,
    SmartImageBuffer smImgBuf
)
{
    RWLock::AutoWLock _l(mEnqueRWLock[eBUFFER_IMAGE]);
    BIDToSmartBufferMap& bidBufMap = mEnqueBufferMap.editValueFor(srcNodeID);
    bidBufMap.add(bufferID, smImgBuf);

    return MTRUE;
}

MBOOL
NodeBufferHandler::
addEnquedBuffer(
    DepthMapPipeNodeID srcNodeID,
    DepthMapBufferID bufferID,
    SmartGraphicBuffer smGraBuf
)
{
    RWLock::AutoWLock _l(mEnqueRWLock[eBUFFER_GRAPHIC]);
    BIDToGraphicBufferMap& bidGraBufMap = mEnqueGraphicBufferMap.editValueFor(srcNodeID);
    bidGraBufMap.add(bufferID, smGraBuf);

    return MTRUE;
}

MBOOL
NodeBufferHandler::
addEnquedBuffer(
    DepthMapPipeNodeID srcNodeID,
    DepthMapBufferID bufferID,
    SmartTuningBuffer smTuningBuf
)
{
    RWLock::AutoWLock _l(mEnqueRWLock[eBUFFER_TUNING]);
    BIDToTuningBufferMap& bidTuningBufMap = mEnqueTuningBufferMap.editValueFor(srcNodeID);
    bidTuningBufMap.add(bufferID, smTuningBuf);

    return MTRUE;
}

IMetadata*
NodeBufferHandler::
requestMetadata(
    DepthMapPipeNodeID srcNodeID,
    DepthMapBufferID bufferID)
{
    IMetadata* pOutMeta;
    // check buffer is inside the request
    if(mActiveReqMetaIDMap.indexOfKey(bufferID) >= 0)
    {
        DepthNodeBufIOType ioType = mActiveReqMetaIDMap.valueFor(bufferID);
        mpDepthMapEffectReq->getRequestMetadata({.bufferID=bufferID, .ioType=ioType}, pOutMeta);
        return pOutMeta;
    }
    else
    {
        MY_LOGE("Failed to request metadata !! bufferID:%d scenario:%d", bufferID, mReqAttr.bufferScenario);
        return NULL;
    }
}

MVOID*
NodeBufferHandler::
requestTuningBuf(
    DepthMapPipeNodeID srcNodeID,
    DepthMapBufferID bufferID
)
{
    SmartTuningBuffer smTuningBuf = mpBufferPoolMgr->requestTB(bufferID, mReqAttr.bufferScenario);

    if(smTuningBuf.get() != NULL)
    {
        addEnquedBuffer(srcNodeID, bufferID, smTuningBuf);
        return smTuningBuf->mpVA;
    }

    MY_LOGE("Failed to request tuning buffer!! bufferID:%d scenario:%d", bufferID, mReqAttr.bufferScenario);
    return NULL;
}

MBOOL
NodeBufferHandler::
onProcessDone(DepthMapPipeNodeID nodeID)
{
    // release the enque buffer
    mEnqueBufferMap.editValueFor(nodeID).clear();
    mEnqueGraphicBufferMap.editValueFor(nodeID).clear();
    mEnqueTuningBufferMap.editValueFor(nodeID).clear();
    return MTRUE;
}

MBOOL
NodeBufferHandler::
configOutBuffer(
    DepthMapPipeNodeID srcNodeID,
    DepthMapBufferID bufferID,
    DepthMapPipeNodeID outNodeID
)
{
    DepthBufferType bufferType;
    // check the need to config
    MBOOL bNeed = mpBufferPoolMgr->queryBufferType(bufferID, mReqAttr.bufferScenario, bufferType);
    if(!bNeed)
    {
        return MTRUE;
    }

    if(bufferType == eBUFFER_IMAGE)
    {
        SmartImageBuffer smBuf;
        BIDToSmartBufferMap queuedBufferMap;
        // read the buffer out
        {
            RWLock::AutoRLock _l(mEnqueRWLock[eBUFFER_IMAGE]);
            queuedBufferMap = mEnqueBufferMap.valueFor(srcNodeID);
            ssize_t bufIndex = queuedBufferMap.indexOfKey(bufferID);

            if(bufIndex<0)
            {
                MY_LOGE("Cannot find the buffer to config, id:%d", bufferID);
                return MFALSE;
            }
            smBuf = queuedBufferMap.valueAt(bufIndex);
        }

        this->addOutputBuffer(outNodeID, bufferID, smBuf);
        return MTRUE;
    }
    else if(bufferType == eBUFFER_GRAPHIC)
    {
        SmartGraphicBuffer smBuf;
        BIDToGraphicBufferMap queuedBufferMap;
        // read the buffer out
        {
            RWLock::AutoRLock _l(mEnqueRWLock[eBUFFER_GRAPHIC]);
            queuedBufferMap = mEnqueGraphicBufferMap.valueFor(srcNodeID);
            ssize_t bufIndex = queuedBufferMap.indexOfKey(bufferID);

            if(bufIndex<0)
            {
                MY_LOGE("Cannot find the graphic buffer to config, id:%d", bufferID);
                return MFALSE;
            }
            smBuf = queuedBufferMap.valueAt(bufIndex);
        }

        this->addOutputBuffer(outNodeID, bufferID, smBuf);
        return MTRUE;
    }
    else if(bufferType == eBUFFER_TUNING)
    {
        SmartTuningBuffer smBuf;
        BIDToTuningBufferMap queuedBufferMap;
        // read the buffer out
        {
            RWLock::AutoRLock _l(mEnqueRWLock[eBUFFER_TUNING]);
            queuedBufferMap = mEnqueTuningBufferMap.valueFor(srcNodeID);
            ssize_t bufIndex = queuedBufferMap.indexOfKey(bufferID);
            if(bufIndex<0)
            {
                MY_LOGE("Cannot find the tuning buffer to config, id:%d", bufferID);
                return MFALSE;
            }
            smBuf = queuedBufferMap.valueAt(bufIndex);
        }
        this->addOutputBuffer(outNodeID, bufferID, smBuf);
        return MTRUE;
    }

    MY_LOGE("Undefined bufferType:%d", bufferType);
    return MFALSE;
}

MBOOL
NodeBufferHandler::
addOutputBuffer(
    DepthMapPipeNodeID outNodeID,
    DepthMapBufferID bufferID,
    SmartImageBuffer smImgBuf
)
{
    RWLock::AutoWLock _l(mEnqueRWLock[eBUFFER_IMAGE]);
    BIDToSmartBufferMap& bidBufMap = mEnqueBufferMap.editValueFor(outNodeID);
    bidBufMap.add(bufferID, smImgBuf);

    return MTRUE;
}

MBOOL
NodeBufferHandler::
addOutputBuffer(
    DepthMapPipeNodeID outNodeID,
    DepthMapBufferID bufferID,
    SmartGraphicBuffer smGraBuf
)
{
    RWLock::AutoWLock _l(mEnqueRWLock[eBUFFER_GRAPHIC]);
    BIDToGraphicBufferMap& bidGraBufMap = mEnqueGraphicBufferMap.editValueFor(outNodeID);
    bidGraBufMap.add(bufferID, smGraBuf);

    return MTRUE;
}

MBOOL
NodeBufferHandler::
addOutputBuffer(
    DepthMapPipeNodeID outNodeID,
    DepthMapBufferID bufferID,
    SmartTuningBuffer smTuningBuf
)
{
    RWLock::AutoWLock _l(mEnqueRWLock[eBUFFER_TUNING]);
    BIDToTuningBufferMap& bidTuningBufMap = mEnqueTuningBufferMap.editValueFor(outNodeID);
    bidTuningBufMap.add(bufferID, smTuningBuf);

    return MTRUE;
}

MBOOL
NodeBufferHandler::
getEnqueBuffer(
    DepthMapPipeNodeID srcNodeID,
    DepthMapBufferID bufferID,
    IImageBuffer*& pImgBuf
)
{
    // check buffer is inside the request
    if(mActiveReqBufferIDMap.indexOfKey(bufferID) >= 0)
    {
        DepthNodeBufIOType ioType = mActiveReqBufferIDMap.valueFor(bufferID);
        mpDepthMapEffectReq->getRequestImageBuffer({.bufferID=bufferID, .ioType=ioType}, pImgBuf);
        return MTRUE;
    }
    else
    {
        int index;
        const BIDToSmartBufferMap& bidToBufMap = mEnqueBufferMap.valueFor(srcNodeID);
        if((index=bidToBufMap.indexOfKey(bufferID))>=0)
        {
            SmartImageBuffer smImgBuf = bidToBufMap.valueAt(index);
            pImgBuf = smImgBuf->mImageBuffer.get();
            return MTRUE;
        }

        const BIDToGraphicBufferMap& bidToGraBufMap = mEnqueGraphicBufferMap.valueFor(srcNodeID);
        if((index=bidToGraBufMap.indexOfKey(bufferID))>=0)
        {
            SmartGraphicBuffer smGraBuf = mEnqueGraphicBufferMap.valueFor(srcNodeID).valueAt(index);
            pImgBuf = smGraBuf->mImageBuffer.get();
            return MTRUE;
        }

        MY_LOGE("Cannot get enqued buffer, node id:%d, buffer id:%d", srcNodeID, bufferID);
        return MFALSE;
    }
}

MBOOL
NodeBufferHandler::
getEnquedSmartBuffer(
    DepthMapPipeNodeID srcNodeID,
    DepthMapBufferID bufferID,
    SmartImageBuffer& pSmImgBuf
)
{
    ssize_t index;
    const BIDToSmartBufferMap& bidToBufMap = mEnqueBufferMap.valueFor(srcNodeID);
    if((index=bidToBufMap.indexOfKey(bufferID))>=0)
    {
        pSmImgBuf = bidToBufMap.valueAt(index);
        return MTRUE;
    }
    return MFALSE;
}

MBOOL
NodeBufferHandler::
getEnquedSmartBuffer(
    DepthMapPipeNodeID srcNodeID,
    DepthMapBufferID bufferID,
    SmartGraphicBuffer& pSmGraBuf
)
{
    ssize_t index;
    const BIDToGraphicBufferMap& bidToBufMap = mEnqueGraphicBufferMap.valueFor(srcNodeID);
    if((index=bidToBufMap.indexOfKey(bufferID))>=0)
    {
        pSmGraBuf = bidToBufMap.valueAt(index);
        return MTRUE;
    }
    return MFALSE;
}

MBOOL
NodeBufferHandler::
getEnquedSmartBuffer(
    DepthMapPipeNodeID srcNodeID,
    DepthMapBufferID bufferID,
    SmartTuningBuffer& pSmTuningBuf
)
{
    ssize_t index;
    const BIDToTuningBufferMap& bidToBufMap = mEnqueTuningBufferMap.valueFor(srcNodeID);
    if((index=bidToBufMap.indexOfKey(bufferID))>=0)
    {
        pSmTuningBuf = bidToBufMap.valueAt(index);
        return MTRUE;
    }
    return MFALSE;
}

}; // NSFeaturePipe_DepthMap_BM
}; // NSCamFeature
}; // NSCam
