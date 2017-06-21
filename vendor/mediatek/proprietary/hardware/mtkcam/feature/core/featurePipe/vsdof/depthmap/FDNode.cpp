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

#include "FDNode.h"
#include "DepthMapPipeUtils.h"

#include <vector>
#include <algorithm>
#include <mtkcam/feature/stereo/hal/stereo_size_provider.h>
#include <mtkcam/utils/metadata/client/mtk_metadata_tag.h>

#define PIPE_CLASS_TAG "StereoFDNode"
#include <featurePipe/core/include/PipeLog.h>

// jpeg roataion
#define ANGLE_DEGREE_0 0
#define ANGLE_DEGREE_90 90
#define ANGLE_DEGREE_180 180
#define ANGLE_DEGREE_270 270

namespace NSCam {
namespace NSCamFeature {
namespace NSFeaturePipe {

const MUINT32 FD_WORKING_BUF_SIZE = 1024*1024*5;
const MUINT32 DDP_WORKING_BUF_SIZE = 640*640*2;
const MUINT32 EXTRACT_Y_WORKING_BUF_SIZE = 1920*1080;
const MUINT DETECT_FACE_NUM = 15;

StereoFDNode::StereoFDNode(const char* name, DepthFlowType flowType)
: DepthMapPipeNode(name, flowType)
{
    this->addWaitQueue(&mJobQueue);

    Pass2SizeInfo pass2SizeInfo;
    StereoSizeProvider* pSizePrvder = StereoSizeProvider::getInstance();
    pSizePrvder->getPass2SizeInfo(PASS2A, eSTEREO_SCENARIO_RECORD, pass2SizeInfo);
    P2AFM_FD_IMG_SIZE = pass2SizeInfo.areaIMG2O;
}

StereoFDNode::
~StereoFDNode()
{
}

MBOOL
StereoFDNode::
onInit()
{
    CAM_TRACE_NAME("StereoFDNode::onInit");
    // create FDHAL
    mpFDHalObj = halFDBase::createInstance(HAL_FD_OBJ_FDFT_SW);
    // allocate FD working buffer
    mpFDWorkingBuffer = new unsigned char[FD_WORKING_BUF_SIZE];
    mpDDPBuffer = new unsigned char[DDP_WORKING_BUF_SIZE];
    mpExtractYBuffer = new unsigned char[EXTRACT_Y_WORKING_BUF_SIZE];
    // FD HAL init
    MINT32  bErr;
    bErr = mpFDHalObj->halFDInit(P2AFM_FD_IMG_SIZE.w, P2AFM_FD_IMG_SIZE.h,
                            mpFDWorkingBuffer, FD_WORKING_BUF_SIZE, 1, 0);

    if(bErr)
    {
        MY_LOGE("FDHal Init Failed! status:%d", bErr);
        return MFALSE;
    }

    // create metadata
    mpDetectedFaces = new MtkCameraFaceMetadata;
    if ( NULL != mpDetectedFaces )
    {
        MtkCameraFace *faces = new MtkCameraFace[DETECT_FACE_NUM];
        MtkFaceInfo *posInfo = new MtkFaceInfo[DETECT_FACE_NUM];

        if ( NULL != faces &&  NULL != posInfo)
        {
            mpDetectedFaces->faces = faces;
            mpDetectedFaces->posInfo = posInfo;
            mpDetectedFaces->number_of_faces = 0;
        }
    }

    //
    mpJsonUtil = new JSON_Util();

    return MTRUE;

}

MBOOL
StereoFDNode::
onUninit()
{
    CAM_TRACE_NAME("StereoFDNode::onUninit");
    if(mpFDHalObj != NULL)
    {
        mpFDHalObj->halFDUninit();
        mpFDHalObj->destroyInstance();
        mpFDHalObj = NULL;
    }

    if(mpJsonUtil != NULL)
    {
        delete mpJsonUtil;
        mpJsonUtil = NULL;
    }

    if(mpFDWorkingBuffer != NULL)
    {
        delete mpFDWorkingBuffer;
        mpFDWorkingBuffer = NULL;
    }

    if(mpDDPBuffer != NULL)
    {
        delete mpDDPBuffer;
        mpDDPBuffer = NULL;
    }

    if(mpExtractYBuffer != NULL)
    {
        delete mpExtractYBuffer;
        mpExtractYBuffer = NULL;
    }

    if(mpDetectedFaces != NULL)
    {
        delete mpDetectedFaces->faces;
        delete mpDetectedFaces->posInfo;
        delete mpDetectedFaces;
        mpDetectedFaces = NULL;
    }

    return MTRUE;
}

MBOOL
StereoFDNode::
onThreadStart()
{
    return MTRUE;
}

MBOOL
StereoFDNode::
onThreadStop()
{
    return MTRUE;
}

MBOOL
StereoFDNode::
onData(DataID data, ExtraDataInfoPtr& N3DExtraData)
{
    MBOOL ret = MTRUE;
    MUINT32 iReqID =  N3DExtraData->getRequestPtr()->getRequestNo();

    VSDOF_LOGD("+ : reqId=%d dataID=%d (N3D)", iReqID, data);
    switch(data)
    {
        case N3D_TO_FD_EXTDATA_MASK:
            {
                android::Mutex::Autolock lock(mJsonMutex);
                mvN3DJsonMap.add(iReqID, N3DExtraData->msExtraData);
            }
            mergeExtraData(N3DExtraData->getRequestPtr());
            break;
        default:
            MY_LOGW("Unrecongnized DataID=%d", data);
            ret = MFALSE;
            break;
    }

    VSDOF_LOGD("-");
    return ret;
}

MBOOL
StereoFDNode::
onData(DataID data, ImgInfoMapPtr& P2FDImgInfo)
{
    MBOOL ret;
    VSDOF_LOGD("+ : reqId=%d dataID=%d (P2A)", P2FDImgInfo->getRequestPtr()->getRequestNo(), data);

    switch(data)
    {
        case P2AFM_TO_FD_IMG:
            mJobQueue.enque(P2FDImgInfo);
            ret = MTRUE;
            break;
        default:
            MY_LOGW("Unrecongnized DataID=%d", data);
            ret = MFALSE;
            break;
    }

    VSDOF_LOGD("-");
    return ret;
}

MBOOL
StereoFDNode::
onThreadLoop()
{
    ImgInfoMapPtr pP2FDImgInfo;
    EffectRequestPtr pEffReqPtr;

    if(!waitAllQueue())
    {
        return MFALSE;
    }

    if(!mJobQueue.deque(pP2FDImgInfo))
    {
        MY_LOGE("mJobQueue.deque() failed.");
        return MFALSE;
    }

    // mark on-going-request start
    this->incExtThreadDependency();
    pEffReqPtr = pP2FDImgInfo->getRequestPtr();

    VSDOF_PRFLOG("FD threapLoop start, reqId=%d", pEffReqPtr->getRequestNo());
    CAM_TRACE_NAME("StereoFDNode::onThreadLoop");

    // FD input src buffer
    SmartImageBuffer smFDBuf = pP2FDImgInfo->getImageBuffer(BID_P2AFM_OUT_FDIMG);
    MUINT8* pSrcBuf = (MUINT8*) smFDBuf->mImageBuffer->getBufVA(0);

    // app meta: get rotation
    MINT32 jpegOrientation;
    MBOOL bRet = tryGetMetadataInFrame<MINT32>(pEffReqPtr, FRAME_INPUT, BID_META_IN_APP, MTK_JPEG_ORIENTATION, jpegOrientation);
    if(!bRet) {
        MY_LOGE("Cannot find MTK_JPEG_ORIENTATION meta!");
        return MFALSE;
    }

    VSDOF_PRFLOG("halFTBufferCreate, reqId=%d", pEffReqPtr->getRequestNo());

    MINT32 bErr;
    // Performa FD ALGO
    CAM_TRACE_BEGIN("StereoFDNode::halFTBufferCreate_Extract");
    bErr = mpFDHalObj->halFTBufferCreate(mpDDPBuffer, pSrcBuf, smFDBuf->mImageBuffer->getPlaneCount(),
                                    P2AFM_FD_IMG_SIZE.w, P2AFM_FD_IMG_SIZE.h);
    bErr |= mpFDHalObj->halFDYUYV2ExtractY(mpExtractYBuffer, pSrcBuf,
                                    P2AFM_FD_IMG_SIZE.w, P2AFM_FD_IMG_SIZE.h);
    CAM_TRACE_END();

    //Do FD
    {
        VSDOF_PRFLOG("halFDDo start, reqId=%d", pEffReqPtr->getRequestNo());
        CAM_TRACE_BEGIN("StereoFDNode::halFDDo");

        struct FD_Frame_Parameters Param;
        Param.pScaleImages = NULL;
        Param.pRGB565Image = (MUINT8 *)mpDDPBuffer;
        Param.pPureYImage  = (MUINT8 *)mpExtractYBuffer;
        Param.pImageBufferVirtual = (MUINT8 *)smFDBuf->mImageBuffer->getBufVA(0);
        Param.pImageBufferPhyP0 = (MUINT8 *)smFDBuf->mImageBuffer->getBufPA(0);
        Param.pImageBufferPhyP1 = NULL;
        Param.pImageBufferPhyP2 = NULL;
        Param.Rotation_Info = jpegOrientation;
        Param.SDEnable = 0;
        Param.AEStable = 0;
        bErr |= mpFDHalObj->halFDDo(Param);

        CAM_TRACE_END();
    }

    if(bErr)
    {
        MY_LOGE("FD?operations failed!! status: %d", bErr);
        return MFALSE;
    }
    else
    {
        VSDOF_PRFLOG("halFDDo Done, reqId=%d", pEffReqPtr->getRequestNo());
    }

    // Get FD result
    CAM_TRACE_BEGIN("StereoFDNode::halFDGetFaceResult");
    int numFace = mpFDHalObj->halFDGetFaceResult(mpDetectedFaces);
    VSDOF_LOGD("halFDGetFaceResult jpegOrientation=%d numFace=%d", jpegOrientation,  numFace);
    CAM_TRACE_END();


    CAM_TRACE_BEGIN("StereoFDNode::calcRotatedFDResult");
    // pack fd result into the structure
    std::vector<FD_DATA_STEREO_T> vFDResult;
    for(int index=0;index<numFace;++index)
    {
        FD_DATA_STEREO_T fdData;
        calcRotatedFDResult_v2(mpDetectedFaces->faces[index], jpegOrientation, fdData);
        vFDResult.push_back(fdData);
    }
    CAM_TRACE_END();

    // gen FD json string
    char *sJsonStr = mpJsonUtil->facesToJSON(vFDResult);
    // add to map
    {
        android::Mutex::Autolock lock(mJsonMutex);
        mvFDJsonMap.add(pEffReqPtr->getRequestNo(), String8(sJsonStr));
    }

    // merge json if can
    mergeExtraData(pEffReqPtr);
    // mark on-going-request end
    this->decExtThreadDependency();

    VSDOF_PRFLOG("FD threapLoop end, reqId=%d", pEffReqPtr->getRequestNo());

    return MTRUE;

}

MVOID
StereoFDNode::
mergeExtraData(EffectRequestPtr pEffReqPtr)
{
    MUINT32 iReqID = pEffReqPtr->getRequestNo();
    int iFDIdx, iN3DIdx;

    VSDOF_LOGD("+");

    android::Mutex::Autolock lock(mJsonMutex);

    if((iFDIdx = mvFDJsonMap.indexOfKey(iReqID)) < 0 ||
        (iN3DIdx = mvN3DJsonMap.indexOfKey(iReqID)) < 0)
        {
            MY_LOGD("extra data not ready, reqID=%d", iReqID);
            return;
        }


    String8 sFdJson = mvFDJsonMap.valueAt(iFDIdx);
    String8 sN3DJson = mvN3DJsonMap.valueAt(iN3DIdx);

    FrameInfoPtr frameInfo_ExtraData;
    sp<IImageBuffer> pExtraDataImgBuf;

    RETRIEVE_OFRAMEINFO_IMGBUF_WITH_WARNING(pEffReqPtr, frameInfo_ExtraData, BID_FD_OUT_EXTRADATA, pExtraDataImgBuf);

    if(frameInfo_ExtraData.get())
    {
        // merge FD and N3D json
        char* sExtraData = mpJsonUtil->mergeJSON(sFdJson.string(), sN3DJson.string());
        // retrieve DOF level
        MINT32 DoFLevel;
        MBOOL bRet = tryGetMetadataInFrame<MINT32>(pEffReqPtr, FRAME_INPUT,
                                        BID_META_IN_APP,  MTK_STEREO_FEATURE_DOF_LEVEL, DoFLevel);
        if(bRet)
        {
            char* sDofJson = mpJsonUtil->dofLevelToJSON(DoFLevel);
            sExtraData = mpJsonUtil->mergeJSON(sDofJson, sExtraData);
        }
        else
            MY_LOGE("Cannot get DOF level in the metadata!");

        if(strlen(sExtraData)+1 > pExtraDataImgBuf->getBufSizeInBytes(0))
        {
            MY_LOGE("Extra data length is larger than the output buffer size, ExtraData size=%d , output buffer size=%d", strlen(sExtraData), pExtraDataImgBuf->getBufSizeInBytes(0));
        }
        else
        {
            VSDOF_LOGD("Copy result to frame!! result=%s", sExtraData);
            memcpy((MUINT8*) pExtraDataImgBuf->getBufVA(0), sExtraData, strlen(sExtraData)+1);
            frameInfo_ExtraData->setFrameReady(true);
            handleDataAndDump(FD_OUT_EXTRADATA, frameInfo_ExtraData, eSTATE_CAPTURE);
        }
    }

    mvFDJsonMap.removeItemsAt(iFDIdx);
    mvN3DJsonMap.removeItemsAt(iN3DIdx);
    VSDOF_LOGD("-");
}

MVOID
StereoFDNode::
calcRotatedFDResult(MtkCameraFace inputFaceInfo, MUINT32 iRotation, FD_DATA_STEREO_T &rRotatedFDResult)
{

    int seq[] = {0, 1, 2, 3};

    if(iRotation & eTransform_FLIP_H)
    {
        std::swap(seq[0], seq[1]);
        std::swap(seq[2], seq[3]);
    }

    if(iRotation & eTransform_FLIP_V)
    {
        std::swap(seq[0], seq[3]);
        std::swap(seq[1], seq[2]);
    }

    if(iRotation & eTransform_ROT_90)
    {
        //rotate the seq
        int idx=3, tmp;
        MUINT start = seq[idx];
        while(idx > 0)
        {
            tmp = seq[idx];
            idx--;
            seq[idx] = seq[idx];
        }
        seq[0] = start;
    }

    MINT32 ori_left = inputFaceInfo.rect[0];
    MINT32 ori_top = inputFaceInfo.rect[1];
    MINT32 ori_right = inputFaceInfo.rect[2];
    MINT32 ori_bottom = inputFaceInfo.rect[3];

    MPoint ori_loc[] = {MPoint(ori_left, ori_top), MPoint(ori_right, ori_top),
                        MPoint(ori_right, ori_bottom), MPoint(ori_left, ori_bottom)};
   int length[] = { ori_right - ori_left, ori_bottom - ori_top, ori_right - ori_left, ori_bottom - ori_top};
   int mapping_length[] = {P2AFM_FD_IMG_SIZE.w, P2AFM_FD_IMG_SIZE.h, P2AFM_FD_IMG_SIZE.w, P2AFM_FD_IMG_SIZE.h};


    int new_left_top_idx = seq[0];

    rRotatedFDResult.left =  ori_loc[new_left_top_idx].x+1000;
    rRotatedFDResult.top = ori_loc[new_left_top_idx].y+1000;

    printf("new_left_top_idx=%d\n", new_left_top_idx);

    // --- change source point ---
    bool is_clockwise = ( seq[0] + 1) % 4 == seq[1] ? true : false;

    // step 1: x, y flip
    if(!is_clockwise || new_left_top_idx%2==1)
    {
        std::swap(rRotatedFDResult.left, rRotatedFDResult.top);
    }

    // step 2: location normalize
    if(new_left_top_idx == 1)
    {
        rRotatedFDResult.top = 2000 - rRotatedFDResult.top;
    }
    else if(new_left_top_idx == 2)
    {
        rRotatedFDResult.left = 2000 - rRotatedFDResult.left;
        rRotatedFDResult.top = 2000 - rRotatedFDResult.top;
    }
    else if(new_left_top_idx == 3)
    {
        rRotatedFDResult.left = 2000 - rRotatedFDResult.left;
    }
    // flip back if non-clockeise and seq index is odd.
    if(!is_clockwise && new_left_top_idx%2==1)
    {
        std::swap(rRotatedFDResult.left, rRotatedFDResult.top);
    }

    int startPoint = is_clockwise ? 0 : 1;
    rRotatedFDResult.right = rRotatedFDResult.left + length[ (new_left_top_idx + startPoint)%4 ];
    rRotatedFDResult.bottom = rRotatedFDResult.top + length[ (new_left_top_idx + startPoint + 1)%4 ];


    // step3: change diamension to img domain
    rRotatedFDResult.left = rRotatedFDResult.left * ( mapping_length[(new_left_top_idx+startPoint)%4]/ 2000.0);
    rRotatedFDResult.top = rRotatedFDResult.top * ( mapping_length[(new_left_top_idx+startPoint + 1)%4]/ 2000.0);
    rRotatedFDResult.right = rRotatedFDResult.right * ( mapping_length[(new_left_top_idx+startPoint + 2)%4]/ 2000.0);
    rRotatedFDResult.bottom = rRotatedFDResult.bottom * ( mapping_length[(new_left_top_idx+startPoint + 3)%4]/ 2000.0);


    MY_LOGD("ori_left=%d, ori_top=%d, ori_right=%d, ori_bottom=%d, rotation=%d\n"
                ", new_left=%d, new_top=%d, new_right=%d, new_bottom=%d\n", ori_left, ori_top,
                ori_right, ori_bottom, iRotation, rRotatedFDResult.left, rRotatedFDResult.top,
                rRotatedFDResult.right, rRotatedFDResult.bottom);

}


MVOID
StereoFDNode::
calcRotatedFDResult_v2(MtkCameraFace inputFaceInfo, MUINT32 iRotation, FD_DATA_STEREO_T &rRotatedFDResult)
{
    int srcWidth = P2AFM_FD_IMG_SIZE.w;
    int srcHeight = P2AFM_FD_IMG_SIZE.h;
    switch(iRotation){
        case ANGLE_DEGREE_0:{
            // rotation = 0 ; mirror = 0
            rRotatedFDResult.left = (MUINT32)(inputFaceInfo.rect[0]);
            rRotatedFDResult.top = (MUINT32)(inputFaceInfo.rect[1]);
            rRotatedFDResult.right = (MUINT32)(inputFaceInfo.rect[2]);
            rRotatedFDResult.bottom = (MUINT32)(inputFaceInfo.rect[3]);
            break;
        }
        case ANGLE_DEGREE_90:{
            // rotation = 90 ; mirror = 0
            rRotatedFDResult.left = (MUINT32)(0 - inputFaceInfo.rect[3]);
            rRotatedFDResult.top = (MUINT32)(inputFaceInfo.rect[0]);
            rRotatedFDResult.right = (MUINT32)(0 - inputFaceInfo.rect[1]);
            rRotatedFDResult.bottom = (MUINT32)(inputFaceInfo.rect[2]);
            break;
        }
        case ANGLE_DEGREE_180:{
            // rotation = 180 ; mirror = 0
            rRotatedFDResult.left = (MUINT32)(0 - inputFaceInfo.rect[2]);
            rRotatedFDResult.top = (MUINT32)(0 - inputFaceInfo.rect[3]);
            rRotatedFDResult.right = (MUINT32)(0 - inputFaceInfo.rect[0]);
            rRotatedFDResult.bottom = (MUINT32)(0 - inputFaceInfo.rect[1]);
            break;
        }
        case ANGLE_DEGREE_270:{
            // rotation = 270 ; mirror = 0
            rRotatedFDResult.left = (MUINT32)(inputFaceInfo.rect[1]);
            rRotatedFDResult.top = (MUINT32)(0 - inputFaceInfo.rect[2]);
            rRotatedFDResult.right = (MUINT32)(inputFaceInfo.rect[3]);
            rRotatedFDResult.bottom = (MUINT32)(0 - inputFaceInfo.rect[0]);
            break;
        }
        default:{
            MY_LOGE("mTransform=%d, no such case, should not happend!", iRotation);
            break;
        }
    }
}

}; //NSFeaturePipe
}; //NSCamFeature
}; //NSCam
