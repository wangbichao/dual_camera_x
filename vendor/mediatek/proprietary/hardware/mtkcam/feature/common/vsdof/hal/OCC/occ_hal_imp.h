/********************************************************************************************
 *     LEGAL DISCLAIMER
 *
 *     (Header of MediaTek Software/Firmware Release or Documentation)
 *
 *     BY OPENING OR USING THIS FILE, BUYER HEREBY UNEQUIVOCALLY ACKNOWLEDGES AND AGREES
 *     THAT THE SOFTWARE/FIRMWARE AND ITS DOCUMENTATIONS ("MEDIATEK SOFTWARE") RECEIVED
 *     FROM MEDIATEK AND/OR ITS REPRESENTATIVES ARE PROVIDED TO BUYER ON AN "AS-IS" BASIS
 *     ONLY. MEDIATEK EXPRESSLY DISCLAIMS ANY AND ALL WARRANTIES, EXPRESS OR IMPLIED,
 *     INCLUDING BUT NOT LIMITED TO THE IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR
 *     A PARTICULAR PURPOSE OR NONINFRINGEMENT. NEITHER DOES MEDIATEK PROVIDE ANY WARRANTY
 *     WHATSOEVER WITH RESPECT TO THE SOFTWARE OF ANY THIRD PARTY WHICH MAY BE USED BY,
 *     INCORPORATED IN, OR SUPPLIED WITH THE MEDIATEK SOFTWARE, AND BUYER AGREES TO LOOK
 *     ONLY TO SUCH THIRD PARTY FOR ANY WARRANTY CLAIM RELATING THERETO. MEDIATEK SHALL ALSO
 *     NOT BE RESPONSIBLE FOR ANY MEDIATEK SOFTWARE RELEASES MADE TO BUYER'S SPECIFICATION
 *     OR TO CONFORM TO A PARTICULAR STANDARD OR OPEN FORUM.
 *
 *     BUYER'S SOLE AND EXCLUSIVE REMEDY AND MEDIATEK'S ENTIRE AND CUMULATIVE LIABILITY WITH
 *     RESPECT TO THE MEDIATEK SOFTWARE RELEASED HEREUNDER WILL BE, AT MEDIATEK'S OPTION,
 *     TO REVISE OR REPLACE THE MEDIATEK SOFTWARE AT ISSUE, OR REFUND ANY SOFTWARE LICENSE
 *     FEES OR SERVICE CHARGE PAID BY BUYER TO MEDIATEK FOR SUCH MEDIATEK SOFTWARE AT ISSUE.
 *
 *     THE TRANSACTION CONTEMPLATED HEREUNDER SHALL BE CONSTRUED IN ACCORDANCE WITH THE LAWS
 *     OF THE STATE OF CALIFORNIA, USA, EXCLUDING ITS CONFLICT OF LAWS PRINCIPLES.
 ************************************************************************************************/
#ifndef _OCC_HAL_IMP_H_
#define _OCC_HAL_IMP_H_

#include <vsdof/hal/occ_hal.h>
#include <libocc/MTKOcc.h>

class OCC_HAL_IMP : public OCC_HAL
{
public:
    OCC_HAL_IMP();
    virtual ~OCC_HAL_IMP();

    virtual bool OCCHALRun(OCC_HAL_PARAMS &occHalParam, OCC_HAL_OUTPUT &occHalOutput);
protected:

private:
    void _setOCCParams(OCC_HAL_PARAMS &occHalParam);
    void _runOCC(OCC_HAL_OUTPUT &occHalOutput);
    //
    void _dumpInitData();
    void _dumpSetProcData();
    void _dumpOCCResult();
    void _dumpOCCBufferInfo(OccBufferInfo *buf, int index);
    //
    char *_getDumpFolderName(int folderNumber, char path[]);
    void _mkdir();  //make output dir for debug
    void _splitDisparityMapAndDump(const short *dispL, const short *dispR);
private:
    MTKOcc          *m_pOccDrv;

    OccInitInfo     m_initInfo;
    OccProcInfo     m_procInfo;
    OccResultInfo   m_resultInfo;
    //
    const bool      DUMP_BUFFER;
    const MINT32    DUMP_START;
    const MINT32    DUMP_END;
    const bool      LOG_ENABLED;
    MINT32          m_requestNumber;
    bool            m_isDump;
};

#endif