# Copyright Statement:
#
# This software/firmware and related documentation ("MediaTek Software") are
# protected under relevant copyright laws. The information contained herein
# is confidential and proprietary to MediaTek Inc. and/or its licensors.
# Without the prior written permission of MediaTek inc. and/or its licensors,
# any reproduction, modification, use or disclosure of MediaTek Software,
# and information contained herein, in whole or in part, shall be strictly prohibited.

# MediaTek Inc. (C) 2010. All rights reserved.
#
# BY OPENING THIS FILE, RECEIVER HEREBY UNEQUIVOCALLY ACKNOWLEDGES AND AGREES
# THAT THE SOFTWARE/FIRMWARE AND ITS DOCUMENTATIONS ("MEDIATEK SOFTWARE")
# RECEIVED FROM MEDIATEK AND/OR ITS REPRESENTATIVES ARE PROVIDED TO RECEIVER ON
# AN "AS-IS" BASIS ONLY. MEDIATEK EXPRESSLY DISCLAIMS ANY AND ALL WARRANTIES,
# EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED WARRANTIES OF
# MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE OR NONINFRINGEMENT.
# NEITHER DOES MEDIATEK PROVIDE ANY WARRANTY WHATSOEVER WITH RESPECT TO THE
# SOFTWARE OF ANY THIRD PARTY WHICH MAY BE USED BY, INCORPORATED IN, OR
# SUPPLIED WITH THE MEDIATEK SOFTWARE, AND RECEIVER AGREES TO LOOK ONLY TO SUCH
# THIRD PARTY FOR ANY WARRANTY CLAIM RELATING THERETO. RECEIVER EXPRESSLY ACKNOWLEDGES
# THAT IT IS RECEIVER'S SOLE RESPONSIBILITY TO OBTAIN FROM ANY THIRD PARTY ALL PROPER LICENSES
# CONTAINED IN MEDIATEK SOFTWARE. MEDIATEK SHALL ALSO NOT BE RESPONSIBLE FOR ANY MEDIATEK
# SOFTWARE RELEASES MADE TO RECEIVER'S SPECIFICATION OR TO CONFORM TO A PARTICULAR
# STANDARD OR OPEN FORUM. RECEIVER'S SOLE AND EXCLUSIVE REMEDY AND MEDIATEK'S ENTIRE AND
# CUMULATIVE LIABILITY WITH RESPECT TO THE MEDIATEK SOFTWARE RELEASED HEREUNDER WILL BE,
# AT MEDIATEK'S OPTION, TO REVISE OR REPLACE THE MEDIATEK SOFTWARE AT ISSUE,
# OR REFUND ANY SOFTWARE LICENSE FEES OR SERVICE CHARGE PAID BY RECEIVER TO
# MEDIATEK FOR SUCH MEDIATEK SOFTWARE AT ISSUE.
#
# The following software/firmware and/or related documentation ("MediaTek Software")
# have been modified by MediaTek Inc. All revisions are subject to any receiver's
# applicable license agreements with MediaTek Inc.

-include $(TOP)/$(MTK_PATH_SOURCE)/hardware/mtkcam/mtkcam.mk

################################################################################
#
################################################################################

MY_CLIENT_C_INCLUDES_PATH := $(TOP)/$(MTK_PATH_SOURCE)/hardware/mtkcam/legacy/v1/client
MY_CLIENT_C_INCLUDES :=
MY_CLIENT_CFLAGS :=

#-----------------------------------------------------------
# MTK Hal Standard Include Path
MY_CLIENT_C_INCLUDES += $(TOP)/$(MTK_PATH_SOURCE)/hardware/include
MY_CLIENT_C_INCLUDES += $(TOP)/$(MTK_PATH_SOURCE)/hardware/mtkacm/include
#
#-----------------------------------------------------------
LOCAL_C_INCLUDES += $(TOP)/$(MTKCAM_C_INCLUDES)
# MTK Framework Standard Include Path
MY_CLIENT_C_INCLUDES += $(TOP)/$(MTK_PATH_SOURCE)/hardware/mtkcam/ext/include
MY_CLIENT_C_INCLUDES += $(TOP)/$(MTK_PATH_SOURCE)/frameworks/av
#
#
################################################################################
# Client Modules
################################################################################
#
# Preview Feature Clients
ifneq ($(MTK_BASIC_PACKAGE), yes)
MTKCAM_HAVE_PREVIEWFEATURE_CLIENT := '1'
else
MTKCAM_HAVE_PREVIEWFEATURE_CLIENT := '0'
endif
MY_CLIENT_CFLAGS += -DMTKCAM_HAVE_PREVIEWFEATURE_CLIENT="$(MTKCAM_HAVE_PREVIEWFEATURE_CLIENT)"
#
# Preview Callback Client
MTKCAM_HAVE_PREVIEWCALLBACK_CLIENT := '1'
MY_CLIENT_CFLAGS += -DMTKCAM_HAVE_PREVIEWCALLBACK_CLIENT="$(MTKCAM_HAVE_PREVIEWCALLBACK_CLIENT)"
#
# Display Client
MTKCAM_HAVE_DISPLAY_CLIENT := '1'
MY_CLIENT_CFLAGS += -DMTKCAM_HAVE_DISPLAY_CLIENT="$(MTKCAM_HAVE_DISPLAY_CLIENT)"
#
# Record Client
MTKCAM_HAVE_RECORD_CLIENT := '1'
MY_CLIENT_CFLAGS += -DMTKCAM_HAVE_RECORD_CLIENT="$(MTKCAM_HAVE_RECORD_CLIENT)"
#
# FD Client
MTKCAM_HAVE_FD_CLIENT := '1'
MY_CLIENT_CFLAGS += -DMTKCAM_HAVE_FD_CLIENT="$(MTKCAM_HAVE_FD_CLIENT)"
#
# OT CLient
ifeq ($(MTK_CAM_OT_SUPPORT),yes)
MTKCAM_HAVE_OT_CLIENT := '1'
else
MTKCAM_HAVE_OT_CLIENT := '0'
endif
MY_CLIENT_CFLAGS += -DMTKCAM_HAVE_OT_CLIENT="$(MTKCAM_HAVE_OT_CLIENT)"
#
