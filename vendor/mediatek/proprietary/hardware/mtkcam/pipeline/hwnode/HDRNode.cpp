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
 * MediaTek Inc. (C) 2015. All rights reserved.
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

#define LOG_TAG "MtkCam/HDRNode"
#define DEBUG_LOG_TAG LOG_TAG

#include "BaseNode.h"
#include "hwnode_utilities.h"
#include <mtkcam/pipeline/hwnode/HDRNode.h>
#include <MFCNodeImp.h>

#include <utils/RWLock.h>
#include <utils/Thread.h>

#include <sys/prctl.h>
#include <sys/resource.h>

#include <mtkcam/drv/iopipe/PostProc/INormalStream.h>
#include <mtkcam/drv/iopipe/SImager/IImageTransform.h>

#include <mtkcam/utils/metastore/IMetadataProvider.h>
#include <mtkcam/utils/metadata/client/mtk_metadata_tag.h>
#include <mtkcam/utils/metadata/hal/mtk_platform_metadata_tag.h>

#include <mtkcam/drv/IHalSensor.h>

#include <mtkcam/aaa/aaa_hal_common.h>

#include "../camera_custom_AEPlinetable.h"

#include <debug_exif/cam/dbg_cam_param.h>
#include <mtkcam/utils/exif/DebugExifUtils.h>

#include <cutils/properties.h>

#include <vector>

#include <mtkcam/feature/hdr/IHDRProc2.h>
#include <mtkcam/feature/hdr/utils/Debug.h>
#include <mtkcam/feature/utils/ImageBufferUtils.h>

#define P2THREAD_NAME_ENQUE "Cam@HDRNode"
#define P2THREAD_NAME_HDR   "Cam@HDRProc"

// round-robin time-sharing policy
#define P2THREAD_POLICY     SCHED_OTHER

// most threads run at normal priority
#define P2THREAD_PRIORITY   ANDROID_PRIORITY_NORMAL

#define ALIGN_FLOOR(x,a)    ((x) & ~((a) - 1L))
#define ALIGN_CEIL(x,a)     (((x) + (a) - 1L) & ~((a) - 1L))

using namespace std;
using namespace android;
using namespace NSCam;
using namespace NSCam::v3;
using namespace NSCam::Utils;
using namespace NS3Av3;
using namespace NSIoPipe;
using namespace NSIoPipe::NSPostProc;
using namespace NSCam::NSIoPipe::NSSImager;

// ---------------------------------------------------------------------------
// TODO: move this part into HDRNodeImp
static Mutex gHdrHandleLock;
static HDRHandle *gHdrHandle = NULL;

// ---------------------------------------------------------------------------

class HDRNodeImp
    : public BaseNode
    , public HDRNode
{
public:
    HDRNodeImp();
    virtual ~HDRNodeImp();

    // IPipelineNode Interface
    MERROR init(const InitParams& rParams);
    MERROR uninit();

    // tell the compiler we want both the fluch from BaseNode and ours
    using  BaseNode::flush;
    MERROR flush();
    MERROR config(const ConfigParams& rParams);
    MERROR queue(sp<IPipelineFrame> pFrame);

    MINT32 inline getHDRInputCount() const { return mHDRHandle.inputFrameCount; };
    MBOOL  inline isSingleFrame() const { return mUseSingleFrame; };

private:
    typedef List< sp<IPipelineFrame> > Que_T;
    typedef Vector< sp<IMetaStreamInfo> >  MetaStreamInfoSetT;
    typedef Vector< sp<IImageStreamInfo> > ImageStreamInfoSetT;

    mutable RWLock        mConfigRWLock;
    sp<IMetaStreamInfo>   mpInAppMeta_Request;
    sp<IMetaStreamInfo>   mpInHalMeta_P1;
    sp<IMetaStreamInfo>   mpOutAppMeta_Result;
    sp<IMetaStreamInfo>   mpOutHalMeta_Result;
    sp<IImageStreamInfo>  mpInFullRaw;
    sp<IImageStreamInfo>  mpInResizedRaw;
    sp<IImageStreamInfo>  mpInLcsoRaw;
    sp<IImageStreamInfo>  mpOutYuvJpeg;
    sp<IImageStreamInfo>  mpOutYuvThumbnail;
    ImageStreamInfoSetT   mvOutImages;

    mutable Mutex         mRequestQueueLock;
    Condition             mRequestQueueCond;
    Que_T                 mRequestQueue;
    MBOOL                 mbRequestDrained;
    mutable Condition     mbRequestDrainedCond;
    MBOOL                 mbRequestExit;
    // sync with HDRProc Thread
    mutable Mutex         mMetatDataInfoLock;
    Condition             mMetatDataInfoCond;
    mutable Mutex         mHDRInputFrameLock;
    Condition             mHDRInputFrameCond;
    mutable Mutex         mHDRProcCompleteLock;
    Condition             mHDRProcCompleteCond;
    mutable Mutex         mHDRProcCompleteLock2;
    Condition             mHDRProcCompleteCond2;
    mutable Mutex         mP2Lock;
    Condition             mP2Cond;

    struct FrameInput
    {
        PortID                  portId;

        StreamId_T              streamId;

        sp<IImageStreamBuffer>  pStreamBuffer;
        sp<IImageBufferHeap>    pImageBufferHeap;
        sp<IImageBuffer>        pImageBuffer;
    };

    struct FrameOutput
    {
        //PortID                  portId;

        StreamId_T              streamId;
        MINT32                  transform;

        sp<IImageStreamBuffer>  pStreamBuffer;
        sp<IImageBufferHeap>    pImageBufferHeap;
        sp<IImageBuffer>        pImageBuffer;
    };

    struct FrameParams
    {
        sp<IPipelineFrame>      pFrame;
        MINT32                  uniqueKey;
        Vector<FrameInput*>     vpIn;   // full_raw, resized_raw, lcso_raw
        Vector<FrameOutput*>    vpOut;  // yuv_jpeg, tb_jpeg, yuv_preview

        //MBOOL                  bResized;

        IMetadata               *pMeta_InApp;
        IMetadata               *pMeta_InHal;
        IMetadata               *pMeta_OutApp;
        IMetadata               *pMeta_OutHal;
        sp<IMetaStreamBuffer>   pMeta_InAppStreamBuffer;
        sp<IMetaStreamBuffer>   pMeta_InHalStreamBuffer;
        sp<IMetaStreamBuffer>   pMeta_OutAppStreamBuffer;
        sp<IMetaStreamBuffer>   pMeta_OutHalStreamBuffer;

        FrameParams()
            : uniqueKey(0)
            , pMeta_InApp(NULL)
            , pMeta_InHal(NULL)
            , pMeta_OutApp(NULL)
            , pMeta_OutHal(NULL)
        {}
    };

    // a handle get from HDRProc, used to communicate with HDR HAL
    HDRHandle mHDRHandle;

    // since HDR needs a burst of shots for post-processing
    // we use this index to indicate these shots
    MINT32 mNodeEnqueIndex;

    // p2 frame params
    Vector<FrameParams>   mvFrameParams;

    // HDRProc result image
    sp<IImageBuffer> mHdrResult;

    // input working buffers for HDR post-processing
    // please note that the index ordering should be
    // {0, 2, 4, ...} for main YUV and {1, 3, 5, ...} for small Y8 frames
    vector< sp<IImageBuffer> > mvHDRInputFrames;

    // when request is incomplete, we need to fall back to single frame flow
    MBOOL mUseSingleFrame;

    class ThreadBase : public Thread
    {
    public:
        ThreadBase(const char* name, const sp<HDRNodeImp>& pNodeImp);

        // ask this object's thread to exit.
        // this function is asynchronous, when the function returns
        // the thread might still be running.
        // of course, this function can be called from a different thread.
        virtual void requestExit() = 0;

    protected:
        char             mThreadName[256];
        sp<HDRNodeImp>   mpNodeImp;

        MERROR threadSetting(const char* threadName);
    };

    class HDRThread : public ThreadBase
    {
    public:
        HDRThread(const char* name, const sp<HDRNodeImp>& pNodeImp);

        // good place to do one-time initializations
        status_t readyToRun() override;
        bool     threadLoop() override;

        void requestExit() override;
    };

    class HDRProcThread : public ThreadBase
    {
    public:
        HDRProcThread(const char* name, const sp<HDRNodeImp>& pNodeImp);

        // good place to do one-time initializations
        virtual status_t readyToRun() override;
        virtual bool     threadLoop() override;

        void requestExit() override;

    private:
        mutable Mutex mRequestExitLock;
        MBOOL         mRequestExit;

        status_t prepareMultiFrameHDR(MSize& size_sensor);
    };

    // threads
    sp<HDRThread>        mpHDRThread;
    sp<HDRProcThread>    mpHDRProcThread;

    // active array
    MRect                mActiveArray;

    // normal stream & 3A HAL
    // CAN ONLY BE MODIFIED via createPipe() and destroyPipe()
    INormalStream* mpPipe;
    IHal3A*        mp3A;

    // log level
    MINT32 mLogLevel;

    // debug dump (0: no dump; 1: dump raw)
    MINT32 mDebugDump;

    // createPipe() is used to create INormalStream and IHal3A
    MBOOL createPipe();

    // destroyPipe() is used to destroy INormalStream and IHal3A
    MVOID destroyPipe();

    // waitForRequestDrained() is used to wait untill
    // 1. the request queue is empty
    // 2. HDR thread is idle
    MVOID waitForRequestDrained() const;

    static MBOOL    HDRProcCompleteCallback(MVOID* user, const sp<IImageBuffer>& hdrResult, MBOOL ret);

    static MVOID    pass2CbFunc(QParams& rParams);
    MVOID           handleDeque(QParams& rParams);

    // getScalerCropRegion() is a helper function that
    // get the scaler crop region from static metadata information
    // and transform the coordinate system from active array to sensor
    MBOOL  getScalerCropRegion(MRect& cropRegion, const MSize& sensorSize) const;

    // StreamControl
    MERROR getInfoIOMapSet(sp<IPipelineFrame> const& pFrame,
            IPipelineFrame::InfoIOMapSet& rIOMapSet,
            MINT32 frameIndex) const;

    inline MBOOL    isStream(
            const sp<IStreamInfo>& streamInfo, StreamId_T streamId) const;
    inline MBOOL    isFullRawLocked(StreamId_T const streamId) const;
    inline MBOOL    isResizedRawLocked(StreamId_T const streamId) const;
    inline MBOOL    isLcsoRawLocked(StreamId_T const streamId) const;
    inline MBOOL    isYuvJpegLocked(StreamId_T const streamId) const;
    inline MBOOL    isYuvThumbnailLocked(StreamId_T const streamId) const;

    MBOOL isInImageStream(StreamId_T const streamId) const;
    MBOOL isInMetaStream(StreamId_T const streamId) const;

    // markImageStream() is a helper function that marks image stream to
    // 1. STREAM_BUFFER_STATUS::WRITE_OK
    // 2. IUsersManager::UserStatus::USED | IUsersManager::UserStatus::RELEASE
    virtual MVOID markImageStream(sp<IPipelineFrame> const& pFrame,
            sp<IImageStreamBuffer> const pStreamBuffer) const;

    // unlockImage() is a helper function that unlocks
    // 1. image stream buffer
    // 2. image buffer
    virtual MVOID unlockImage(sp<IImageStreamBuffer> const& pStreamBuffer,
            sp<IImageBuffer> const& pImageBuffer) const;

    // markMetaStream() is a helper function that marks meta stream to
    // 1. STREAM_BUFFER_STATUS::WRITE_OK
    // 2. IUsersManager::UserStatus::USED | IUsersManager::UserStatus::RELEASE
    virtual MVOID markMetaStream(
            sp<IPipelineFrame> const& pFrame,
            sp<IMetaStreamBuffer> const pStreamBuffer) const;

    // unlockMetadata() is a helper function that unlocks
    // 1. meta stream buffer
    virtual MVOID unlockMetadata(
            sp<IMetaStreamBuffer> const& pStreamBuffer,
            IMetadata* const pMetadata) const;

    // getImageBuffer() is a helper function that
    // 1. gets image buffer heap from streambuffer (with holding a read or write lock)
    // 2. creates imagebuffer from image buffer heap
    // 3. locks imageBuffer
    //
    // return OK if success; otherwise INVALID_OPERATION is returned
    MERROR getImageBuffer(
            MINT32 type, StreamId_T const streamId,
            sp<IImageStreamBuffer>& streamBuffer,
            sp<IImageBufferHeap>& imageBufferHeap,
            sp<IImageBuffer>& imageBuffer);

    // getMetadata() is a helper function that
    // gets meta buffer from streambuffer (with holding a read lock)
    MERROR getMetadata(
            MINT32 type, StreamId_T const streamId,
            sp<IMetaStreamBuffer>& streamBuffer,
            IMetadata*& metadata);

    MERROR onDequeRequest(android::sp<IPipelineFrame>& rpFrame);
    MVOID  onProcessFrame(android::sp<IPipelineFrame> const& pFrame);
    MVOID  onPartialFrameDone(const sp<IPipelineFrame>& frame);
    MERROR verifyConfigParams(ConfigParams const & rParams) const;
    MERROR mapToRequests(
            sp<IPipelineFrame> const& pFrame,
            IPipelineFrame::InfoIOMapSet const& infoIOMapSet);
    MERROR convertRaw2Yuv();

    MERROR processSingleFrame();
    MERROR processMultiFrameHDR();

    MERROR handleDigitalZoom(const sp<IImageBuffer>& captureBuffer);

    MBOOL makeDebugInfo(IMetadata* metadata);
};

// ---------------------------------------------------------------------------

template <typename T>
inline MBOOL tryGetMetadata(IMetadata* pMetadata, MUINT32 const tag, T& rVal)
{
    if (pMetadata == NULL)
    {
        HDR_LOGW("pMetadata is NULL");
        return MFALSE;
    }

    IMetadata::IEntry entry = pMetadata->entryFor(tag);
    if (!entry.isEmpty())
    {
        rVal = entry.itemAt(0, Type2Type<T>());
        return MTRUE;
    }

    return MFALSE;
}

template <typename T>
inline MVOID updateEntry(IMetadata* pMetadata, MUINT32 const tag, T const& val)
{
    if (pMetadata == NULL)
    {
        HDR_LOGW("pMetadata is NULL");
        return;
    }

    IMetadata::IEntry entry(tag);
    entry.push_back(val, Type2Type<T>());
    pMetadata->update(tag, entry);
}

static MRect calCrop(
        MRect const &rSrc, MRect const &rDst,
        MBOOL isCentered = MTRUE, uint32_t ratio = 100)
{
    #define ROUND_TO_2X(x) ((x) & (~0x1))

    MRect rCrop;

    // srcW/srcH < dstW/dstH
    if (rSrc.s.w * rDst.s.h < rDst.s.w * rSrc.s.h)
    {
        rCrop.s.w = rSrc.s.w;
        rCrop.s.h = rSrc.s.w * rDst.s.h / rDst.s.w;
    }
    // srcW/srcH > dstW/dstH
    else if (rSrc.s.w * rDst.s.h > rDst.s.w * rSrc.s.h)
    {
        rCrop.s.w = rSrc.s.h * rDst.s.w / rDst.s.h;
        rCrop.s.h = rSrc.s.h;
    }
    // srcW/srcH == dstW/dstH
    else
    {
        rCrop.s.w = rSrc.s.w;
        rCrop.s.h = rSrc.s.h;
    }

    rCrop.s.w =  ROUND_TO_2X(rCrop.s.w * 100 / ratio);
    rCrop.s.h =  ROUND_TO_2X(rCrop.s.h * 100 / ratio);

    if (isCentered)
    {
        rCrop.p.x = (rSrc.s.w - rCrop.s.w) / 2;
        rCrop.p.y = (rSrc.s.h - rCrop.s.h) / 2;
    }

    #undef ROUND_TO_2X
    return rCrop;
}

static inline void dumpStreamIfExist(const char* str, const sp<IStreamInfo>& streamInfo)
{
    HDR_LOGD_IF(streamInfo.get(), "%s: streamId(%#08" PRIxPTR ") %s",
            str, streamInfo->getStreamId(), streamInfo->getStreamName());
}

static void dumpCaptureParam(
        const CaptureParam_T& captureParam, const char* msg = NULL)
{
    if (msg) HDR_LOGD("%s", msg);

    HDR_LOGD("=================");
    HDR_LOGD("u4ExposureMode(%u)", captureParam.u4ExposureMode);
    HDR_LOGD("u4Eposuretime(%u)", captureParam.u4Eposuretime);
    HDR_LOGD("u4AfeGain(%u)", captureParam.u4AfeGain);
    HDR_LOGD("u4IspGain(%u)", captureParam.u4IspGain);
    HDR_LOGD("u4RealISO(%u)", captureParam.u4RealISO);
    HDR_LOGD("u4FlareGain(%u)", captureParam.u4FlareGain);
    HDR_LOGD("u4FlareOffset(%u)", captureParam.u4FlareOffset);
    HDR_LOGD("i4LightValue_x10(%d)", captureParam.i4LightValue_x10);
}

static MVOID dumpCapPLineTable(const MINT32 aeTableIndex, const strAETable& aeTable)
{
    HDR_LOGD("tableCurrentIndex(%d) eID(%d) u4TotalIndex(%d)",
            aeTableIndex, aeTable.eID, aeTable.u4TotalIndex);
}

const char* getFileFormatName(const MINT format)
{
    switch (format)
    {
        case eImgFmt_YUY2:
            return "yuy2";
        case eImgFmt_YV12:
            return "yv12";
        case eImgFmt_I420:
            return "i420";
        default:
            HDR_LOGE("cannot find format(0x%x)", format);
            return "N/A";
    }
}

// ---------------------------------------------------------------------------

android::sp<HDRNode> HDRNode::createInstance()
{
    return new HDRNodeImp();
}

// ---------------------------------------------------------------------------

MERROR HDRNode::getCaptureParamSet(
        MINT32 openId,
        Vector<CaptureParam_T>& vCaptureParams,
        Vector<CaptureParam_T>* vOrigCaptureParams)
{
    Mutex::Autolock _l(gHdrHandleLock);

    if (gHdrHandle == NULL)
    {
        HDR_LOGE("hdr handle is empty");
        return NO_INIT;
    }

    // copy hdr handle
    HDRHandle& hdrHandle(*gHdrHandle);

    if (hdrHandle.isExposureConfigured)
    {
        HDR_LOGE("exposure setting has been configured");
        return INVALID_OPERATION;
    }

    IHal3A *hal3A = MAKE_Hal3A(
            openId, LOG_TAG);
    if (hal3A == NULL)
    {
        HDR_LOGE("create 3A HAL failed");
        return NO_INIT;
    }

    // get exposure setting from 3A
    ExpSettingParam_T rExpSetting;
    hal3A->send3ACtrl(
            E3ACtrl_GetExposureInfo, reinterpret_cast<MINTPTR>(&rExpSetting), 0);

    IHDRProc2& hdrProc(HDRProc2Factory::getIHDRProc2());

    // set exposure setting to HDR proc
    hdrProc.setParam(
            hdrHandle,
            HDRProcParam_Set_AOEMode, rExpSetting.u4AOEMode, 0);
    hdrProc.setParam(
            hdrHandle,
            HDRProcParam_Set_MaxSensorAnalogGain, rExpSetting.u4MaxSensorAnalogGain, 0);
    hdrProc.setParam(
            hdrHandle,
            HDRProcParam_Set_MaxAEExpTimeInUS, rExpSetting.u4MaxAEExpTimeInUS, 0);
    hdrProc.setParam(
            hdrHandle,
            HDRProcParam_Set_MinAEExpTimeInUS, rExpSetting.u4MinAEExpTimeInUS, 0);
    hdrProc.setParam(
            hdrHandle,
            HDRProcParam_Set_ShutterLineTime, rExpSetting.u4ShutterLineTime, 0);
    hdrProc.setParam(
            hdrHandle,
            HDRProcParam_Set_MaxAESensorGain, rExpSetting.u4MaxAESensorGain, 0);
    hdrProc.setParam(
            hdrHandle,
            HDRProcParam_Set_MinAESensorGain, rExpSetting.u4MinAESensorGain, 0);
    hdrProc.setParam(
            hdrHandle,
            HDRProcParam_Set_ExpTimeInUS0EV, rExpSetting.u4ExpTimeInUS0EV, 0);
    hdrProc.setParam(
            hdrHandle,
            HDRProcParam_Set_SensorGain0EV, rExpSetting.u4SensorGain0EV, 0);
    hdrProc.setParam(
            hdrHandle,
            HDRProcParam_Set_FlareOffset0EV, rExpSetting.u1FlareOffset0EV, 0);
    hdrProc.setParam(
            hdrHandle,
            HDRProcParam_Set_GainBase0EV, rExpSetting.i4GainBase0EV, 0);
    hdrProc.setParam(
            hdrHandle,
            HDRProcParam_Set_LE_LowAvg, rExpSetting.i4LE_LowAvg, 0);
    hdrProc.setParam(
            hdrHandle,
            HDRProcParam_Set_SEDeltaEVx100, rExpSetting.i4SEDeltaEVx100, 0);
    hdrProc.setParam(
            hdrHandle,
            HDRProcParam_Set_DetectFace, rExpSetting.bDetectFace, 0);
    hdrProc.setParam(
            hdrHandle,
            HDRProcParam_Set_Histogram,
            reinterpret_cast<MUINTPTR>(rExpSetting.u4Histogram), 0);
    hdrProc.setParam(
            hdrHandle,
            HDRProcParam_Set_FlareHistogram,
            reinterpret_cast<MUINTPTR>(rExpSetting.u4FlareHistogram), 0);
    {
        // get AEPlineTable
        MINT32 aeTableCurrentIndex;
        strAETable aePlineTable;
        hal3A->send3ACtrl(
                E3ACtrl_GetAECapPLineTable,
                reinterpret_cast<MINTPTR>(&aeTableCurrentIndex),
                reinterpret_cast<MINTPTR>(&aePlineTable));

        hdrProc.setParam(
                hdrHandle,
                HDRProcParam_Set_PLineAETable,
                reinterpret_cast<MUINTPTR>(&aePlineTable), aeTableCurrentIndex);

        dumpCapPLineTable(aeTableCurrentIndex, aePlineTable);
    }

    // get HDR capture information from HDR proc
    MINT32& inputFrameCount(hdrHandle.inputFrameCount);
    Vector<MUINT32> vu4Eposuretime;
    Vector<MUINT32> vu4SensorGain;
    Vector<MUINT32> vu4FlareOffset;

    hdrProc.getHDRCapInfo(
            hdrHandle,
            inputFrameCount, vu4Eposuretime, vu4SensorGain, vu4FlareOffset);

    // query the current 3A information
    CaptureParam_T tmpCap3AParam;
    hal3A->send3ACtrl(
            E3ACtrl_GetExposureParam,
            reinterpret_cast<MINTPTR>(&tmpCap3AParam), 0);

    MUINT32 delayedFrames = 0;
    if (vOrigCaptureParams)
    {
        hal3A->send3ACtrl(
                E3ACtrl_GetCaptureDelayFrame,
                reinterpret_cast<MINTPTR>(&delayedFrames), 0);

        // resize to the amount of delayed frames
        vOrigCaptureParams->resize(delayedFrames);

        for (MUINT32 i = 0; i < delayedFrames; i++)
            vOrigCaptureParams->editItemAt(i) = tmpCap3AParam;
    }

    HDR_LOGD("HDR input frames(%d) delayed frames(%u)",
        inputFrameCount, delayedFrames);

    dumpCaptureParam(tmpCap3AParam, "Original ExposureParam");

    // update 3A information with information from HDR proc
    for (MINT32 i = 0; i < inputFrameCount; i++)
    {
        // copy original capture parameter
        CaptureParam_T modifiedCap3AParam = tmpCap3AParam;
        modifiedCap3AParam.u4Eposuretime  = vu4Eposuretime[i];
        modifiedCap3AParam.u4AfeGain      = vu4SensorGain[i];
        modifiedCap3AParam.u4IspGain      = 1024; // fix ISP gain to 1x
        modifiedCap3AParam.u4FlareOffset  = vu4FlareOffset[i];

        String8 str;
        str.appendFormat("Modified ExposureParam[%d]", i);
        dumpCaptureParam(modifiedCap3AParam, str.string());

        vCaptureParams.push_back(modifiedCap3AParam);
    }

    // raise exposure flag
    hdrHandle.isExposureConfigured = MTRUE;

    hal3A->destroyInstance(LOG_TAG);

    return OK;
}

// ---------------------------------------------------------------------------

HDRNodeImp::HDRNodeImp()
    : mRequestQueue()
    , mbRequestDrained(MFALSE)
    , mbRequestExit(MFALSE)
    , mNodeEnqueIndex(0)
    , mUseSingleFrame(MFALSE)
    , mpPipe(NULL)
    , mp3A(NULL)
{
    // log level
    mLogLevel = property_get_int32("debug.camera.log", 0);
    if (mLogLevel == 0)
        mLogLevel = property_get_int32("debug.camera.log.HDRNode", 0);

    // debug dump
    mDebugDump = property_get_int32("mediatek.hdr.debug", 0);

    HDR_LOGI("log level is set to %d", mLogLevel);
    HDR_LOGI_IF(mDebugDump, "buffer dump enabled");

    // make debug dump path
    if (mDebugDump && !makePath(HDR_DUMP_PATH, 0660))
    {
        HDR_LOGW("make debug dump path %s failed", HDR_DUMP_PATH);
    }
}

HDRNodeImp::~HDRNodeImp()
{
}

MERROR HDRNodeImp::init(const InitParams& rParams)
{
    CAM_TRACE_CALL();

    RWLock::AutoWLock _l(mConfigRWLock);

    mvFrameParams.clear();
    mvHDRInputFrames.clear();

    mOpenId   = rParams.openId;
    mNodeId   = rParams.nodeId;
    mNodeName = rParams.nodeName;

    // initialize HDR proc
    HDR_LOGD("init HDR proc...");
    IHDRProc2& hdrProc(HDRProc2Factory::getIHDRProc2());

    // get HDR handle and set complete callback
    if (hdrProc.init(mOpenId, mHDRHandle) != MTRUE)
    {
        HDR_LOGE("init HDR proc failed");
        return INVALID_OPERATION;
    }

    hdrProc.setCompleteCallback(mHDRHandle, HDRProcCompleteCallback, this);

    // set the current hdr handle as a global scope
    {
        Mutex::Autolock _l(gHdrHandleLock);
        gHdrHandle = &mHDRHandle;
    }

    // create pipe before HDR processing
    if (createPipe() == MFALSE)
    {
        HDR_LOGE("create pipe failed");
        return INVALID_OPERATION;
    }

    // create worker threads
    mpHDRThread = new HDRThread(P2THREAD_NAME_ENQUE, this);
    if (OK != mpHDRThread->run(P2THREAD_NAME_ENQUE))
    {
        HDR_LOGE("create hdr thread failed");
        return INVALID_OPERATION;
    }

    mpHDRProcThread = new HDRProcThread(P2THREAD_NAME_HDR, this);
    if (OK != mpHDRProcThread->run(P2THREAD_NAME_HDR))
    {
        HDR_LOGE("create HDR proc thread failed");
        return INVALID_OPERATION;
    }

    // query active array size
    sp<IMetadataProvider> pMetadataProvider = NSMetadataProviderManager::valueFor(getOpenId());
    IMetadata static_meta = pMetadataProvider->geMtktStaticCharacteristics();
    if (tryGetMetadata<MRect>(&static_meta, MTK_SENSOR_INFO_ACTIVE_ARRAY_REGION, mActiveArray))
    {
        HDR_LOGD("active array(%d, %d, %dx%d)",
                mActiveArray.p.x, mActiveArray.p.y, mActiveArray.s.w, mActiveArray.s.h);
    }
    else
    {
        HDR_LOGE("no static info: MTK_SENSOR_INFO_ACTIVE_ARRAY_REGION");
        return UNKNOWN_ERROR;
    }

    HDR_LOGD("nodeId(%#08" PRIxPTR ") nodeName(%s)", getNodeId(), getNodeName());

    return OK;
}

MERROR HDRNodeImp::uninit()
{
    CAM_TRACE_CALL();

    if (OK != flush()) HDR_LOGE("flush failed");

    // TODO: check if pending requests are handled properly before exiting threads
    // destroy worker threads
    mpHDRThread->requestExit();
    mpHDRThread->join();
    mpHDRThread = NULL;

    mpHDRProcThread->requestExit();
    mpHDRProcThread->join();
    mpHDRProcThread = NULL;

    // release output frames
    for (size_t i = 0; i < mvFrameParams.size(); i++)
    {
        for (size_t j = 0; j < mvFrameParams[i].vpIn.size(); j++)
        {
            HDR_LOGV("delete vpIn i(%zu)j(%zu)", i, j);
            delete mvFrameParams[i].vpIn[j];
        }

        for (size_t j = 0; j < mvFrameParams[i].vpOut.size(); j++)
        {
            HDR_LOGV("delete vpOut i(%zu)j(%zu)", i, j);
            delete mvFrameParams[i].vpOut[j];
        }
    }

    // destroy pipe after finish HDR processing
    destroyPipe();

    mvFrameParams.clear();
    mvHDRInputFrames.clear();

    return OK;
}

MERROR HDRNodeImp::config(const ConfigParams& rParams)
{
    CAM_TRACE_CALL();

    // check ConfigParams
    MERROR const err = verifyConfigParams(rParams);
    if (err != OK)
    {
        HDR_LOGE("verifyConfigParams failed: err = %d", err);
        return err;
    }

    flush();

    // configure streams into node
    {
        RWLock::AutoWLock _l(mConfigRWLock);
        // meta
        mpInAppMeta_Request = rParams.pInAppMeta;
        mpInHalMeta_P1      = rParams.pInHalMeta;
        mpOutAppMeta_Result = rParams.pOutAppMeta;
        mpOutHalMeta_Result = rParams.pOutHalMeta; // not necessary
        // image
        mpInFullRaw         = rParams.vInFullRaw[0];
        mpInResizedRaw      = rParams.pInResizedRaw;
        mpOutYuvJpeg        = rParams.vOutYuvJpeg;
        mpOutYuvThumbnail   = rParams.vOutYuvThumbnail;
        mvOutImages         = rParams.vOutImage;
        mpInLcsoRaw         = (rParams.vInLcsoRaw.size() > 0) ?
                               rParams.vInLcsoRaw[0] : NULL;
    }

    return OK;
}

MERROR HDRNodeImp::flush()
{
    CAM_TRACE_CALL();

    // flush requests before exit
    {
        Mutex::Autolock _l(mRequestQueueLock);

        Que_T::iterator it = mRequestQueue.begin();
        while (it != mRequestQueue.end())
        {
            BaseNode::flush(*it);
            it = mRequestQueue.erase(it);
            CAM_TRACE_INT("request(hdr)", mRequestQueue.size());
        }
    }

    // wait for HDR thread
    waitForRequestDrained();

    return OK;
}

MERROR HDRNodeImp::queue(android::sp<IPipelineFrame> pFrame)
{
    CAM_TRACE_NAME("queue(HDR)");

    if (!pFrame.get())
    {
        HDR_LOGE("null frame");
        return BAD_VALUE;
    }

    Mutex::Autolock _l(mRequestQueueLock);

    // make sure the request with a smaller frame number has a higher priority
    Que_T::iterator it = mRequestQueue.end();
    for (; it != mRequestQueue.begin();)
    {
        --it;
        if (0 <= (MINT32)(pFrame->getFrameNo() - (*it)->getFrameNo()))
        {
            ++it;   //insert(): insert before the current node
            break;
        }
    }

    // insert before the current frame
    mRequestQueue.insert(it, pFrame);

    CAM_TRACE_INT("request(hdr)", mRequestQueue.size());

    mRequestQueueCond.broadcast();

    return OK;
}

MERROR HDRNodeImp::mapToRequests(
        sp<IPipelineFrame> const& pFrame,
        IPipelineFrame::InfoIOMapSet const& infoIOMapSet)
{
    CAM_TRACE_CALL();

    MERROR err = OK;

    IStreamBufferSet& rStreamBufferSet(pFrame->getStreamBufferSet());
    const MUINT32 FRAME_NO = pFrame->getFrameNo();
    FrameParams frameParams;

    // NOTE: ensure buffers are available
    frameParams.pFrame = pFrame;

    // image
    {
        IPipelineFrame::ImageInfoIOMapSet const& mapSet =
            infoIOMapSet.mImageInfoIOMapSet;

        // traverse image info set
        for (size_t i = 0; i < mapSet.size(); i++)
        {
            HDR_LOGD("ImageInfoIOMapSet(%zu) frameNo(%u) In(%zu) Out(%zu)",
                    i, FRAME_NO, mapSet[i].vIn.size(), mapSet[i].vOut.size());

            // get input image buffer
            for (size_t j = 0; j < mapSet[i].vIn.size(); j++)
            {
                FrameInput* in = new FrameInput;

                in->streamId = mapSet[i].vIn[j]->getStreamId();
                const char* streamName = mapSet[i].vIn[j]->getStreamName();
                HDR_LOGD("    IN  streamId(%#08" PRIxPTR ") name(%s)", in->streamId, streamName);

                if (isFullRawLocked(in->streamId))
                {
                    in->portId = PORT_IMGI; // use IMGI port for full size RAW
                }
                else if (isLcsoRawLocked(in->streamId))
                {
                    in->portId = PORT_LCEI; // use LCEI port for lcso buffer
                }
                else
                {
                    // TODO: add error handling
                    HDR_LOGE("unsupported input streamId(%#08" PRIxPTR ")",
                            in->streamId);
                }

                sp<IImageStreamBuffer>& inStreamBuffer(in->pStreamBuffer);
                sp<IImageBufferHeap>& inImageBufferHeap(in->pImageBufferHeap);
                sp<IImageBuffer>& inImageBuffer(in->pImageBuffer);

                inStreamBuffer    = NULL;
                inImageBufferHeap = NULL;
                inImageBuffer     = NULL;

                err = ensureImageBufferAvailable_(
                        FRAME_NO, in->streamId, rStreamBufferSet, inStreamBuffer);
                if (err != OK)
                {
                    HDR_LOGE("src buffer err = %d (%s)", err, strerror(-err));
                    if (inStreamBuffer == NULL)
                    {
                        HDR_LOGE("pStreamBuffer is NULL");
                    }
                    delete in;
                    return NO_INIT;
                }

                // get imagebuffer from image buffer heap, which belongs to a streambuffer
                err = getImageBuffer(IN, in->streamId,
                        inStreamBuffer, inImageBufferHeap, inImageBuffer);
                if (err != OK)
                {
                    HDR_LOGE("get imagebuffer failed");
                    delete in;
                    return UNKNOWN_ERROR;
                }

                frameParams.vpIn.push_back(in);
            }

            // get output image buffers
            for (size_t j = 0; j < mapSet[i].vOut.size(); j++)
            {
                FrameOutput* out = new FrameOutput;

                out->streamId = mapSet[i].vOut[j]->getStreamId();
                out->transform = mapSet[i].vOut[j]->getTransform();
                const char* streamName = mapSet[i].vOut[j]->getStreamName();

                HDR_LOGD("    OUT streamId(%#08" PRIxPTR ") name(%s)",
                        out->streamId, streamName);

                // get output image buffer
                sp<IImageStreamBuffer>& outStreamBuffer(out->pStreamBuffer);
                sp<IImageBufferHeap>& outImageBufferHeap(out->pImageBufferHeap);
                sp<IImageBuffer>& outImageBuffer(out->pImageBuffer);

                outStreamBuffer    = NULL;
                outImageBufferHeap = NULL;
                outImageBuffer     = NULL;

                err = ensureImageBufferAvailable_(
                        FRAME_NO, out->streamId, rStreamBufferSet, outStreamBuffer);
                if (err != OK)
                {
                    HDR_LOGE("dst buffer err = %d (%s)", err, strerror(-err));
                    if (outStreamBuffer == NULL)
                    {
                        HDR_LOGE("pStreamBuffer is NULL");
                    }
                    delete out;
                    return NO_INIT;
                }

                // get imagebuffer from image buffer heap, which belongs to a streambuffer
                err = getImageBuffer(OUT, out->streamId,
                        outStreamBuffer, outImageBufferHeap, outImageBuffer);
                if (err != OK)
                {
                    HDR_LOGE("get imagebuffer failed");
                    delete out;
                    return UNKNOWN_ERROR;
                }

                frameParams.vpOut.push_back(out);
            }
        } // traverse image info set
    } // image

    // metadata
    {
        IPipelineFrame::MetaInfoIOMapSet const& mapSet =
            infoIOMapSet.mMetaInfoIOMapSet;

        // traverse metadata info set
        for (size_t i = 0; i < mapSet.size(); i++)
        {
            HDR_LOGD("MetaInfoIOMapSet(%zu) frameNo(%u) In(%zu) Out(%zu)",
                    i, FRAME_NO, mapSet[i].vIn.size(), mapSet[i].vOut.size());

            // get input meta buffer
            for (size_t j = 0; j < mapSet[i].vIn.size(); j++)
            {
                StreamId_T const streamId = mapSet[i].vIn[j]->getStreamId();
                const char* streamName = mapSet[i].vIn[j]->getStreamName();
                HDR_LOGD("    IN  streamId(%#08" PRIxPTR ") name(%s)", streamId, streamName);

                // get in app metadata
                if (mpInAppMeta_Request->getStreamId() == streamId)
                {
                    sp<IMetaStreamBuffer>& inStreamBuffer(frameParams.pMeta_InAppStreamBuffer);

                    inStreamBuffer = NULL;

                    err = ensureMetaBufferAvailable_(
                            FRAME_NO, streamId, rStreamBufferSet, inStreamBuffer);
                    if (err != OK)
                    {
                        HDR_LOGE("src metadata err = %d (%s)", err, strerror(-err));
                        if (inStreamBuffer == NULL)
                        {
                            HDR_LOGE("pStreamBuffer is NULL");
                        }
                        return NO_INIT;
                    }

                    // get metadata from meta stream buffer
                    err = getMetadata(IN, streamId,
                            inStreamBuffer, frameParams.pMeta_InApp);
                    if (err != OK)
                    {
                        HDR_LOGE("get metadata failed");
                        return UNKNOWN_ERROR;
                    }
                }

                // get P1 hal metadata
                if (mpInHalMeta_P1->getStreamId() == streamId)
                {
                    sp<IMetaStreamBuffer>& inStreamBuffer(frameParams.pMeta_InHalStreamBuffer);

                    inStreamBuffer = NULL;

                    err = ensureMetaBufferAvailable_(
                            FRAME_NO, streamId, rStreamBufferSet, inStreamBuffer);
                    if (err != OK)
                    {
                        HDR_LOGE("src metadata err = %d (%s)", err, strerror(-err));
                        if (inStreamBuffer == NULL)
                        {
                            HDR_LOGE("pStreamBuffer is NULL");
                        }
                        return NO_INIT;
                    }

                    // get metadata from meta stream buffer
                    err = getMetadata(IN, streamId,
                            inStreamBuffer, frameParams.pMeta_InHal);
                    if (err != OK)
                    {
                        HDR_LOGE("get metadata failed");
                        return UNKNOWN_ERROR;
                    }

                    // get pipeline's unique key for debug purpose
                    {
                        if (!tryGetMetadata<MINT32>(frameParams.pMeta_InHal,
                                    MTK_PIPELINE_UNIQUE_KEY, frameParams.uniqueKey))
                        {
                            HDR_LOGE("get pipeline unique key failed");
                            return UNKNOWN_ERROR;
                        }

                        MFCNodeImp::dumpExifInfo(*frameParams.pMeta_InHal);
                    }
                }
            }

            // get output meta buffer
            for (size_t j = 0; j < mapSet[i].vOut.size(); j++)
            {
                StreamId_T const streamId = mapSet[i].vOut[j]->getStreamId();
                const char* streamName = mapSet[i].vOut[j]->getStreamName();
                HDR_LOGD("    OUT streamId(%#08" PRIxPTR ") name(%s)", streamId, streamName);

                // get out app metadata
                if (mpOutAppMeta_Result->getStreamId() == streamId)
                {
                    sp<IMetaStreamBuffer>& outStreamBuffer(frameParams.pMeta_OutAppStreamBuffer);

                    outStreamBuffer = NULL;

                    err = ensureMetaBufferAvailable_(
                            FRAME_NO, streamId, rStreamBufferSet, outStreamBuffer);
                    if (err != OK)
                    {
                        HDR_LOGE("dst metadata err = %d (%s)", err, strerror(-err));
                        if (outStreamBuffer == NULL)
                        {
                            HDR_LOGE("pStreamBuffer is NULL");
                        }
                        return NO_INIT;
                    }

                    // get metadata from meta stream buffer
                    err = getMetadata(OUT, streamId,
                            outStreamBuffer, frameParams.pMeta_OutApp);
                    if (err != OK)
                    {
                        HDR_LOGE("get metadata failed");
                        return UNKNOWN_ERROR;
                    }
                }

                // get out hal metadata
                if (mpOutHalMeta_Result->getStreamId() == streamId)
                {
                    sp<IMetaStreamBuffer>& outStreamBuffer(frameParams.pMeta_OutHalStreamBuffer);

                    outStreamBuffer = NULL;

                    err = ensureMetaBufferAvailable_(
                            FRAME_NO, streamId, rStreamBufferSet, outStreamBuffer);
                    if (err != OK)
                    {
                        HDR_LOGE("dst metadata err = %d (%s)", err, strerror(-err));
                        if (outStreamBuffer == NULL)
                        {
                            HDR_LOGE("pStreamBuffer is NULL");
                        }
                        return NO_INIT;
                    }

                    // get metadata from meta stream buffer
                    err = getMetadata(OUT, streamId,
                            outStreamBuffer, frameParams.pMeta_OutHal);
                    if (err != OK)
                    {
                        HDR_LOGE("get metadata failed");
                        return UNKNOWN_ERROR;
                    }
                }
            } // get output meta buffer
        } // traverse metadata info set
    } // metadata

    if ((frameParams.pMeta_InApp == NULL) || (frameParams.pMeta_InHal == NULL))
    {
        HDR_LOGE_IF(frameParams.pMeta_InApp == NULL, "In app metadata is NULL");
        HDR_LOGE_IF(frameParams.pMeta_InHal == NULL, "In hal metadata is NULL");
        return NO_INIT;
    }

    // from now on, frameParams is ready for processing
    mvFrameParams.push_back(frameParams);

    return err;
}

MERROR HDRNodeImp::convertRaw2Yuv()
{
    CAM_TRACE_CALL();

    MERROR err = OK;
    // used for dump buffer
    char szResultFileName[100];

    QParams enqueParams;

    // frame tag
    enqueParams.mvStreamTag.push_back(
            NSCam::NSIoPipe::NSPostProc::ENormalStreamTag_Normal);

    // full raw input
    FrameInput *fullRawFrameInput = NULL;
    // check if LCSO raw buffer exists
    // if yes, we need give LCSO buffer to pass2 driver
    FrameInput *lcsoRawFrameInput = NULL;
    for (size_t i = 0; i < mvFrameParams[mNodeEnqueIndex].vpIn.size(); i++)
    {
        if (isFullRawLocked(mvFrameParams[mNodeEnqueIndex].vpIn[i]->streamId))
        {
            HDR_LOGV("found full raw at %zu", i);
            fullRawFrameInput = mvFrameParams[mNodeEnqueIndex].vpIn[i];
        }
        else if (isLcsoRawLocked(mvFrameParams[mNodeEnqueIndex].vpIn[i]->streamId))
        {
            HDR_LOGV("found lcso raw at %zu", i);
            lcsoRawFrameInput = mvFrameParams[mNodeEnqueIndex].vpIn[i];
        }
    }

    if (fullRawFrameInput == NULL)
    {
        HDR_LOGE("cannot find full raw");
        return INVALID_OPERATION;
    }

    if (lcsoRawFrameInput == NULL)
    {
        HDR_LOGD("connot find lcso raw");
        // it doesn't matter if no lcso raw
    }

    // NOTE: for full sized raw, ISP pass1 uses IMGO as output port
    // and the AWB gain is set via PGN in ISP pass2
    MUINT8 enablePGN = 1;
    {
        Input src;
        src.mPortID       = fullRawFrameInput->portId;
        src.mPortID.group = 0;
        src.mBuffer       = fullRawFrameInput->pImageBuffer.get();
        enqueParams.mvIn.push_back(src);

        HDR_LOGD("pass2 enque - src port index(%d) format(0x%x) size(%dx%d) heap(%p)",
                src.mPortID.index, src.mBuffer->getImgFormat(),
                src.mBuffer->getImgSize().w, src.mBuffer->getImgSize().h,
                src.mBuffer->getImageBufferHeap());
    }

    // NOTE: if LCSO raw buffer exists, ISP pass2 MUST need LCSO buffer,
    // otherwise the device will hang
    if (lcsoRawFrameInput)
    {
        Input src;
        src.mPortID        = lcsoRawFrameInput->portId;
        src.mPortID.group  = 0;
        src.mBuffer        = lcsoRawFrameInput->pImageBuffer.get();
        enqueParams.mvIn.push_back(src);

        HDR_LOGD("pass2 enque - src port index(%d) format(0x%x) size(%dx%d) heap(%p)",
                src.mPortID.index, src.mBuffer->getImgFormat(),
                src.mBuffer->getImgSize().w, src.mBuffer->getImgSize().h,
                src.mBuffer->getImageBufferHeap());
    }

    // output
    // FRAME_INDEX is uesd to access the YUV and Y8 frames
    const MINT32 FRAME_INDEX = mNodeEnqueIndex << 1;
    {
        // main output (full image)
        Output dst;
        dst.mPortID       = PORT_WROTO;
        dst.mPortID.group = 0;
        dst.mBuffer       = mvHDRInputFrames[FRAME_INDEX].get();
        enqueParams.mvOut.push_back(dst);

        HDR_LOGD("pass2 enque - dst port index(%d) format(0x%x) size(%dx%d)",
                dst.mPortID.index, dst.mBuffer->getImgFormat(),
                dst.mBuffer->getImgSize().w, dst.mBuffer->getImgSize().h);

        if (CC_LIKELY(!mUseSingleFrame))
        {
            // main output (small image)
            Output dst2;
            dst2.mPortID       = PORT_WDMAO;
            dst2.mPortID.group = 0;
            dst2.mBuffer       = mvHDRInputFrames[(FRAME_INDEX + 1)].get();
            enqueParams.mvOut.push_back(dst2);

            HDR_LOGD("pass2 enque - dst2 port index(%d) format(0x%x) size(%dx%d)",
                    dst2.mPortID.index, dst2.mBuffer->getImgFormat(),
                    dst2.mBuffer->getImgSize().w, dst2.mBuffer->getImgSize().h);
        }
    }

    if (enqueParams.mvOut.size() == 0)
    {
        HDR_LOGW("no dst buffer");
        return BAD_VALUE;
    }

    {
        TuningParam rTuningParam { NULL, NULL, NULL };
        const MUINT32 tuningsize = mpPipe->getRegTableSize();
        void *pTuning = malloc(tuningsize);
        if (pTuning == NULL)
        {
            HDR_LOGE("alloc tuning buffer failed");
            return UNKNOWN_ERROR;
        }
        rTuningParam.pRegBuf = pTuning;
        rTuningParam.pLcsBuf =
            lcsoRawFrameInput ? lcsoRawFrameInput->pImageBuffer.get() : NULL;

        // prepare input metadata
        IMetadata *pMeta_InApp(mvFrameParams[mNodeEnqueIndex].pMeta_InApp);
        IMetadata *pMeta_InHal(mvFrameParams[mNodeEnqueIndex].pMeta_InHal);
        IMetadata *pMeta_OutApp(mvFrameParams[mNodeEnqueIndex].pMeta_OutApp);
        IMetadata *pMeta_OutHal(mvFrameParams[mNodeEnqueIndex].pMeta_OutHal);

        // copy input metadata to meta set
        MetaSet_T inMetaSet { .appMeta = *pMeta_InApp, .halMeta = *pMeta_InHal };
        MetaSet_T outMetaSet;

        const MBOOL bGetResult = (pMeta_OutApp || pMeta_OutHal);

        // NOTE: for full sized raw, ISP pass1 uses IMGO as output port
        // and the AWB gain is set via PGN in ISP pass2
        updateEntry<MUINT8>(&inMetaSet.halMeta, MTK_3A_PGN_ENABLE, 1);

        if (mp3A)
        {
            // set pipeline frame and request number into hal metadata
            const sp<IPipelineFrame>& frame(mvFrameParams[mNodeEnqueIndex].pFrame);
            {
                trySetMetadata<MINT32>(
                        &inMetaSet.halMeta,
                        MTK_PIPELINE_FRAME_NUMBER, frame->getFrameNo());
                trySetMetadata<MINT32>(
                        &inMetaSet.halMeta,
                        MTK_PIPELINE_REQUEST_NUMBER, frame->getRequestNo());

                HDR_LOGD("frameNo(%d) requestNo(%d) getResult(%d)",
                        frame->getFrameNo(), frame->getRequestNo(), bGetResult);
            }

            HDR_LOGD("pass2 setIsp - tuning data addr(%#" PRIxPTR ") size(%d)",
                    reinterpret_cast<uintptr_t>(pTuning), tuningsize);
            if (0 > mp3A->setIsp(0, inMetaSet, &rTuningParam, &outMetaSet))
            {
                HDR_LOGW("pass2 setIsp - skip tuning pushing");
                if (pTuning != NULL)
                {
                    HDR_LOGD("pass2 setIsp - free tuning data addr(%#" PRIxPTR ") size(%d)",
                            reinterpret_cast<uintptr_t>(pTuning), tuningsize);
                    free(pTuning);
                }
            }
            else
            {
                enqueParams.mvTuningData.push_back(pTuning);

                IImageBuffer* pSrc = static_cast<IImageBuffer*>(rTuningParam.pLsc2Buf);
                if (pSrc != NULL)
                {
                    Input src;
                    src.mPortID       = PORT_DEPI;
                    src.mPortID.group = 0;
                    src.mBuffer       = pSrc;

                    enqueParams.mvIn.push_back(src);

                    HDR_LOGD("pass2 enque - src port index(%d) format(0x%x) size(%dx%d)",
                            src.mPortID.index, src.mBuffer->getImgFormat(),
                            src.mBuffer->getImgSize().w, src.mBuffer->getImgSize().h);
                }
            }
        }
        else
        {
            HDR_LOGD("pass2 setIsp - clear tuning data addr(%#" PRIxPTR ") size(%d)",
                    reinterpret_cast<uintptr_t>(pTuning), tuningsize);
            memset((unsigned char*)(pTuning), 0, tuningsize);
        }

        // because output metadata stream only exist in the first pipeline frame,
        // append 3A output metadata to the first pipeline frame's metadata stream
        if (bGetResult)
        {
            if (pMeta_OutApp)
            {
                *pMeta_OutApp = outMetaSet.appMeta;
            }

            if (pMeta_OutHal)
            {
                *pMeta_OutHal = *pMeta_InHal;
                *pMeta_OutHal += outMetaSet.halMeta;
            }
        }
    }

    // for output group crop
    {
        const MRect srcRect(MPoint(0, 0), enqueParams.mvIn[0].mBuffer->getImgSize());

        Vector<Output>::const_iterator it = enqueParams.mvOut.begin();
        Vector<Output>::const_iterator end = enqueParams.mvOut.end();
        while (it != end)
        {
            const MRect dstRect(MPoint(0, 0), it->mBuffer->getImgSize());

            HDR_LOGD("src rect(%d, %d, %dx%d) -> dst rect(%d, %d, %dx%d)",
                    srcRect.p.x, srcRect.p.y, srcRect.s.w, srcRect.s.h,
                    dstRect.p.x, dstRect.p.y, dstRect.s.w, dstRect.s.h);

            MRect cropDstRect = calCrop(srcRect, dstRect);

            HDR_LOGD("dst crop rect(%d, %d, %dx%d)",
                    cropDstRect.p.x, cropDstRect.p.y,
                    cropDstRect.s.w, cropDstRect.s.h);

            MCrpRsInfo cropInfo;

            if (it->mPortID == PORT_IMG2O)
            {
                cropInfo.mGroupID = 1;
            }
            else if (it->mPortID == PORT_WDMAO)
            {
                cropInfo.mGroupID = 2;
            }
            else if (it->mPortID == PORT_WROTO)
            {
                cropInfo.mGroupID = 3;
            }
            else
            {
                HDR_LOGE("not supported output port index(%d)", it->mPortID.index);
                return BAD_TYPE;
            }

            cropInfo.mFrameGroup = 0;
            cropInfo.mCropRect.p_integral.x = cropDstRect.p.x;
            cropInfo.mCropRect.p_integral.y = cropDstRect.p.y;
            cropInfo.mCropRect.p_fractional.x = 0;
            cropInfo.mCropRect.p_fractional.y = 0;
            cropInfo.mCropRect.s.w = cropDstRect.s.w;
            cropInfo.mCropRect.s.h = cropDstRect.s.h;

            enqueParams.mvCropRsInfo.push_back(cropInfo);

            it++;
        }
    }

    // set callback
    enqueParams.mpfnCallback = pass2CbFunc;
    enqueParams.mpCookie     = this;

    // queue a request into the pipe
    {
        CAM_TRACE_NAME("enque_P2");
        HDR_LOGI("enque pass2");
        if (!mpPipe->enque(enqueParams))
        {
            HDR_LOGE("enque pass2 failed");
            mNodeEnqueIndex += 1;
            return INVALID_OPERATION;
        }
    }

    // wait for P2 to finish raw-to-yuv process
    {
        CAM_TRACE_NAME("wait_P2_done");
        HDR_LOGD("wait for P2...");
        mP2Cond.wait(mP2Lock);
        HDR_LOGD("wait P2 done");
    }

    // dump input raw buffer if necessary
    if (mDebugDump)
    {
        const sp<IPipelineFrame>& frame(mvFrameParams[mNodeEnqueIndex].pFrame);
        const sp<IImageBuffer>& imageBuffer(fullRawFrameInput->pImageBuffer);
        const sp<IImageBuffer>& yuvImageBuffer(mvHDRInputFrames[FRAME_INDEX]);
        const MSize size = imageBuffer->getImgSize();
        const MSize yuvSize = yuvImageBuffer->getImgSize();

        // raw format
        sprintf(szResultFileName,
                HDR_DUMP_PATH "hdr-%09d-%04d-%04d-input-%dx%d-RawImgBuffer[%d].raw",
                mvFrameParams[mNodeEnqueIndex].uniqueKey,
                frame->getFrameNo(), frame->getRequestNo(),
                size.w, size.h,
                mNodeEnqueIndex);
        imageBuffer->saveToFile(szResultFileName);

        // yuv format
        sprintf(szResultFileName,
                HDR_DUMP_PATH "hdr-%09d-%04d-%04d-input-%dx%d-YuvImgBuffer[%d]-%s.yuv",
                mvFrameParams[mNodeEnqueIndex].uniqueKey,
                frame->getFrameNo(), frame->getRequestNo(),
                yuvSize.w, yuvSize.h,
                mNodeEnqueIndex,
                getFileFormatName(yuvImageBuffer->getImgFormat()));
        yuvImageBuffer->saveToFile(szResultFileName);
    }

    return err;
}

MVOID HDRNodeImp::onProcessFrame(sp<IPipelineFrame> const& pFrame)
{
    CAM_TRACE_NAME("onProcessFrame(HDR)");

    MERROR err = OK;
    // used for dump buffer
    char szResultFileName[100];

    HDR_LOGD("mNodeEnqueIndex(%d) frameNo(%u) requestNo(%u) nodeId(%#08" PRIxPTR ")",
            mNodeEnqueIndex, pFrame->getFrameNo(), pFrame->getRequestNo(), getNodeId());

    // map pipeline frame to fream parameter
    {
        // get IOMapSet
        IPipelineFrame::InfoIOMapSet infoIOMapSet;
        if (OK != getInfoIOMapSet(pFrame, infoIOMapSet, mNodeEnqueIndex))
        {
            HDR_LOGE("queryInfoIOMap failed");
            BaseNode::flush(pFrame);
            return;
        }

        if (OK != mapToRequests(pFrame, infoIOMapSet))
        {
            HDR_LOGE("map pipeline frame to frame parameter failed");
            BaseNode::flush(pFrame);
            return;
        }
    }

    // check if this request is used to do error handling
    {
        const FrameParams& frameParams(mvFrameParams[mNodeEnqueIndex]);

        MUINT8 isUseSingleFrame = 0;
        tryGetMetadata<MUINT8>(frameParams.pMeta_InHal,
                    MTK_HAL_REQUEST_ERROR_FRAME, isUseSingleFrame);

        mUseSingleFrame = (isUseSingleFrame != 0) ? MTRUE : MFALSE;

        HDR_LOGD("process %s", mUseSingleFrame ? "single frame" : "multi-frame");
    }

    err= CC_LIKELY(!mUseSingleFrame) ?
        processMultiFrameHDR() : processSingleFrame();
    if (err != OK)
    {
        HDR_LOGE("process frame failed(%s)", strerror(-err));
        BaseNode::flush(pFrame);
        return;
    }

    // let's go to the next round
    mNodeEnqueIndex += 1;
}

MERROR HDRNodeImp::processSingleFrame()
{
    if (mNodeEnqueIndex != 0)
        HDR_LOGW("single frame flow does not support multiple requests");

    // capture buffer
    sp<IImageBuffer> captureBuffer;

    if (mNodeEnqueIndex == 0)
    {
        // we've already collect all meta from the first enqueued frame,
        // signal HDR proc thread to prepare
        HDR_LOGD("notify HDRProcThread to work");
        mMetatDataInfoCond.signal();

        // NOTE: hdrproc is initialized in init(),
        // we need to release hdrproc in single frame flow
        {
            IHDRProc2& hdrProc(HDRProc2Factory::getIHDRProc2());
            const HDRHandle& hdrHandle(mHDRHandle);

            hdrProc.release(mHDRHandle);
            hdrProc.uninit(mHDRHandle);
        }

        // allocate capture buffer (YV12)
        {
            // full raw input
            MINT fullRawIndex = -1;
            for (size_t i = 0; i < mvFrameParams[mNodeEnqueIndex].vpIn.size(); i++)
            {
                if (isFullRawLocked(mvFrameParams[mNodeEnqueIndex].vpIn[i]->streamId))
                {
                    HDR_LOGV("found full raw at %zu", i);
                    fullRawIndex = i;
                    break;
                }
            }

            if (fullRawIndex == -1)
            {
                HDR_LOGE("cannnot find full raw");
                return INVALID_OPERATION;
            }

            sp<IImageBuffer>& fullRaw(
                    mvFrameParams[mNodeEnqueIndex].vpIn[fullRawIndex]->pImageBuffer);

            ImageBufferUtils::getInstance().allocBuffer(
                    captureBuffer,
                    fullRaw->getImgSize().w, fullRaw->getImgSize().h,
                    eImgFmt_YV12, MFALSE);
            if (captureBuffer == NULL)
            {
                HDR_LOGE("image buffer is NULL");
                return BAD_VALUE;
            }

            mvHDRInputFrames.push_back(captureBuffer);
        }

        // convert raw to yuv buffers
        if (OK != convertRaw2Yuv())
        {
            HDR_LOGE("convert raw to yuv failed");
        }
    }

    // unlock image/meta input buffer, mark status and then apply buffer to be released
    {
        CAM_TRACE_NAME("releaseInput");

        const FrameParams& frameParams(mvFrameParams[mNodeEnqueIndex]);

        for (size_t i = 0; i < frameParams.vpIn.size(); i++)
        {
            HDR_LOGD("release input image buffer i(%zu)", i);

            FrameInput* frameInput(frameParams.vpIn[i]);

            sp<IImageStreamBuffer>& streamBuffer(frameInput->pStreamBuffer);
            unlockImage(streamBuffer, frameInput->pImageBuffer);
            markImageStream(frameParams.pFrame, streamBuffer);
        }

        if (frameParams.pMeta_InApp != NULL)
        {
            const sp<IMetaStreamBuffer>& streamBuffer(frameParams.pMeta_InAppStreamBuffer);
            unlockMetadata(streamBuffer, frameParams.pMeta_InApp);
            markMetaStream(frameParams.pFrame, streamBuffer);
            HDR_LOGD("release pMeta_InAppStreamBuffer(%#08" PRIxPTR ")",
                    streamBuffer->getStreamInfo()->getStreamId());
        }

        if (frameParams.pMeta_InHal != NULL)
        {
            const sp<IMetaStreamBuffer>& streamBuffer(frameParams.pMeta_InHalStreamBuffer);
            unlockMetadata(streamBuffer, frameParams.pMeta_InHal);
            markMetaStream(frameParams.pFrame, streamBuffer);
            HDR_LOGD("release pMeta_InHalStreamBuffer(%#08" PRIxPTR ")",
                    streamBuffer->getStreamInfo()->getStreamId());
        }

        // apply buffers to be released
        {
            CAM_TRACE_NAME("applyRelease");
            HDR_LOGD("applyRelease - frameNo(%d) nodeId(%#08" PRIxPTR ")",
                    frameParams.pFrame->getFrameNo(), getNodeId());

            // after this call, all of RELEASE-marked buffers are released by this user
            onPartialFrameDone(frameParams.pFrame);
        }
    }

    if (mNodeEnqueIndex == 0)
    {
        // handle digital zoom
        handleDigitalZoom(captureBuffer);

        // dealloc yuv buffer
        ImageBufferUtils::getInstance().deallocBuffer(captureBuffer);
    }

    // unlock image/meta output buffer, mark status and then apply buffer to be released
    {
        CAM_TRACE_NAME("releaseOutput");

        const FrameParams& frameParams(mvFrameParams[mNodeEnqueIndex]);

        for (size_t i = 0; i < frameParams.vpOut.size(); i++)
        {
            HDR_LOGD("release output image buffer i(%zu)", i);

            FrameOutput* frameOutput(frameParams.vpOut[i]);

            sp<IImageStreamBuffer>& streamBuffer(frameOutput->pStreamBuffer);
            unlockImage(streamBuffer, frameOutput->pImageBuffer);
            markImageStream(frameParams.pFrame, streamBuffer);
        }

        if (frameParams.pMeta_OutApp != NULL)
        {
            const sp<IMetaStreamBuffer>& streamBuffer(frameParams.pMeta_OutAppStreamBuffer);
            unlockMetadata(streamBuffer, frameParams.pMeta_OutApp);
            markMetaStream(frameParams.pFrame, streamBuffer);
            HDR_LOGD("release pMeta_OutAppStreamBuffer(%#lx)",
                    streamBuffer->getStreamInfo()->getStreamId());
        }

        if (frameParams.pMeta_OutHal != NULL)
        {
            const sp<IMetaStreamBuffer>& streamBuffer(frameParams.pMeta_OutHalStreamBuffer);
            unlockMetadata(streamBuffer, frameParams.pMeta_OutHal);
            markMetaStream(frameParams.pFrame, streamBuffer);
            HDR_LOGD("release pMeta_OutHalStreamBuffer(%#lx)",
                    streamBuffer->getStreamInfo()->getStreamId());
        }

        // apply buffers to be released
        {
            CAM_TRACE_NAME("applyRelease");
            HDR_LOGD("applyRelease - frameNo(%d) nodeId(%#08" PRIxPTR ")",
                    frameParams.pFrame->getFrameNo(), getNodeId());

            // after this call, all of RELEASE-marked buffers are released by this user
            onPartialFrameDone(frameParams.pFrame);
        }
    }

    // dispatch the main frame (i.e. index is 0) to the next node
    if (mNodeEnqueIndex == 0)
    {
        onDispatchFrame(mvFrameParams[mNodeEnqueIndex].pFrame);

        HDR_LOGD("single process done");
    }

    return OK;
}

MERROR HDRNodeImp::processMultiFrameHDR()
{
    // set pipeline's unique key to hdrproc for debug purpose
    {
        const FrameParams& frameParams(mvFrameParams[mNodeEnqueIndex]);
        IHDRProc2& hdrProc(HDRProc2Factory::getIHDRProc2());
        hdrProc.setParam(
                mHDRHandle,
                HDRProcParam_Set_SequenceNumber,
                frameParams.uniqueKey, 0);
    }

    if (mNodeEnqueIndex == 0)
    {
        // we've already collect all meta from the first enqueued frame,
        // signal HDR proc thread to prepare
        HDR_LOGD("notify HDRProcThread to work");
        mMetatDataInfoCond.signal();

        // we already allocate all hdr src frames when the main-frame enqueued
        CAM_TRACE_NAME("wait_input_frames");
        HDR_LOGD("wait HDR input frames...");
        mHDRInputFrameCond.wait(mHDRInputFrameLock);
        HDR_LOGD("wait HDR input frames done");
    }

    // convert raw to yuv buffers
    if (OK != convertRaw2Yuv())
    {
        HDR_LOGE("convert raw to yuv failed");
    }

    // unlock image/meta input buffer, mark status and then apply buffer to be released
    // TODO: this part is time-consuming and can be executed asynchronously
    if ((mNodeEnqueIndex + 1) == getHDRInputCount())
    {
        CAM_TRACE_NAME("releaseInput");

        for (MINT32 i = 0; i < getHDRInputCount(); i++)
        {
            const FrameParams& frameParams(mvFrameParams[i]);

            for (size_t j = 0; j < frameParams.vpIn.size(); j++)
            {
                HDR_LOGD("release input image buffer i(%d)j(%zu)", i, j);

                FrameInput* frameInput(frameParams.vpIn[j]);

                sp<IImageStreamBuffer>& streamBuffer(frameInput->pStreamBuffer);
                unlockImage(streamBuffer, frameInput->pImageBuffer);
                markImageStream(frameParams.pFrame, streamBuffer);
            }

            if (frameParams.pMeta_InApp != NULL)
            {
                const sp<IMetaStreamBuffer>& streamBuffer(frameParams.pMeta_InAppStreamBuffer);
                unlockMetadata(streamBuffer, frameParams.pMeta_InApp);
                markMetaStream(frameParams.pFrame, streamBuffer);
                HDR_LOGD("release pMeta_InAppStreamBuffer(%#lx)",
                        streamBuffer->getStreamInfo()->getStreamId());
            }

            if (frameParams.pMeta_InHal != NULL)
            {
                const sp<IMetaStreamBuffer>& streamBuffer(frameParams.pMeta_InHalStreamBuffer);
                unlockMetadata(streamBuffer, frameParams.pMeta_InHal);
                markMetaStream(frameParams.pFrame, streamBuffer);
                HDR_LOGD("release pMeta_InHalStreamBuffer(%#lx)",
                        streamBuffer->getStreamInfo()->getStreamId());
            }

            // apply buffers to be released
            {
                CAM_TRACE_NAME("applyRelease");
                HDR_LOGD("applyRelease - frameNo(%d) nodeId(%#08" PRIxPTR ")",
                        frameParams.pFrame->getFrameNo(), getNodeId());

                // after this call, all of RELEASE-marked buffers are released by this user
                onPartialFrameDone(frameParams.pFrame);
            }
        }
    }

    // add input/output frames into HDR proc
    {
        // FRAME_INDEX is uesd to access the YUV and Y8 frames
        const MINT32 FRAME_INDEX = mNodeEnqueIndex << 1;

        IHDRProc2& hdrProc(HDRProc2Factory::getIHDRProc2());

        // add input frames (YUV and Y8)
        for (MUINT32 i = 0; i < 2; i++)
        {
            const MINT32 INDEX = FRAME_INDEX + i;

            HDR_LOGD("add input frame(%d) buffer(%p)",
                    INDEX, mvHDRInputFrames[INDEX].get());

            hdrProc.addInputFrame(mHDRHandle, INDEX, mvHDRInputFrames[INDEX]);
        }

        // output only exist in the main frame (1st queued frame)
        if (mNodeEnqueIndex == 0)
        {
            const FrameParams& frameParams(mvFrameParams[mNodeEnqueIndex]);
            for (size_t i = 0; i < frameParams.vpOut.size(); i++)
            {
                FrameOutput *output(frameParams.vpOut[i]);

                // only support yuv frames of jpeg and thumbnail
                if (!isYuvJpegLocked(output->streamId) &&
                    !isYuvThumbnailLocked(output->streamId))
                {
                    HDR_LOGW("unsupported streamId(%#08" PRIxPTR ")",
                            output->streamId);
                    continue;
                }

                HDROutputType type =
                    isYuvJpegLocked(output->streamId) ?
                    HDR_OUTPUT_JPEG_YUV : HDR_OUTPUT_JPEG_THUMBNAIL_YUV;

                HDR_LOGD("add output frame(%zu) buffer(%p)",
                        i, output->pImageBuffer.get());

                hdrProc.addOutputFrame(mHDRHandle, type, output->pImageBuffer);
            }
        }
    }

    // we've gotten all input and output frames at this round
    // wait unlock buffers after finishing HDR process
    if ((mNodeEnqueIndex + 1) == getHDRInputCount())
    {
        {
            CAM_TRACE_NAME("wait_HDRProc");
            HDR_LOGD("wait for HDR post-processing...");
            mHDRProcCompleteCond2.wait(mHDRProcCompleteLock2);
            HDR_LOGD("finish HDR post-processing");
        }

        handleDigitalZoom(mHdrResult);

        ImageBufferUtils::getInstance().deallocBuffer(mHdrResult);
        HDR_LOGD("dealloc mHdrResult buffer(%p)", mHdrResult.get());
        mHdrResult = NULL;

        // generate debug information
        if (mvFrameParams[mNodeEnqueIndex].pMeta_InHal)
        {
            MUINT8 isRequestExif = 0;
            tryGetMetadata<MUINT8>(mvFrameParams[mNodeEnqueIndex].pMeta_InHal,
                    MTK_HAL_REQUEST_REQUIRE_EXIF, isRequestExif);

            if ((isRequestExif != 0) &&
                makeDebugInfo(mvFrameParams[0].pMeta_OutHal) != MTRUE)
            {
                HDR_LOGW("cannnot make debug information");
            }
        }

        // unlock image/meta output buffer, mark status and then apply buffer to be released
        for (MINT32 i = 0; i < getHDRInputCount(); i++)
        {
            CAM_TRACE_NAME("releaseOutput");

            const FrameParams& frameParams(mvFrameParams[i]);

            for (size_t j = 0; j < frameParams.vpOut.size(); j++)
            {
                HDR_LOGD("release output image buffer i(%d)j(%zu)", i, j);

                FrameOutput* frameOutput(frameParams.vpOut[j]);

                sp<IImageStreamBuffer>& streamBuffer(frameOutput->pStreamBuffer);
                unlockImage(streamBuffer, frameOutput->pImageBuffer);
                markImageStream(frameParams.pFrame, streamBuffer);
            }

            if (frameParams.pMeta_OutApp != NULL)
            {
                const sp<IMetaStreamBuffer>& streamBuffer(frameParams.pMeta_OutAppStreamBuffer);
                unlockMetadata(streamBuffer, frameParams.pMeta_OutApp);
                markMetaStream(frameParams.pFrame, streamBuffer);
                HDR_LOGD("release pMeta_OutAppStreamBuffer(%#lx)",
                        streamBuffer->getStreamInfo()->getStreamId());
            }

            if (frameParams.pMeta_OutHal != NULL)
            {
                const sp<IMetaStreamBuffer>& streamBuffer(frameParams.pMeta_OutHalStreamBuffer);
                unlockMetadata(streamBuffer, frameParams.pMeta_OutHal);
                markMetaStream(frameParams.pFrame, streamBuffer);
                HDR_LOGD("release pMeta_OutHalStreamBuffer(%#lx)",
                        streamBuffer->getStreamInfo()->getStreamId());
            }

            // apply buffers to be released
            {
                CAM_TRACE_NAME("applyRelease");
                HDR_LOGD("applyRelease - frameNo(%d) nodeId(%#08" PRIxPTR ")",
                        frameParams.pFrame->getFrameNo(), getNodeId());

                // after this call, all of RELEASE-marked buffers are released by this user
                onPartialFrameDone(frameParams.pFrame);
            }
        }

        // dispatch the main frame (i.e. index is 0) to the next node
        onDispatchFrame(mvFrameParams[0].pFrame);
    }

    return OK;
}

MERROR HDRNodeImp::handleDigitalZoom(const sp<IImageBuffer>& captureBuffer)
{
    if (captureBuffer == NULL)
    {
        HDR_LOGE("capture buffer is NULL");
        return BAD_VALUE;
    }

    MERROR err = OK;

    // complet in/out buffer/meta-data will be save in the first array entry
    const MINT32 mainRequestIndex = 0;

    MINT32 orientation = 0;
    const MSize& dstSize = captureBuffer->getImgSize();

    // get app/hal metadata
    CAM_TRACE_NAME("getMetadata");

    IMetadata* dynamic_app_meta(mvFrameParams[mainRequestIndex].pMeta_InApp);
    IMetadata* dynamic_hal_meta(mvFrameParams[mainRequestIndex].pMeta_InHal);

    // the clockwise rotation angle in degrees,
    // relative to the orientation to the camera
    tryGetMetadata<MINT32>(
            dynamic_app_meta, MTK_JPEG_ORIENTATION, orientation);
    HDR_LOGD("jpeg orientation(%d)", orientation);

    HDR_LOGD("dstSize(%dx%d)", dstSize.w, dstSize.h);

    sp<IImageBuffer> yuvJpeg;
    sp<IImageBuffer> yuvThumbnail;
    {
        const FrameParams& frameParams(mvFrameParams[mainRequestIndex]);
        for (size_t i = 0; i < frameParams.vpOut.size(); i++)
        {
            FrameOutput *output(frameParams.vpOut[i]);

            // only support yuv frames of jpeg and thumbnail
            if (!isYuvJpegLocked(output->streamId) &&
                    !isYuvThumbnailLocked(output->streamId))
            {
                HDR_LOGW("unsupported streamId(%#08" PRIxPTR ")",
                        output->streamId);
                continue;
            }

            if (isYuvJpegLocked(output->streamId))
                yuvJpeg = output->pImageBuffer;
            else
                yuvThumbnail = output->pImageBuffer;

            HDR_LOGD("add output frame(%zu) buffer(%p)",
                    i, output->pImageBuffer.get());
        }
    }

    if ((yuvJpeg == NULL) || (yuvThumbnail == NULL))
    {
        HDR_LOGE("yuv jpeg(%p) or yuv thumbnail(%p) is NULL",
                yuvJpeg.get(), yuvThumbnail.get());
        return BAD_VALUE;
    }

    // scaler crop region
    MRect cropRegion;
    getScalerCropRegion(cropRegion, dstSize);

    // write result to jpeg and thumbnail buffer
    {
        MUINT32 transform = [](const MINT32 orientation) -> MUINT32
        {
            switch (orientation)
            {
                case 0:
                    return 0;
                case 90:
                    return eTransform_ROT_90;
                case 180:
                    return eTransform_ROT_180;
                case 270:
                    return eTransform_ROT_270;
                default:
                    HDR_LOGW("invalid orientation(%d), do nothing", orientation);
                    return 0;
            }
        }(orientation);

        HDR_LOGD("result(%dx%d) -> jpeg(%dx%d) thumbnail(%dx%d) " \
                "crop(%d, %d, %dx%d) transform(%d)",
                captureBuffer->getImgSize().w, captureBuffer->getImgSize().h,
                yuvJpeg->getImgSize().w, yuvJpeg->getImgSize().h,
                yuvThumbnail->getImgSize().w, yuvThumbnail->getImgSize().h,
                cropRegion.p.x, cropRegion.p.y, cropRegion.s.w, cropRegion.s.h,
                transform);

        // the aspect ratios between sensor and jpeg may be different,
        // so adjust the aspect ratio of the source crop region to be the same as jpeg's
        {
            // due to the dimension of hdr output frame has been swapped,
            // in order to calculate the aspect ratio, we need to restore it
            MRect dstRect =
                (transform & eTransform_ROT_90) ?
                MRect(yuvJpeg->getImgSize().h, yuvJpeg->getImgSize().w) :
                MRect(yuvJpeg->getImgSize().w, yuvJpeg->getImgSize().h);

            // recalculate the final crop
            cropRegion = calCrop(cropRegion, dstRect, MFALSE);

            // let the final crop to be centered within the hdr result's buffer
            cropRegion.p.x = (captureBuffer->getImgSize().w - cropRegion.s.w) / 2;
            cropRegion.p.y = (captureBuffer->getImgSize().h - cropRegion.s.h) / 2;

            HDR_LOGD("cropRegion(%d, %d, %dx%d) dst(%d, %d, %dx%d)",
                    cropRegion.p.x, cropRegion.p.y, cropRegion.s.w, cropRegion.s.h,
                    dstRect.p.x, dstRect.p.y, dstRect.s.w, dstRect.s.h);
        }

        // handle digital zoom
        MFCNodeImp::generateOutputYuv(
            mOpenId,
            captureBuffer.get(),
            yuvJpeg.get(),
            yuvThumbnail.get(),
            cropRegion,
            cropRegion,
            transform,
            dynamic_app_meta,
            dynamic_hal_meta);
    }

    return err;
}

MBOOL HDRNodeImp::makeDebugInfo(IMetadata* metadata)
{
    CAM_TRACE_CALL();

    // add HDR image flag
    std::map<MUINT32, MUINT32> debugInfoList;
    debugInfoList[MF_TAG_IMAGE_HDR] = 1;

    // get debug Exif metadata
    IMetadata exifMetadata;
    tryGetMetadata<IMetadata>(metadata, MTK_3A_EXIF_METADATA, exifMetadata);

    // set debug information into debug Exif metadata
    if (DebugExifUtils::setDebugExif(
                DebugExifUtils::DebugExifType::DEBUG_EXIF_MF,
                static_cast<MUINT32>(MTK_MF_EXIF_DBGINFO_MF_KEY),
                static_cast<MUINT32>(MTK_MF_EXIF_DBGINFO_MF_DATA),
                debugInfoList,
                &exifMetadata) == nullptr)
        return MFALSE;

    // update debug Exif metadata
    updateEntry<IMetadata>(metadata, MTK_3A_EXIF_METADATA, exifMetadata);

    return MTRUE;
}

MVOID HDRNodeImp::onPartialFrameDone(const sp<IPipelineFrame>& frame)
{
    CAM_TRACE_CALL();
    frame->getStreamBufferSet().applyRelease(getNodeId());
}

MERROR HDRNodeImp::verifyConfigParams(ConfigParams const & rParams) const
{
    if (!rParams.pInAppMeta.get())
    {
        HDR_LOGE("out in app metadata");
        return BAD_VALUE;
    }

    if (!rParams.pInHalMeta.get())
    {
        HDR_LOGE("no in hal metadata");
        return BAD_VALUE;
    }

    if (rParams.vInFullRaw.size() == 0)
    {
        HDR_LOGE("no in image fullraw");
        return BAD_VALUE;
    }

    // resized raw can be null; just log for notification
    if (!rParams.pInResizedRaw.get())
    {
        HDR_LOGD("no in resized raw");
    }

    // lcso is not a necessary raw buffer
    HDR_LOGD("%s lcso", (rParams.vInLcsoRaw.size() == 0) ? "disable" : "enable");

    if ((rParams.vOutYuvJpeg == NULL) || (rParams.vOutYuvThumbnail == NULL))
    {
        HDR_LOGE("no out yuvJpeg(%#" PRIxPTR ") yuvThumbnail(%#" PRIxPTR ")",
                reinterpret_cast<uintptr_t>(rParams.vOutYuvJpeg.get()),
                reinterpret_cast<uintptr_t>(rParams.vOutYuvThumbnail.get()));
        return BAD_VALUE;
    }

    if (rParams.vOutImage.size() == 0)
    {
        HDR_LOGD("no out yuv image");
    }

    // dump all streams
    size_t count;

    dumpStreamIfExist("[meta] in app", rParams.pInAppMeta);
    dumpStreamIfExist("[meta] in hal", rParams.pInHalMeta);
    dumpStreamIfExist("[meta] out app", rParams.pOutAppMeta);
    dumpStreamIfExist("[meta] out hal", rParams.pOutHalMeta);

    count = rParams.vInFullRaw.size();
    for (size_t i = 0; i < count; i++)
    {
        dumpStreamIfExist("[image] in full", rParams.vInFullRaw[i]);
    }

    dumpStreamIfExist("[image] in resized", rParams.pInResizedRaw);

    for (size_t i = 0; i < rParams.vInLcsoRaw.size(); i++)
    {
        dumpStreamIfExist("[image] in lcso", rParams.vInLcsoRaw[i]);
    }

    count = rParams.vOutImage.size();
    for (size_t i = 0; i < count; i++)
    {
        dumpStreamIfExist("[image] out yuv", rParams.vOutImage[i]);
    }

    return OK;
}

MERROR HDRNodeImp::getInfoIOMapSet(
        sp<IPipelineFrame> const& pFrame,
        IPipelineFrame::InfoIOMapSet& rIOMapSet,
        MINT32 /*frameIndex*/) const
{
    // get PipelineFrame's InfoIOMapSet
    if (OK != pFrame->queryInfoIOMapSet(getNodeId(), rIOMapSet))
    {
        HDR_LOGE("queryInfoIOMap failed");
        return NAME_NOT_FOUND;
    }

    const MUINT32 FRAME_NO = pFrame->getFrameNo();

    // check image part
    IPipelineFrame::ImageInfoIOMapSet& imageIOMapSet = rIOMapSet.mImageInfoIOMapSet;
    if (imageIOMapSet.size() == 0)
    {
        HDR_LOGW("no imageIOMap in frame");
        return BAD_VALUE;
    }

    for (size_t i = 0; i < imageIOMapSet.size(); i++)
    {
        IPipelineFrame::ImageInfoIOMap const& imageIOMap = imageIOMapSet[i];

        // return if no any input
        if (imageIOMap.vIn.size() <= 0)
        {
            HDR_LOGE("[image] #%zu: wrong size vIn %zu", i, imageIOMap.vIn.size());
            return BAD_VALUE;
        }

        HDR_LOGD_IF(mLogLevel >= 1, "ImageInfoIOMapSet(%zu) frameNo(%u) In(%zu) Out(%zu)",
                i, FRAME_NO, imageIOMap.vIn.size(), imageIOMap.vOut.size());
    }

    // check metadata part
    IPipelineFrame::MetaInfoIOMapSet& metaIOMapSet = rIOMapSet.mMetaInfoIOMapSet;
    if (metaIOMapSet.size() == 0)
    {
        HDR_LOGW("no metaIOMap in frame");
        return BAD_VALUE;
    }

    for (size_t i = 0; i < metaIOMapSet.size(); i++)
    {
        IPipelineFrame::MetaInfoIOMap const& metaIOMap = metaIOMapSet[i];

        // return if app's meta request is empty or cannot be found in the current MetaInfoIOMap
        if (!mpInAppMeta_Request.get() ||
                metaIOMap.vIn.indexOfKey(mpInAppMeta_Request->getStreamId()) < 0)
        {
            HDR_LOGE("[meta] #%zu: app's meta request is empty or cannot be found", i);
            return BAD_VALUE;
        }

        // return if P1's meta request is empty or cannot be found in the current MetaInfoIOMap
        if (!mpInHalMeta_P1.get() ||
                metaIOMap.vIn.indexOfKey(mpInHalMeta_P1->getStreamId()) < 0)
        {
            HDR_LOGE("[meta] #%zu: P1's meta request is empty or cannot be found", i);
            return BAD_VALUE;
        }

        HDR_LOGD_IF(mLogLevel >= 1, "MetaInfoIOMapSet(%zu) frameNo(%u) In(%zu) Out(%zu)",
                i, FRAME_NO, metaIOMap.vIn.size(), metaIOMap.vOut.size());
    }

    return OK;
}

MBOOL HDRNodeImp::getScalerCropRegion(
        MRect& cropRegion, const MSize& dstSize) const
{
    MRect origCropRegion;

    // query crop region (pixel coordinate is relative to active array)
    IMetadata* dynamic_app_meta = mvFrameParams[0].pMeta_InApp;
    if (!tryGetMetadata<MRect>(dynamic_app_meta, MTK_SCALER_CROP_REGION, origCropRegion))
    {
        // set crop region to full size
        origCropRegion.p = MPoint(0, 0);
        origCropRegion.s = mActiveArray.s;
        HDR_LOGW("no MTK_SCALER_CROP_REGION, set crop region to full size %dx%d",
                origCropRegion.s.w, origCropRegion.s.h);
    }

    // setup transform (active arrary -> sensor)
    simpleTransform tranActive2Sensor =
        simpleTransform(MPoint(0, 0), mActiveArray.s, dstSize);

    // apply transform
    cropRegion = transform(tranActive2Sensor, origCropRegion);

    HDR_LOGD("cropRegion(%d, %d, %dx%d)",
            cropRegion.p.x, cropRegion.p.y, cropRegion.s.w, cropRegion.s.h);

    return MTRUE;
}

inline MBOOL HDRNodeImp::isStream(const sp<IStreamInfo>& streamInfo,
        StreamId_T streamId) const
{
    return streamInfo.get() && (streamInfo->getStreamId() == streamId);
}

inline MBOOL HDRNodeImp::isFullRawLocked(StreamId_T const streamId) const
{
    return isStream(mpInFullRaw, streamId);
}

inline MBOOL HDRNodeImp::isResizedRawLocked(StreamId_T const streamId) const
{
    return isStream(mpInResizedRaw, streamId);
}

inline MBOOL HDRNodeImp::isLcsoRawLocked(StreamId_T const streamId) const
{
    return isStream(mpInLcsoRaw, streamId);
}

inline MBOOL HDRNodeImp::isYuvJpegLocked(StreamId_T const streamId) const
{
    return isStream(mpOutYuvJpeg, streamId);
}

inline MBOOL HDRNodeImp::isYuvThumbnailLocked(StreamId_T const streamId) const
{
    return isStream(mpOutYuvThumbnail, streamId);
}

MBOOL HDRNodeImp::isInImageStream(StreamId_T const streamId) const
{
    RWLock::AutoRLock _l(mConfigRWLock);

    if (isFullRawLocked(streamId) || isResizedRawLocked(streamId))
        return MTRUE;

    HDR_LOGD_IF(0, "streamId(%#08" PRIxPTR ") is not in-stream", streamId);
    return MFALSE;
}

MBOOL HDRNodeImp::isInMetaStream(StreamId_T const streamId) const
{
    RWLock::AutoRLock _l(mConfigRWLock);

    return isStream(mpInAppMeta_Request, streamId) || isStream(mpInHalMeta_P1, streamId);
}

MERROR HDRNodeImp::onDequeRequest(android::sp<IPipelineFrame>& rpFrame)
{
    CAM_TRACE_NAME("onDequeueRequest(HDR)");

    Mutex::Autolock _l(mRequestQueueLock);
    //  Wait until the queue is not empty or this thread will exit
    while (mRequestQueue.empty() && !mbRequestExit)
    {
        // enable drained flag
        mbRequestDrained = MTRUE;
        mbRequestDrainedCond.signal();

        status_t status = mRequestQueueCond.wait(mRequestQueueLock);
        if (OK != status)
        {
            HDR_LOGW("wait status(%d):%s, mRequestQueue.size(%zu)",
                    status, ::strerror(-status), mRequestQueue.size());
        }
    }

    // warn if request queue is not empty
    if (mbRequestExit)
    {
        HDR_LOGE_IF(!mRequestQueue.empty(), "[flush] mRequestQueue.size(%zu)",
                mRequestQueue.size());
        return DEAD_OBJECT;
    }

    // request queue is not empty, take the first request from the queue
    mbRequestDrained = MFALSE;
    rpFrame = *mRequestQueue.begin();
    mRequestQueue.erase(mRequestQueue.begin());

    CAM_TRACE_INT("request(hdr)", mRequestQueue.size());

    return OK;
}

MVOID HDRNodeImp::waitForRequestDrained() const
{
    CAM_TRACE_CALL();

    Mutex::Autolock _l(mRequestQueueLock);
    if (!mbRequestDrained)
    {
        HDR_LOGD("wait for request drained...");
        mbRequestDrainedCond.wait(mRequestQueueLock);
    }
}

MBOOL HDRNodeImp::HDRProcCompleteCallback(
        MVOID* user, const sp<IImageBuffer>& hdrResult, MBOOL ret)
{
    HDRNodeImp* self = reinterpret_cast<HDRNodeImp*>(user);
    if (NULL == self)
    {
        HDR_LOGE("HDRProcCompleteCallback with NULL user");
        return MFALSE;
    }

    HDR_LOGD("HDRProcCompleteCallback ret(%d)", ret);

    self->mHdrResult = hdrResult;

    HDR_LOGD("signal mHDRProcCompleteLock");
    self->mHDRProcCompleteCond.signal();
    HDR_LOGD("signal mHDRProcCompleteLock2");
    self->mHDRProcCompleteCond2.signal();

    return MTRUE;
}

MVOID HDRNodeImp::pass2CbFunc(QParams& rParams)
{
    HDRNodeImp* self = reinterpret_cast<HDRNodeImp*>(rParams.mpCookie);
    self->handleDeque(rParams);
}

MVOID HDRNodeImp::handleDeque(QParams& rParams)
{
    CAM_TRACE_NAME("p2 deque");

    if (rParams.mvTuningData.size() > 0)
    {
        void* pTuning = rParams.mvTuningData[0];
        if (pTuning)
        {
            free(pTuning);
        }
    }

    HDR_LOGD("signal P2");
    mP2Cond.signal();
}

MBOOL HDRNodeImp::createPipe()
{
    MBOOL ret = MTRUE;

    mpPipe = INormalStream::createInstance(mOpenId);
    if (mpPipe == NULL)
    {
        HDR_LOGE("create normal stream failed");
        ret = MFALSE;
        goto lbExit;
    }

    if (!mpPipe->init(LOG_TAG))
    {
        HDR_LOGE("initialize normal stream failed");
        ret = MFALSE;
        goto lbExit;
    }

    mp3A = MAKE_Hal3A(
            mOpenId, LOG_TAG);
    if (mp3A == NULL)
    {
        HDR_LOGE("create 3A HAL failed");
        ret = MFALSE;
        goto lbExit;
    }

    HDR_LOGD("pipe created: pipe(%p) 3AHal(%p)", mpPipe, mp3A);

lbExit:
    // fall back if cannot create pipe
    if ((mpPipe == NULL) || (mp3A == NULL))
    {
        destroyPipe();
    }

    return ret;
}

MVOID HDRNodeImp::destroyPipe()
{
    if (mpPipe)
    {
        // uninit pipe
        if (!mpPipe->uninit(LOG_TAG))
        {
            HDR_LOGE("pipe uninit failed");
        }

        mpPipe->destroyInstance();
        mpPipe = NULL;
    }

    if (mp3A)
    {
        mp3A->destroyInstance(LOG_TAG);
        mp3A = NULL;
    }

    HDR_LOGD("pipe destroyed");
}

MERROR HDRNodeImp::getImageBuffer(
    MINT32 type,
    StreamId_T const streamId,
    sp<IImageStreamBuffer>& streamBuffer,
    sp<IImageBufferHeap>& imageBufferHeap,
    sp<IImageBuffer>& imageBuffer)
{
    // query the group usage from UsersManager
    MUINT const groupUsage =
        streamBuffer->queryGroupUsage(getNodeId());

    // get image buffer heap from streambuffer (with read or write lock locked)
    switch (type & (IN | OUT))
    {
        case IN:
            imageBufferHeap = streamBuffer->tryReadLock(getNodeName());
            break;
        case OUT:
            imageBufferHeap = streamBuffer->tryWriteLock(getNodeName());
            break;
        default:
            HDR_LOGE("    invalid buffer type(%d)", type);
    }

    if (imageBufferHeap == NULL)
    {
        HDR_LOGE("    node(%ld) type(%d) stream buffer(%s): cannot get imageBufferHeap",
                getNodeId(), type, streamBuffer->getName());
        return INVALID_OPERATION;
    }

    // create imagebuffer from image buffer heap
    imageBuffer = imageBufferHeap->createImageBuffer();
    if (imageBuffer == NULL)
    {
        HDR_LOGE("    node(%ld) type(%d) stream buffer(%s): cannot create imageBuffer",
                getNodeId(), type, streamBuffer->getName());
        return INVALID_OPERATION;
    }

    // a buffer is allowed to access only between the interval of
    // lockBuf() and unlockBuf()
    imageBuffer->lockBuf(getNodeName(), groupUsage);
    HDR_LOGD_IF(mLogLevel >= 1, "    streamId(%#08" PRIxPTR ") buffer(%#" PRIxPTR ")"
            " usage(%#x) type(0x%x)",
            streamId, reinterpret_cast<uintptr_t>(imageBuffer.get()),
            groupUsage, type);

    return OK;
}

MERROR HDRNodeImp::getMetadata(
    MINT32 type,
    StreamId_T const streamId,
    sp<IMetaStreamBuffer>& streamBuffer,
    IMetadata*& metadata)
{
    // get metadata from meta stream buffer
    switch (type & (IN | OUT))
    {
        case IN:
            metadata = streamBuffer->tryReadLock(getNodeName());
            break;
        case OUT:
            metadata = streamBuffer->tryWriteLock(getNodeName());
    }

    if (metadata == NULL)
    {
        HDR_LOGE("      node(%ld) stream buffer(%s): cannot get app metadata",
                getNodeId(), streamBuffer->getName());
        return INVALID_OPERATION;
    }

    HDR_LOGD_IF(mLogLevel >= 1, "      streamId(%#lx) metadata(%p) type(0x%x)",
            streamId, metadata, type);

    return OK;
}

MVOID HDRNodeImp::markImageStream(
        sp<IPipelineFrame> const& pFrame,
        sp<IImageStreamBuffer> const pStreamBuffer) const
{
    if ((pFrame == NULL) || (pStreamBuffer == NULL))
    {
        HDR_LOGE("pFrame(%#" PRIxPTR ") or pStreamBuffer(%#" PRIxPTR ")"
                " should not be NULL",
                reinterpret_cast<uintptr_t>(pFrame.get()),
                reinterpret_cast<uintptr_t>(pStreamBuffer.get()));
        return;
    }

    IStreamBufferSet& streamBufferSet = pFrame->getStreamBufferSet();
    StreamId_T const streamId = pStreamBuffer->getStreamInfo()->getStreamId();

    // buffer Producer must set this status
    if (isInImageStream(streamId))
    {
        pStreamBuffer->markStatus(STREAM_BUFFER_STATUS::WRITE_OK);
    }

    // mark this buffer as USED by this user
    // mark this buffer as RELEASE by this user
    streamBufferSet.markUserStatus(
            streamId,
            getNodeId(),
            IUsersManager::UserStatus::USED |
            IUsersManager::UserStatus::RELEASE);
}

MVOID HDRNodeImp::markMetaStream(
        android::sp<IPipelineFrame> const& pFrame,
        sp<IMetaStreamBuffer> const pStreamBuffer) const
{
    if ((pFrame == NULL) || (pStreamBuffer == NULL))
    {
        HDR_LOGE("pFrame(%#" PRIxPTR ") or pStreamBuffer(%#" PRIxPTR ")"
                " should not be NULL",
                reinterpret_cast<uintptr_t>(pFrame.get()),
                reinterpret_cast<uintptr_t>(pStreamBuffer.get()));
        return;
    }

    IStreamBufferSet& rStreamBufferSet = pFrame->getStreamBufferSet();
    StreamId_T const streamId = pStreamBuffer->getStreamInfo()->getStreamId();


    // buffer Producer must set this status
    if (!isInMetaStream(streamId))
    {
        pStreamBuffer->markStatus(STREAM_BUFFER_STATUS::WRITE_OK);
    }

    // mark this buffer as USED by this user
    // mark this buffer as RELEASE by this user
    rStreamBufferSet.markUserStatus(
            streamId,
            getNodeId(),
            IUsersManager::UserStatus::USED |
            IUsersManager::UserStatus::RELEASE);
}

MVOID HDRNodeImp::unlockImage(
        sp<IImageStreamBuffer> const& pStreamBuffer,
        sp<IImageBuffer> const& pImageBuffer) const
{
    if (pStreamBuffer == NULL || pImageBuffer == NULL)
    {
        HDR_LOGE("pStreamBuffer(%p) or pImageBuffer(%p) should not be NULL",
               pStreamBuffer.get(), pImageBuffer.get());
        return;
    }

    pImageBuffer->unlockBuf(getNodeName());
    pStreamBuffer->unlock(getNodeName(), pImageBuffer->getImageBufferHeap());
}

MVOID HDRNodeImp::unlockMetadata(
        sp<IMetaStreamBuffer> const& pStreamBuffer,
        IMetadata* const pMetadata) const
{
    if (pStreamBuffer == NULL || pMetadata == NULL)
    {
        HDR_LOGE("pStreamBuffer(%p) or pMetadata(%p) should not be NULL",
                pStreamBuffer.get(), pMetadata);
        return;
    }

    pStreamBuffer->unlock(getNodeName(), pMetadata);
}

// ---------------------------------------------------------------------------

HDRNodeImp::ThreadBase::ThreadBase(const char* name, const sp<HDRNodeImp>& pNodeImp)
    : mpNodeImp(pNodeImp)
{
    snprintf(mThreadName, sizeof(mThreadName), "%s", name);
}

MERROR HDRNodeImp::ThreadBase::threadSetting(const char* threadName)
{
    //  set thread policy & priority
    //  NOTE:
    //  Even if pthread_create() with SCHED_OTHER policy, a newly-created thread
    //  may inherit the non-SCHED_OTHER policy & priority of the thread creator.
    //  And thus, we must set the expected policy & priority after a thread creation.

    // retrieve the parameters of the calling process
    struct sched_param schedParam;
    sched_getparam(0, &schedParam);

    if (setpriority(PRIO_PROCESS, 0, P2THREAD_PRIORITY))
    {
        HDR_LOGW("set priority failed(%s)", strerror(errno));
    }

    schedParam.sched_priority = 0;
    sched_setscheduler(0, P2THREAD_POLICY, &schedParam);

    HDR_LOGD("thread(%s) tid(%d) policy(%d) priority(%d)",
            threadName, gettid(), P2THREAD_POLICY, P2THREAD_PRIORITY);

    return OK;
}

// ---------------------------------------------------------------------------

HDRNodeImp::HDRThread::HDRThread(const char* name, const sp<HDRNodeImp>& pNodeImp)
    : ThreadBase(name, pNodeImp)
{
}

status_t HDRNodeImp::HDRThread::readyToRun()
{
    return threadSetting(mThreadName);
}

void HDRNodeImp::HDRThread::requestExit()
{
    CAM_TRACE_CALL();

    HDR_LOGD("request exit(%s)", mThreadName);

    Mutex::Autolock _l(mpNodeImp->mRequestQueueLock);
    mpNodeImp->mbRequestExit = MTRUE;
    mpNodeImp->mRequestQueueCond.signal();
}

bool HDRNodeImp::HDRThread::threadLoop()
{
    CAM_TRACE_NAME("threadLoop(HDR)");

    sp<IPipelineFrame> pFrame;
    if (!exitPending() &&
        (OK == mpNodeImp->onDequeRequest(pFrame)) &&
        (pFrame != 0))
    {
        HDR_LOGD("onProcessFrame %d", mpNodeImp->mNodeEnqueIndex);
        mpNodeImp->onProcessFrame(pFrame);

        return true;
    }

    HDR_LOGD("exit hdr thread");
    return false;
}

// ---------------------------------------------------------------------------

HDRNodeImp::HDRProcThread::HDRProcThread(
        const char* name, const sp<HDRNodeImp>& pNodeImp)
    : ThreadBase(name, pNodeImp)
    , mRequestExit(MFALSE)
{
}

status_t HDRNodeImp::HDRProcThread::readyToRun()
{
    return threadSetting(mThreadName);
}

bool HDRNodeImp::HDRProcThread::threadLoop()
{
    CAM_TRACE_NAME("threadLoop(HDRProc)");

    MSize size_sensor;

    // wait for P1 to enqueue the first frame with the destination metadata
    {
        CAM_TRACE_NAME("wait_P1_enqueue");
        HDR_LOGD("wait for P1 enqueue...");
        mpNodeImp->mMetatDataInfoCond.wait(mpNodeImp->mMetatDataInfoLock);

        Mutex::Autolock _l(mRequestExitLock);
        if (CC_UNLIKELY(mRequestExit == MTRUE))
        {
            HDR_LOGD("exit hdr proc thread");
            return false;
        }

        // we've already collect all meta from the first enqueued frame
        HDR_LOGD("wait P1 done");
    }

    // single frame flow, exit hdr proc thread directly
    if (CC_UNLIKELY(mpNodeImp->isSingleFrame()))
        return false;

    // get app/hal metadata
    {
        CAM_TRACE_NAME("getMetadata");

        IMetadata* dynamic_app_meta(mpNodeImp->mvFrameParams[0].pMeta_InApp);
        IMetadata* dynamic_hal_meta(mpNodeImp->mvFrameParams[0].pMeta_InHal);

        // sensor size
        tryGetMetadata<MSize>(
                dynamic_hal_meta, MTK_HAL_REQUEST_SENSOR_SIZE, size_sensor);
        HDR_LOGD("hal request sensor size(%dx%d)", size_sensor.w, size_sensor.h);
    }

    prepareMultiFrameHDR(size_sensor);

    return false;
}

void HDRNodeImp::HDRProcThread::requestExit()
{
    CAM_TRACE_CALL();

    HDR_LOGD("request exit(%s)", mThreadName);

    Mutex::Autolock _l(mRequestExitLock);
    mRequestExit = MTRUE;
    mpNodeImp->mMetatDataInfoCond.signal();
}


status_t HDRNodeImp::HDRProcThread::prepareMultiFrameHDR(MSize& size_sensor)
{
    IHDRProc2& hdrProc(HDRProc2Factory::getIHDRProc2());
    const HDRHandle& hdrHandle(mpNodeImp->mHDRHandle);

    // scaler crop region
    MRect cropRegion;
    mpNodeImp->getScalerCropRegion(cropRegion, size_sensor);

    // TODO: set the corresponding postview size
    MSize postviewSize(800, 600);
    hdrProc.setShotParam(hdrHandle, size_sensor, postviewSize, cropRegion);

    hdrProc.setParam(
            hdrHandle,
            HDRProcParam_Set_sensor_size, size_sensor.w, size_sensor.w);

    // currently HAL 3 does not support YUV sensor, so we force RAW as the sensor type
    hdrProc.setParam(hdrHandle, HDRProcParam_Set_sensor_type, SENSOR_TYPE_RAW, 0);

    hdrProc.prepare(hdrHandle);

    MUINT32 uSrcMainFormat = 0;
    MUINT32 uSrcMainWidth = 0;
    MUINT32 uSrcMainHeight = 0;
    MUINT32 uSrcSmallFormat = 0;
    MUINT32 uSrcSmallWidth = 0;
    MUINT32 uSrcSmallHeight = 0;
    MUINT32 empty = 0;

    hdrProc.getParam(
            hdrHandle,
            HDRProcParam_Get_src_main_format, uSrcMainFormat, empty);
    hdrProc.getParam(
            hdrHandle,
            HDRProcParam_Get_src_main_size, uSrcMainWidth, uSrcMainHeight);
    hdrProc.getParam(
            hdrHandle,
            HDRProcParam_Get_src_small_format, uSrcSmallFormat, empty);
    hdrProc.getParam(
            hdrHandle,
            HDRProcParam_Get_src_small_size, uSrcSmallWidth, uSrcSmallHeight);

    const MINT32 FRAME_NUM = mpNodeImp->getHDRInputCount() << 1;

    // each HDR input frame requires one main YUV and one small Y8 frame
    {
        CAM_TRACE_NAME("allocateInputBuffer");

        for (MINT32 i = 0; i < FRAME_NUM; i++)
        {
            EImageFormat inputImageFormat;
            MUINT32 inputImageWidth;
            MUINT32 inputImageHeight;

            // set buffer's format and dimension
            // please note that the index ordering should be
            // {0, 2, 4, ...} for main YUV and {1, 3, 5, ...} for small Y8 frames
            if ((i & 0x1) == 0)
            {
                // main YUV
                inputImageFormat = (EImageFormat)uSrcMainFormat;
                inputImageWidth  = uSrcMainWidth;
                inputImageHeight = uSrcMainHeight;
            }
            else
            {
                // small Y8
                inputImageFormat = (EImageFormat)uSrcSmallFormat;
                inputImageWidth  = uSrcSmallWidth;
                inputImageHeight = uSrcSmallHeight;
            }

            HDR_LOGD("input working buffer(%d) format(0x%x) size(%dX%d)",
                    i, inputImageFormat, inputImageWidth, inputImageHeight);

            // allocate working buffer
            sp<IImageBuffer> imageBuffer;
            ImageBufferUtils::getInstance().allocBuffer(
                    imageBuffer,
                    inputImageWidth, inputImageHeight, inputImageFormat);
            if (imageBuffer == NULL)
            {
                HDR_LOGE("image buffer is NULL");
                return false;
            }

            mpNodeImp->mvHDRInputFrames.push_back(imageBuffer);

            HDR_LOGD("alloc input working buffer(%d:%p) size(%dx%d) format(0x%x)",
                    i, imageBuffer.get(),
                    inputImageWidth, inputImageHeight, inputImageFormat);
        }
    }

    // HDR proc start wait src yuv and dst yuv frame input by hdrnode
    {
        CAM_TRACE_NAME("start_HDRProc");
        HDR_LOGD("start HDR proc");
        hdrProc.start(hdrHandle);
        mpNodeImp->mHDRInputFrameCond.signal();
    }

    {
        CAM_TRACE_NAME("wait_HDRProc_done");
        HDR_LOGD("wait for HDR proc to complete...");
        mpNodeImp->mHDRProcCompleteCond.wait(mpNodeImp->mHDRProcCompleteLock);
        HDR_LOGD("wait HDR process done");
    }

    for (MINT32 i = 0; i < FRAME_NUM; i++)
    {
        sp<IImageBuffer> imageBuffer(mpNodeImp->mvHDRInputFrames[i]);
        ImageBufferUtils::getInstance().deallocBuffer(imageBuffer);
        HDR_LOGV("dealloc input working buffer(%d)", i);
    }

    hdrProc.release(hdrHandle);

    hdrProc.uninit(hdrHandle);

    HDR_LOGD("HDR process done");

    return OK;
}
