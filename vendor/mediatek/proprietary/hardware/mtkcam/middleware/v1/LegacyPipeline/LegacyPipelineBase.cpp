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

#define LOG_TAG "MtkCam/LPipeline"

//
#include "MyUtils.h"
#include "PipelineBuilderBase.h"
#include "LegacyPipelineBase.h"

#include <mtkcam/pipeline/utils/streambuf/StreamBuffers.h>
#include <mtkcam/utils/metadata/hal/mtk_platform_metadata_tag.h>

using namespace android;
using namespace NSCam;
using namespace NSCam::Utils;
using namespace NSCam::v3;
using namespace NSCam::v3::NSPipelineContext;
using namespace NSCam::v1;
using namespace NSCam::v1::NSLegacyPipeline;

typedef NSCam::v3::Utils::HalMetaStreamBuffer::Allocator  HalMetaStreamBufferAllocatorT;

/******************************************************************************
*
*******************************************************************************/
#define MY_LOGV(fmt, arg...)        CAM_LOGV("[%s] " fmt,  __FUNCTION__, ##arg)
#define MY_LOGD(fmt, arg...)        CAM_LOGD("[%s] " fmt,  __FUNCTION__, ##arg)
#define MY_LOGI(fmt, arg...)        CAM_LOGI("[%s] " fmt,  __FUNCTION__, ##arg)
#define MY_LOGW(fmt, arg...)        CAM_LOGW("[%s] " fmt,  __FUNCTION__, ##arg)
#define MY_LOGE(fmt, arg...)        CAM_LOGE("[%s] " fmt,  __FUNCTION__, ##arg)
#define MY_LOGA(fmt, arg...)        CAM_LOGA("[%s] " fmt,  __FUNCTION__, ##arg)
#define MY_LOGF(fmt, arg...)        CAM_LOGF("[%s] " fmt,  __FUNCTION__, ##arg)
//
#define MY_LOGV_IF(cond, ...)       do { if ( (cond) ) { MY_LOGV(__VA_ARGS__); } }while(0)
#define MY_LOGD_IF(cond, ...)       do { if ( (cond) ) { MY_LOGD(__VA_ARGS__); } }while(0)
#define MY_LOGI_IF(cond, ...)       do { if ( (cond) ) { MY_LOGI(__VA_ARGS__); } }while(0)
#define MY_LOGW_IF(cond, ...)       do { if ( (cond) ) { MY_LOGW(__VA_ARGS__); } }while(0)
#define MY_LOGE_IF(cond, ...)       do { if ( (cond) ) { MY_LOGE(__VA_ARGS__); } }while(0)
#define MY_LOGA_IF(cond, ...)       do { if ( (cond) ) { MY_LOGA(__VA_ARGS__); } }while(0)
#define MY_LOGF_IF(cond, ...)       do { if ( (cond) ) { MY_LOGF(__VA_ARGS__); } }while(0)
//
#define FUNC_START                  MY_LOGD("+")
#define FUNC_END                    MY_LOGD("-")
//

/******************************************************************************
*
*******************************************************************************/
LegacyPipelineBase::
LegacyPipelineBase()
    : mpPipelineContext(NULL)
    , mpResultProcessor(NULL)
    , mpTimestampProcessor(NULL)
    , mOpenId(-1)
{
    mLogLevel = property_get_int32("debug.camera.log", 0);
    if ( mLogLevel == 0 ) {
        mLogLevel = property_get_int32("debug.camera.log.LegacyPipeline", 0);
    }
    mTimestamp = TimeTool::getReadableTime();
}

/******************************************************************************
*
*******************************************************************************/
MERROR
LegacyPipelineBase::
flush()
{
    FUNC_START;
    Mutex::Autolock _l(mLock);
    MERROR err = OK;
    if ( mpPipelineContext != 0 ){
        mpPipelineContext->flush();
        mpPipelineContext->waitUntilDrained();
        mpPipelineContext = NULL;
    }
    if( mpResultProcessor != NULL )
    {
        mpResultProcessor->flush();
        mpResultProcessor = NULL;
    }
    //
    mpTimestampProcessor = NULL;
    mpRequestBuilder     = NULL;
    //
    int n = vStreamBufferProvider.size();
    for(int i=0; i<n; i++)
    {
        vStreamBufferProvider.editValueAt(i)->flush();
    }
    //
    vStreamBufferProvider.clear();
    vImageStreamInfo.clear();

    FUNC_END;
    return err;
}

/******************************************************************************
*
*******************************************************************************/
MVOID
LegacyPipelineBase::
onLastStrongRef( const void* /*id*/)
{
    FUNC_START;

    flush();

    FUNC_END;
}

/******************************************************************************
*
*******************************************************************************/
MVOID
LegacyPipelineBase::
setStreamBufferProvider(
    sp<StreamBufferProvider> const pStreamBufferProvider
)
{
    if ( pStreamBufferProvider == 0 ) {
        MY_LOGE("NULL StreamBufferProvider");
        return;
    }

    vStreamBufferProvider.add(pStreamBufferProvider->queryImageStreamInfo()->getStreamId(), pStreamBufferProvider);
}

/******************************************************************************
*
*******************************************************************************/
MVOID
LegacyPipelineBase::
setImageStreamInfo(
    sp<IImageStreamInfo> const pImageStreamInfo
)
{
    if ( pImageStreamInfo == 0 ) {
        MY_LOGE("NULL ImageStreamInfo");
        return;
    }

    vImageStreamInfo.add(pImageStreamInfo->getStreamId(), pImageStreamInfo);
}

/******************************************************************************
*
*******************************************************************************/
MVOID
LegacyPipelineBase::
setMetaStreamInfo(
    sp<IMetaStreamInfo> const pMetaStreamInfo
)
{
    if ( pMetaStreamInfo == 0 ) {
        MY_LOGE("NULL MetaStreamInfo");
        return;
    }

    vMetaStreamInfo.add(pMetaStreamInfo->getStreamId(), pMetaStreamInfo);
}
