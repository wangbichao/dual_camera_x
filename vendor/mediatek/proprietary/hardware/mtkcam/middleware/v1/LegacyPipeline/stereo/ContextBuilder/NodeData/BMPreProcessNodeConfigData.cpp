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
#define LOG_TAG "ContextBuilder/BMPreProcessNodeData"
//
#include <mtkcam/utils/std/Log.h>
#include <utils/StrongPointer.h>
//
#include <mtkcam/middleware/v1/LegacyPipeline/stereo/ContextBuilder/MetaStreamManager.h>
#include <mtkcam/middleware/v1/LegacyPipeline/stereo/ContextBuilder/ImageStreamManager.h>
#include <mtkcam/middleware/v1/LegacyPipeline/stereo/ContextBuilder/StereoBasicParameters.h>
//
#include <mtkcam/pipeline/pipeline/PipelineContext.h>
//
#include <mtkcam/pipeline/hwnode/BMPreProcessNode.h>
//
#include <mtkcam/middleware/v1/LegacyPipeline/StreamId.h>
//
#include "BMPreProcessNodeConfigData.h"
//
/******************************************************************************
 *
 ******************************************************************************/
#define MY_LOGV(fmt, arg...)        CAM_LOGV("(%d)[%s] " fmt, ::gettid(), __FUNCTION__, ##arg)
#define MY_LOGD(fmt, arg...)        CAM_LOGD("(%d)[%s] " fmt, ::gettid(), __FUNCTION__, ##arg)
#define MY_LOGI(fmt, arg...)        CAM_LOGI("(%d)[%s] " fmt, ::gettid(), __FUNCTION__, ##arg)
#define MY_LOGW(fmt, arg...)        CAM_LOGW("(%d)[%s] " fmt, ::gettid(), __FUNCTION__, ##arg)
#define MY_LOGE(fmt, arg...)        CAM_LOGE("(%d)[%s] " fmt, ::gettid(), __FUNCTION__, ##arg)
//
#define MY_LOGV_IF(cond, ...)       do { if ( (cond) ) { MY_LOGV(__VA_ARGS__); } }while(0)
#define MY_LOGD_IF(cond, ...)       do { if ( (cond) ) { MY_LOGD(__VA_ARGS__); } }while(0)
#define MY_LOGI_IF(cond, ...)       do { if ( (cond) ) { MY_LOGI(__VA_ARGS__); } }while(0)
#define MY_LOGW_IF(cond, ...)       do { if ( (cond) ) { MY_LOGW(__VA_ARGS__); } }while(0)
#define MY_LOGE_IF(cond, ...)       do { if ( (cond) ) { MY_LOGE(__VA_ARGS__); } }while(0)
//
using namespace android;
using namespace NSCam;
using namespace v3;
using namespace NSCam::v3::NSPipelineContext;
//
typedef BMPreProcessNode          NodeT;
typedef NodeActor< NodeT >     MyNodeActorT;
//
BMPreProcessNodeConfigData::
BMPreProcessNodeConfigData(
    MINT32 openId,
    NodeId_T nodeId,
    char const* nodeName)
    : INodeConfigBase(openId, nodeId, nodeName)
{
    initParam.openId = mOpenId;
    initParam.nodeId = mNodeId;
    initParam.nodeName = mNodeName;
}
//
BMPreProcessNodeConfigData::
~BMPreProcessNodeConfigData()
{
    MY_LOGD("dcot(%x)", this);
}
//
sp<INodeActor>
BMPreProcessNodeConfigData::
getNode()
{
    MY_LOGD("+");
    sp<MyNodeActorT> pNode = new MyNodeActorT( NodeT::createInstance() );
    //
    pNode->setInitParam(initParam);
    pNode->setConfigParam(cfgParam);
    //
    MY_LOGD("-");
    return pNode;
}
//
void
BMPreProcessNodeConfigData::
configNode(
    MetaStreamManager* metaManager,
    ImageStreamManager* imageManager,
    StereoBasicParameters* userParams,
    PipelineContext* pipeContext)
{
    //
    // Get meta stream info
    //
    if(metaManager != NULL)
    {
        addStreamBegin(MTRUE);

        cfgParam.pInAppMeta = metaManager->getStreamInfoByStreamId(eSTREAMID_META_APP_CONTROL);
        addStream(cfgParam.pInAppMeta, MTRUE);

        cfgParam.pOutAppMeta = metaManager->getStreamInfoByStreamId(eSTREAMID_META_APP_DYNAMIC_BMPREPROCESS);
        addStream(cfgParam.pOutAppMeta, MFALSE);

        cfgParam.pInHalMeta = metaManager->getStreamInfoByStreamId(eSTREAMID_META_HAL_DYNAMIC_P1);
        addStream(cfgParam.pInHalMeta, MTRUE);

        cfgParam.pOutHalMeta = metaManager->getStreamInfoByStreamId(eSTREAMID_META_HAL_DYNAMIC_BMPREPROCESS);
        addStream(cfgParam.pOutHalMeta, MFALSE);

        addStreamEnd(MTRUE);
    }
    //
    // Get image stream info
    //
    if(imageManager != NULL)
    {
        addStreamBegin(MFALSE);
        cfgParam.pInFullRaw = imageManager->getStreamInfoByStreamId(eSTREAMID_IMAGE_PIPE_RAW_OPAQUE);
        addStream(cfgParam.pInFullRaw, MTRUE);

        cfgParam.pInFullRawMain2 = imageManager->getStreamInfoByStreamId(eSTREAMID_IMAGE_PIPE_RAW_OPAQUE_01);
        addStream(cfgParam.pInFullRawMain2, MTRUE);

        cfgParam.vOutImageMFBO_1 = imageManager->getStreamInfoByStreamId(eSTREAMID_IMAGE_PIPE_BMDENOISE_MFBO_0);
        addStream(cfgParam.vOutImageMFBO_1, MFALSE);

        cfgParam.vOutImageMFBO_2 = imageManager->getStreamInfoByStreamId(eSTREAMID_IMAGE_PIPE_BMDENOISE_MFBO_1);
        addStream(cfgParam.vOutImageMFBO_2, MFALSE);

        cfgParam.vOutImageMFBO_final_1 = imageManager->getStreamInfoByStreamId(eSTREAMID_IMAGE_PIPE_BMDENOISE_MFBO_FINAL_0);
        addStream(cfgParam.vOutImageMFBO_final_1, MFALSE);

        cfgParam.vOutImageMFBO_final_2 = imageManager->getStreamInfoByStreamId(eSTREAMID_IMAGE_PIPE_BMDENOISE_MFBO_FINAL_1);
        addStream(cfgParam.vOutImageMFBO_final_2, MFALSE);

        cfgParam.vOutImageW_1 = imageManager->getStreamInfoByStreamId(eSTREAMID_IMAGE_PIPE_BMDENOISE_W_0);
        addStream(cfgParam.vOutImageW_1, MFALSE);

        cfgParam.vOutImageW_2 = imageManager->getStreamInfoByStreamId(eSTREAMID_IMAGE_PIPE_BMDENOISE_W_1);
        addStream(cfgParam.vOutImageW_2, MFALSE);

        addStreamEnd(MFALSE);
    }
}
//
void
BMPreProcessNodeConfigData::
dump()
{

}
//
void
BMPreProcessNodeConfigData::
destroy()
{
    cfgParam.pInAppMeta  = NULL;
    cfgParam.pInHalMeta  = NULL;
    cfgParam.pOutAppMeta = NULL;
    cfgParam.pOutHalMeta = NULL;
    cfgParam.pInFullRaw = NULL;
    cfgParam.pInFullRawMain2 = NULL;
    cfgParam.vOutImageMFBO_1 = NULL;
    cfgParam.vOutImageMFBO_2 = NULL;
    cfgParam.vOutImageMFBO_final_1 = NULL;
    cfgParam.vOutImageMFBO_final_2 = NULL;
    cfgParam.vOutImageW_1 = NULL;
    cfgParam.vOutImageW_2 = NULL;

    mvInStreamSet.clear();
    mvOutStreamSet.clear();
    mvStreamUsageSet.clear();
}