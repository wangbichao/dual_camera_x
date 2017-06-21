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

#include "MtkHeader.h"
//#include <mtkcam/feature/featurePipe/IStreamingFeaturePipe.h>

#include <featurePipe/core/include/DebugUtil.h>

#include "StreamingFeature_Common.h"

#include "DebugControl.h"
#define PIPE_CLASS_TAG "Util"
#define PIPE_TRACE TRACE_STREAMING_FEATURE_COMMON
#include <featurePipe/core/include/PipeLog.h>

namespace NSCam {
namespace NSCamFeature {
namespace NSFeaturePipe {

MBOOL useMDPHardware()
{
    return MTRUE;
}

MBOOL isDisplayOutput(const NSCam::NSIoPipe::NSPostProc::Output &output)
{
    return output.mPortID.capbility == NSCam::NSIoPipe::EPortCapbility_Disp;
}

MBOOL isRecordOutput(const NSCam::NSIoPipe::NSPostProc::Output &output)
{
    return (output.mPortID.capbility == NSCam::NSIoPipe::EPortCapbility_Rcrd) &&
           (getGraphicBufferAddr(output.mBuffer) != NULL );
}

IImageBuffer* findOutBuffer(const NSCam::NSIoPipe::NSPostProc::QParams &qparam, unsigned skip)
{
    IImageBuffer *buffer = NULL;
    for( unsigned i = 0, count = qparam.mvOut.size(); i < count; ++i )
    {
        unsigned index = qparam.mvOut[i].mPortID.index;
        if( index == NSImageio::NSIspio::EPortIndex_WROTO ||
            index == NSImageio::NSIspio::EPortIndex_WDMAO )
        {
            if( skip == 0 )
            {
                buffer = qparam.mvOut[i].mBuffer;
                break;
            }
            --skip;
        }
    }
    return buffer;
}

MBOOL toDpColorFormat(const NSCam::EImageFormat fmt, DpColorFormat &dpFmt)
{
    TRACE_FUNC_ENTER();
    MBOOL ret = MTRUE;
    switch( fmt )
    {
    case eImgFmt_YUY2:    dpFmt = DP_COLOR_YUYV;      break;
    case eImgFmt_UYVY:    dpFmt = DP_COLOR_UYVY;      break;
    case eImgFmt_YVYU:    dpFmt = DP_COLOR_YVYU;      break;
    case eImgFmt_VYUY:    dpFmt = DP_COLOR_VYUY;      break;
    case eImgFmt_NV16:    dpFmt = DP_COLOR_NV16;      break;
    case eImgFmt_NV61:    dpFmt = DP_COLOR_NV61;      break;
    case eImgFmt_NV21:    dpFmt = DP_COLOR_NV21;      break;
    case eImgFmt_NV12:    dpFmt = DP_COLOR_NV12;      break;
    case eImgFmt_YV16:    dpFmt = DP_COLOR_YV16;      break;
    case eImgFmt_I422:    dpFmt = DP_COLOR_I422;      break;
    case eImgFmt_YV12:    dpFmt = DP_COLOR_YV12;      break;
    case eImgFmt_I420:    dpFmt = DP_COLOR_I420;      break;
    case eImgFmt_Y800:    dpFmt = DP_COLOR_GREY;      break;
    case eImgFmt_RGB565:  dpFmt = DP_COLOR_RGB565;    break;
    case eImgFmt_RGB888:  dpFmt = DP_COLOR_RGB888;    break;
    case eImgFmt_ARGB888: dpFmt = DP_COLOR_ARGB8888;  break;
    default:
        MY_LOGE("fmt(0x%x) not support in DP", fmt);
        ret = MFALSE;
        break;
    }
    TRACE_FUNC_EXIT();
    return ret;
}

MBOOL copyImageBuffer(IImageBuffer *src, IImageBuffer *dst)
{
    TRACE_FUNC_ENTER();
    MBOOL ret = MTRUE;

    if( !src || !dst )
    {
        MY_LOGE("Invalid buffers src=%p dst=%p", src, dst);
        ret = MFALSE;
    }
    else if( src->getImgSize() != dst->getImgSize() )
    {
        MY_LOGE("Mismatch buffer size src(%dx%d) dst(%dx%d)",
                src->getImgSize().w, src->getImgSize().h,
                dst->getImgSize().w, dst->getImgSize().h);
        ret = MFALSE;
    }
    else
    {
        unsigned srcPlane = src->getPlaneCount();
        unsigned dstPlane = dst->getPlaneCount();

        if( !srcPlane || !dstPlane ||
            (srcPlane != dstPlane && srcPlane != 1 && dstPlane != 1) )
        {
            MY_LOGE("Mismatch buffer plane src(%d) dst(%d)", srcPlane, dstPlane);
            ret = MFALSE;
        }
        for( unsigned i = 0; i < srcPlane; ++i )
        {
            if( !src->getBufVA(i) )
            {
                MY_LOGE("Invalid src plane[%d] VA", i);
                ret = MFALSE;
            }
        }
        for( unsigned i = 0; i < dstPlane; ++i )
        {
            if( !dst->getBufVA(i) )
            {
                MY_LOGE("Invalid dst plane[%d] VA", i);
                ret = MFALSE;
            }
        }

        if( srcPlane == 1 )
        {
            MY_LOGD("src: plane=1 size=%zu stride=%zu",
                    src->getBufSizeInBytes(0), src->getBufStridesInBytes(0));
            ret = MFALSE;
        }
        if( dstPlane == 1 )
        {
            MY_LOGD("dst: plane=1 size=%zu stride=%zu",
                    dst->getBufSizeInBytes(0), dst->getBufStridesInBytes(0));
            ret = MFALSE;
        }

        if( ret )
        {
            char *srcVA = NULL, *dstVA = NULL;
            size_t srcSize = 0;
            size_t dstSize = 0;
            size_t srcStride = 0;
            size_t dstStride = 0;

            for( unsigned i = 0; i < srcPlane && i < dstPlane; ++i )
            {
                if( i < srcPlane )
                {
                    srcVA = (char*)src->getBufVA(i);
                }
                if( i < dstPlane )
                {
                    dstVA = (char*)dst->getBufVA(i);
                }

                srcSize = src->getBufSizeInBytes(i);
                dstSize = dst->getBufSizeInBytes(i);
                srcStride = src->getBufStridesInBytes(i);
                dstStride = dst->getBufStridesInBytes(i);
                MY_LOGD("plane[%d] memcpy %p(%zu)=>%p(%zu)",
                          i, srcVA, srcSize, dstVA, dstSize);
                if( srcStride == dstStride )
                {
                    memcpy((void*)dstVA, (void*)srcVA, (srcSize <= dstSize) ? srcSize : dstSize );
                }
                else
                {
                    MY_LOGD("Stride: src(%d) dst(%d)", srcStride, dstStride);
                    size_t stride = (srcStride < dstStride) ? srcStride : dstStride;
                    unsigned height = dstSize / dstStride;
                    for( unsigned j = 0; j < height; ++j )
                    {
                        memcpy((void*)dstVA, (void*)srcVA, stride);
                        srcVA += srcStride;
                        dstVA += dstStride;
                    }
                }
            }
        }
    }

    TRACE_FUNC_EXIT();
    return ret;
}

sp<GraphicBuffer>* getGraphicBufferAddr(IImageBuffer *imageBuffer)
{
    TRACE_FUNC_ENTER();
    void *addr = NULL;
    if( !imageBuffer )
    {
        MY_LOGE("Invalid imageBuffer");
    }
    else if( !imageBuffer->getImageBufferHeap() )
    {
        MY_LOGW("Cannot get imageBufferHeap");
    }
    else
    {
        addr = imageBuffer->getImageBufferHeap()->getGraphicBuffer();
        // MY_LOGV_IF(!addr, "Cannot get graphic buffer addr");
    }
    TRACE_FUNC_EXIT();
    return (sp<GraphicBuffer>*)addr;
}

MSize calcDsImgSize(const MSize &src)
{
    TRACE_FUNC_ENTER();
    MSize result;

    if( src.w*3 == src.h*4 )
    {
        result = MSize(320, 24);
    }
    else if( src.w*9 == src.h*16 )
    {
        result = MSize(320, 180);
    }
    else if( src.w*3 == src.h*5 )
    {
        result = MSize(320, 180);
    }
    else
    {
        result = MSize(320, 320*src.h/src.w);
    }

    TRACE_FUNC_EXIT();
    return result;
}

MBOOL dumpToFile(IImageBuffer *buffer, const char *fmt, ...)
{
    MBOOL ret = MFALSE;
    if( buffer && fmt )
    {
        char filename[256];
        va_list arg;
        va_start(arg, fmt);
        vsprintf(filename, fmt, arg);
        va_end(arg);
        buffer->saveToFile(filename);
        ret = MTRUE;
    }
    return ret;
}

MBOOL is4K2K(const MSize &size)
{
    return (size.w >= UHD_VR_WIDTH && size.h >= UHD_VR_HEIGHT);
}

} // namespace NSFeaturePipe
} // namespace NSCamFeature
} // namespace NSCam
