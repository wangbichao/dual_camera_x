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

#ifndef _MTK_CAMERA_INCLUDE_STEREO_DEPTHMAP_FEATUREPIPE_COMMON_H_
#define _MTK_CAMERA_INCLUDE_STEREO_DEPTHMAP_FEATUREPIPE_COMMON_H_

// Standard C header file
#include <ctime>
#include <chrono>
#include <bitset>
// Android system/core header file
#include <utils/RefBase.h>
#include <utils/KeyedVector.h>

// mtkcam custom header file

// mtkcam global header file
#include <mtkcam/def/common.h>
#include <mtkcam/feature/stereo/pipe/vsdof_common.h>
#include <mtkcam/feature/stereo/pipe/IBMDepthMapPipe.h>

// Module header file
#include <mtkcam/drv/iopipe/Port.h>
#include <mtkcam/utils/metadata/IMetadata.h>
#include <mtkcam/feature/stereo/hal/stereo_size_provider.h>
#include <featurePipe/core/include/ImageBufferPool.h>
#include <featurePipe/core/include/GraphicBufferPool.h>
#include <mtkcam/drv/def/Dip_Notify_datatype.h>
// Local header file

// Logging log header
#define PIPE_MODULE_TAG "DepthMapPipe"
#define PIPE_CLASS_TAG "Common"
#include <featurePipe/core/include/PipeLog.h>

/*******************************************************************************
* Namespace start.
********************************************************************************/
namespace NSCam {
namespace NSCamFeature {
namespace NSFeaturePipe_DepthMap_BM {

using android::String8;
using android::KeyedVector;
using namespace android;


/*******************************************************************************
* Const Definition
********************************************************************************/
const int VSDOF_CONST_FE_EXEC_TIMES = 2;
const int VSDOF_BURST_FRAME_SIZE = 10;
const int VSDOF_WORKING_BUF_SET = 3;

/*******************************************************************************
* Macro Definition
********************************************************************************/
#define MY_LOGV_IF(cond, ...)       do { if ( (cond) ) { MY_LOGV(__VA_ARGS__); } }while(0)
#define MY_LOGD_IF(cond, ...)       do { if ( (cond) ) { MY_LOGD(__VA_ARGS__); } }while(0)
#define MY_LOGI_IF(cond, ...)       do { if ( (cond) ) { MY_LOGI(__VA_ARGS__); } }while(0)
#define MY_LOGW_IF(cond, ...)       do { if ( (cond) ) { MY_LOGW(__VA_ARGS__); } }while(0)
#define MY_LOGE_IF(cond, ...)       do { if ( (cond) ) { MY_LOGE(__VA_ARGS__); } }while(0)
#define MY_LOGA_IF(cond, ...)       do { if ( (cond) ) { MY_LOGA(__VA_ARGS__); } }while(0)
#define MY_LOGF_IF(cond, ...)       do { if ( (cond) ) { MY_LOGF(__VA_ARGS__); } }while(0)

/******************************************************************************
* Enum Definition
********************************************************************************/
/**
  * @brief Node ID inside the DepthMapPipe
 */
typedef enum eDepthMapPipeNodeID {
    eDPETHMAP_PIPE_NODEID_P2AFM,
    eDPETHMAP_PIPE_NODEID_N3D,
    eDPETHMAP_PIPE_NODEID_DPE,
    eDPETHMAP_PIPE_NODEID_WMF,
    eDPETHMAP_PIPE_NODEID_OCC,
    eDPETHMAP_PIPE_NODEID_FD,
    eDPETHMAP_PIPE_NODEID_GF,
    // any node need to put upon this line
    eDPETHMAP_PIPE_NODE_SIZE,
} DepthMapPipeNodeID;

typedef std::bitset<eDPETHMAP_PIPE_NODE_SIZE> PipeNodeBitSet;

/**
  * @brief Data ID used in handleData inside the DepthMapPipe
 */
enum DepthMapDataID {
    ID_INVALID,
    ROOT_ENQUE,

    P2AFM_TO_N3D_FEFM_CCin,
    P2AFM_TO_WMF_MY_SL,
    P2AFM_TO_FD_IMG,
    N3D_TO_DPE_MVSV_MASK,
    N3D_TO_OCC_LDC,
    N3D_TO_FD_EXTDATA_MASK,
    DPE_TO_OCC_MVSV_DMP_CFM,
    OCC_TO_WMF_DMH_MY_S,
    // DepthMap output
    P2AFM_OUT_MV_F,   //10
    P2AFM_OUT_FD,
    P2AFM_OUT_MV_F_CAP,
    WMF_OUT_DMW_MY_S,
    WMF_OUT_DEPTHMAP,
    FD_OUT_EXTRADATA,
    N3D_OUT_JPS_WARPING_MATRIX,
    DPE_OUT_DISPARITY,
    DEPTHMAP_META_OUT,

    // Notify error occur underlying the nodes
    ERROR_OCCUR_NOTIFY,
    // use to dump p1 raw buffers
    TO_DUMP_RAWS,
    // use to dump buffers
    TO_DUMP_BUFFERS, //20
    #ifdef GTEST
    // For UT
    UT_OUT_FE
    #endif
};

/**
  * @brief Data ID used in handleData inside the DepthMapPipe
 */
typedef enum eStereoP2Path
{
    eP2APATH_MAIN1,
    eP2APATH_MAIN2

} StereoP2Path;

/*******************************************************************************
* Structure Definition
********************************************************************************/

/*******************************************************************************
* Class Definition
********************************************************************************/
/**
 * @class DepthPipeLoggingSetup
 * @brief Control the logging enable of the depthmap pipe
 */

class DepthPipeLoggingSetup
{
public:
    static MBOOL mbProfileLog;
    static MBOOL mbDebugLog;
};

#undef VSDOF_LOGD
#undef VSDOF_PRFLOG
// logging macro
#define VSDOF_LOGD(fmt, arg...) \
    do { if(DepthPipeLoggingSetup::mbDebugLog) {MY_LOGD("%d: " fmt, __LINE__, ##arg);} } while(0)

#define VSDOF_PRFLOG(fmt, arg...) \
    do { if(DepthPipeLoggingSetup::mbProfileLog) {MY_LOGD("[VSDOF_Profile] %d: " fmt, __LINE__, ##arg);} } while(0)

struct SimpleTimer
{
public:
    SimpleTimer();
    SimpleTimer(bool bStart);
    /* start timer */
    MBOOL startTimer();
    /* cpunt the timer, return elaspsed time from start timer. */
    float countTimer();
public:
    std::chrono::time_point<std::chrono::system_clock> start;
};

/******************************************************************************
* Type Definition
********************************************************************************/
class DepthMapPipeNode;
typedef KeyedVector<DepthMapPipeNodeID, DepthMapPipeNode*> DepthMapPipeNodeMap;

}; // NSFeaturePipe_DepthMap_BM
}; // NSCamFeature
}; // NSCam

#endif // _MTK_CAMERA_INCLUDE_STEREO_DEPTHMAP_FEATUREPIPE_COMMON_H_
