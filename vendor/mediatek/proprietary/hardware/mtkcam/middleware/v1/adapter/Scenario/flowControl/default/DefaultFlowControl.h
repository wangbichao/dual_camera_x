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

#ifndef _MTK_HARDWARE_MTKCAM_ADAPTER_DEFAULT_DEFAULTFLOWCONTROL_H_
#define _MTK_HARDWARE_MTKCAM_ADAPTER_DEFAULT_DEFAULTFLOWCONTROL_H_
//
#include <utils/RefBase.h>
#include <utils/StrongPointer.h>
#include <utils/String8.h>
#include <utils/Vector.h>
//
#include <mtkcam/utils/metadata/IMetadata.h>
#include <mtkcam/pipeline/utils/streambuf/StreamBuffers.h>
#include <mtkcam/pipeline/utils/streambuf/StreamBufferProvider.h>
#include <mtkcam/middleware/v1/LegacyPipeline/ILegacyPipeline.h>
#include <mtkcam/middleware/v1/Scenario/IFlowControl.h>
#include <mtkcam/middleware/v1/LegacyPipeline/request/IRequestController.h>
#include <mtkcam/middleware/v1/LegacyPipeline/buffer/StreamBufferProvider.h>
#include <mtkcam/middleware/v1/LegacyPipeline/IResourceContainer.h>

using namespace android;
#include <mtkcam/middleware/v1/IParamsManagerV3.h>
#include <mtkcam/middleware/v1/LegacyPipeline/LegacyPipelineUtils.h>
#include <mtkcam/middleware/v1/LegacyPipeline/LegacyPipelineBuilder.h>
#include <mtkcam/utils/hw/HwInfoHelper.h>
#include "../inc/FlowControlBase.h"
//
#include <mtkcam/utils/hw/CamManager.h>
using namespace NSCam::Utils;

/******************************************************************************
 *
 ******************************************************************************/
//
namespace NSCam {
namespace v1 {
namespace NSLegacyPipeline {


class DefaultFlowControl
    : public FlowControlBase
{
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  Interface.
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
public:
                                                DefaultFlowControl(
                                                    char const*                 pcszName,
                                                    MINT32 const                i4OpenId,
                                                    sp<IParamsManagerV3>          pParamsManagerV3,
                                                    sp<ImgBufProvidersManager>  pImgBufProvidersManager,
                                                    sp<INotifyCallback>         pCamMsgCbInfo
                                                );

    virtual                                     ~DefaultFlowControl() {};

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  IFlowControl Interface.
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    virtual char const*                         getName()   const;

    virtual int32_t                             getOpenId() const;

public:  //// Adapter

    virtual status_t                            startPreview();

    virtual status_t                            stopPreview();

    virtual status_t                            startRecording();

    virtual status_t                            stopRecording();

    virtual status_t                            autoFocus();

    virtual status_t                            cancelAutoFocus();

    virtual status_t                            precapture(int& flashRequired);

    virtual status_t                            takePicture();

    virtual status_t                            setParameters();

    virtual status_t                            sendCommand(
                                                    int32_t cmd,
                                                    int32_t arg1,
                                                    int32_t arg2
                                                );

public:

    virtual status_t                            dump(
                                                    int fd,
                                                    Vector<String8>const& args
                                                );

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  IRequestUpdater Interface.
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
public:

    virtual MERROR                              updateRequestSetting(
                                                    IMetadata* appSetting,
                                                    IMetadata* halSetting
                                                );

    virtual MERROR                              updateParameters(
                                                    IMetadata* setting
                                                );

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  IFeatureFlowControl Interface.
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
public:

    virtual MERROR                              submitRequest(
                                                    Vector< SettingSet >          vSettings,
                                                    BufferList                    vDstStreams,
                                                    Vector< MINT32 >&             vRequestNo
                                                );

    virtual MERROR                              submitRequest(
                                                    Vector< SettingSet >          vSettings,
                                                    Vector< BufferList >          vDstStreams,
                                                    Vector< MINT32 >&             vRequestNo
                                                );

    virtual MUINT                               getSensorMode(){ return mSensorParam.mode; }

    virtual MERROR                              pausePreview( MBOOL stopPipeline );


    virtual MERROR                              resumePreview();

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  RefBase Interface.
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
public:
    virtual MVOID                               onLastStrongRef( const void* /*id*/);

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  Implementations.
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
protected:  //// pipeline
    MERROR                                      constructNormalPreviewPipeline();

    MERROR                                      constructZsdPreviewPipeline();

    MERROR                                      constructRecordingPipeline();

    MERROR                                      prepareVSSInfo(
                                                    Vector< SettingSet >& vSettings,
                                                    BufferList&           vDstStreams
                                                );

    MERROR                                      prepareVideoInfo(
                                                    Vector< SettingSet >& vSettings,
                                                    BufferList&           vDstStreams
                                                );

    MBOOL                                       needReconstructRecordingPipe();

protected:
    MERROR                                      decideSensorModeAndRrzo(
                                                    MSize&  recordsize,
                                                    MUINT&  sensorMode,
                                                    MBOOL   bHighSpeedMode);

    MERROR                                      decideRrzoImage(
                                                    NSCamHW::HwInfoHelper& helper,
                                                    MUINT32 const bitDepth,
                                                    MSize referenceSize,
                                                    MUINT const usage,
                                                    MINT32 const  minBuffer,
                                                    MINT32 const  maxBuffer,
                                                    sp<IImageStreamInfo>& rpImageStreamInfo
                                                );

    MERROR                                      decideImgoImage(
                                                    NSCamHW::HwInfoHelper& helper,
                                                    MUINT32 const bitDepth,
                                                    MSize referenceSize,
                                                    MUINT const usage,
                                                    MINT32 const  minBuffer,
                                                    MINT32 const  maxBuffer,
                                                    sp<IImageStreamInfo>& rpImageStreamInfo
                                                );

    MERROR                                      setCamClient(
                                                    const char* name,
                                                    StreamId streamId,
                                                    Vector<PipelineImageParam>& vImageParam,
                                                    Vector<MUINT32> clientMode,
                                                    MUINT usage = 0,
                                                    MBOOL useTransform = MFALSE
                                                );

    MERROR                                      selectHighSpeedSensorScen(
                                                    MUINT   fps,
                                                    MUINT&  sensorScen
                                                );

    virtual MINT32                              getScenario() const;

    MVOID                                       runHighQualityZoomFlow();
    MVOID                                       setNewZsdPreviewRequestBuilder(MBOOL useImgoPreview);

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  Data Members.
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
protected:
    typedef struct
    {
        MUINT32 scenario;
        MINT32  fps;
    }SensorSlimVideoInfo;

    #define HighSpeedVideoFpsBase               (30)

    char*                                       mName;
    MINT32                                      mOpenId;
    MINT32                                      mLogLevel;
    MINT32                                      mpRequestThreadLoopCnt;
    LegacyPipelineBuilder::ConfigParams         mLPBConfigParams;
    sp< ILegacyPipeline >                       mpPipeline;
    sp<IRequestController>                      mpRequestController;
    sp<IParamsManagerV3>                        mpParamsManagerV3;
    sp<ImgBufProvidersManager>                  mpImgBufProvidersMgr;
    sp<INotifyCallback>                         mpCamMsgCbInfo;
    PipelineSensorParam                         mSensorParam;
    MBOOL                                       mb4K2KVideoRecord;
    sp<CamManager::UsingDeviceHelper>           mpDeviceHelper;
    MINT32                                      mRequestType;
    MUINT32                                     mLastZoomRatio;
    MBOOL                                       mbIsLastImgoPreview;
    MBOOL                                       mbNeedHighQualityZoom;
    MINT32                                      mEnLtm;

protected:
    enum {
        COMMAND_AUTO_FOCUS,
        COMMAND_CANCEL_AUTO_FOCUS
    };
    Vector<MINT32>                              mFocusCommandList;

protected:
    sp<IResourceContainer>                      mpResourceContainer;
};

/******************************************************************************
*
******************************************************************************/
};  //namespace NSPipelineContext
};  //namespace v1
};  //namespace NSCam
#endif  //_MTK_HARDWARE_MTKCAM_ADAPTER_DEFAULT_DEFAULTFLOWCONTROL_H_

