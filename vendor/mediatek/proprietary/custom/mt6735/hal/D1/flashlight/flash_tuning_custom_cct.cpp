#define LOG_TAG "flash_tuning_custom_cct.cpp"
#include "string.h"
#include "camera_custom_nvram.h"
#include "camera_custom_types.h"
#include "camera_custom_AEPlinetable.h"
#include <cutils/log.h>
#include "flash_feature.h"
#include "flash_param.h"
#include "flash_tuning_custom.h"
#include <kd_camera_feature.h>

//==============================================================================
//
//==============================================================================
int cust_fillDefaultStrobeNVRam_main (void* data)
{
    int i;
	NVRAM_CAMERA_STROBE_STRUCT* p;
	p = (NVRAM_CAMERA_STROBE_STRUCT*)data;

#if defined(LM3644) //sanford.lin
	static short engTab[]={
          -1, 661,1287,1868,2204,2558,2819,3073,3325,3609,3854,4195,4558,4990,5353,5753,5988,6338,6714,7031,7442,7719,8166,8615,8782,9242,9638,
         534,1184,1673,2121,2370,2635,2825,3011,3193,3382,3554,3800,4042,4349,4593,4868,5004,5220,5485,5665,5923,6156,6377,6668,6870,7087,  -1,
        1017,1808,2299,2747,2994,3417,3612,3791,3972,4164,4333,4577,4821,5130,5348,5643,5780,5990,6250,6436,6691,6919,7142,7466,7633,  -1,  -1,
        1461,2391,2876,3319,3571,3980,4168,4355,4535,4727,4897,5146,5384,5705,5906,6203,6338,6559,6810,7030,7261,7471,7698,7969,  -1,  -1,  -1,
        1712,2717,3205,3649,3899,4249,4440,4626,4804,4994,5165,5431,5646,5954,6178,6467,6603,6814,7049,7261,7495,7730,8005,  -1,  -1,  -1,  -1,
        1984,3075,3682,4112,4320,4527,4714,4900,5081,5269,5439,5675,5923,6233,6448,6746,6879,7124,7342,7522,7763,8016,8227,  -1,  -1,  -1,  -1,
        2171,3332,3942,4377,4573,4786,4975,5159,5338,5525,5696,5942,6182,6485,6702,6996,7131,7340,7598,7778,8019,8266,  -1,  -1,  -1,  -1,  -1,
        2364,3591,4198,4627,4828,5039,5242,5416,5607,5780,5952,6195,6444,6769,6962,7248,7385,7595,7837,8062,8277,8508,  -1,  -1,  -1,  -1,  -1,
        2538,3840,4449,4877,5076,5300,5473,5661,5840,6031,6196,6439,6666,6986,7199,7494,7623,7879,8079,8267,8518,  -1,  -1,  -1,  -1,  -1,  -1,
        2743,4109,4713,5157,5345,5551,5741,5927,6105,6297,6462,6734,6945,7249,7463,7762,7889,8099,8345,8520,8774,  -1,  -1,  -1,  -1,  -1,  -1,
        2911,4350,4961,5383,5585,5812,5982,6164,6343,6532,6703,6944,7212,7503,7700,7981,8169,8326,8595,8756,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
        3168,4722,5307,5736,5934,6141,6331,6520,6690,6910,7054,7293,7534,7832,8049,8385,8466,8707,8925,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
        3412,5040,5646,6086,6271,6478,6666,6854,7031,7216,7380,7667,7867,8159,8423,8657,8799,8999,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
        3729,5490,6121,6514,6714,6921,7121,7292,7487,7657,7822,8068,8297,8595,8811,9164,9294,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
        3925,5813,6416,6839,7034,7289,7429,7612,7834,7980,8137,8381,8614,8907,9115,9398,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
        4223,6257,6855,7269,7470,7682,7865,8045,8284,8469,8566,8780,9036,9332,9627,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
        4404,6457,7056,7477,7672,7880,8067,8245,8489,8606,8765,9015,9252,9620,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
        4617,6761,7368,7849,7973,8248,8431,8557,8724,8920,9062,9305,9533,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
        4878,7156,7750,8169,8355,8605,8749,8926,9096,9285,9459,9687,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
        5059,7525,8028,8440,8650,8848,9029,9202,9388,9563,9772,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
        5347,7820,8420,8935,9132,9217,9405,9579,9748,9982,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
        5524,8180,8802,9269,9366,9601,9738,9935,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
        5804,8517,9085,9639,9686,9889,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
        6038,8923,9507,9999,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
        6254,9244,9823,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
        6489,9632,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
        6778,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
	};
#elif defined(SKY81294)
static short engTab[]=
	{2165,2950,3573,4498,5638,6067,6025,6382,6918,7634,8848,8530,8545,8855,9226,9999};
#elif defined(LM3642TLX) || defined(SY7804)
static short engTab[]=
	{1339,1940,2505,3586,4562,5486,6348,7168,8043,9156,9999};
#elif defined(SGM37891)
static short engTab[]=
	{1339,2505,4562,5486,7168,8665,9999};
#else //MTK MOS
static short engTab[]=
	{4738,9999};
#endif

	//version
	p->u4Version = NVRAM_CAMERA_STROBE_FILE_VERSION;
	//eng tab
	memcpy(p->engTab.yTab, engTab, sizeof(engTab));

	//tuningPara[8];
	for(i=0;i<8;i++)
    {
        p->tuningPara[i].yTarget = 188;
        p->tuningPara[i].fgWIncreaseLevelbySize = 10;
        p->tuningPara[i].fgWIncreaseLevelbyRef = 0;
        p->tuningPara[i].ambientRefAccuracyRatio = 5;
        p->tuningPara[i].flashRefAccuracyRatio = 1;
        p->tuningPara[i].backlightAccuracyRatio = 18;
        p->tuningPara[i].backlightUnderY = 40;
        p->tuningPara[i].backlightWeakRefRatio = 32;
        p->tuningPara[i].safetyExp =33322;
        p->tuningPara[i].maxUsableISO = 680;
        p->tuningPara[i].yTargetWeight = 0;
        p->tuningPara[i].lowReflectanceThreshold = 13;
        p->tuningPara[i].flashReflectanceWeight = 0;
        p->tuningPara[i].bgSuppressMaxDecreaseEV = 20;
        p->tuningPara[i].bgSuppressMaxOverExpRatio = 6;
        p->tuningPara[i].fgEnhanceMaxIncreaseEV = 50;
        p->tuningPara[i].fgEnhanceMaxOverExpRatio = 6;
        p->tuningPara[i].isFollowCapPline = 1;
        p->tuningPara[i].histStretchMaxFgYTarget = 300;//285;//266;
        p->tuningPara[i].histStretchBrightestYTarget = 480;//404;//328;
        p->tuningPara[i].fgSizeShiftRatio = 0;
        p->tuningPara[i].backlitPreflashTriggerLV = 90;
        p->tuningPara[i].backlitMinYTarget = 100;
    }

    p->tuningPara[0].isFollowCapPline = 0;

    p->paraIdxForceOn[0] =1;    //default
    p->paraIdxForceOn[1] =0;    //LIB3A_AE_SCENE_OFF
    p->paraIdxForceOn[2] =0;    //LIB3A_AE_SCENE_AUTO
    p->paraIdxForceOn[3] =1;    //LIB3A_AE_SCENE_NIGHT
    p->paraIdxForceOn[4] =1;    //LIB3A_AE_SCENE_ACTION
    p->paraIdxForceOn[5] =1;    //LIB3A_AE_SCENE_BEACH
    p->paraIdxForceOn[6] =1;    //LIB3A_AE_SCENE_CANDLELIGHT
    p->paraIdxForceOn[7] =1;    //LIB3A_AE_SCENE_FIREWORKS
    p->paraIdxForceOn[8] =1;    //LIB3A_AE_SCENE_LANDSCAPE
    p->paraIdxForceOn[9] =1;    //LIB3A_AE_SCENE_PORTRAIT
    p->paraIdxForceOn[10] =1;   //LIB3A_AE_SCENE_NIGHT_PORTRAIT
    p->paraIdxForceOn[11] =1;   //LIB3A_AE_SCENE_PARTY
    p->paraIdxForceOn[12] =1;   //LIB3A_AE_SCENE_SNOW
    p->paraIdxForceOn[13] =1;   //LIB3A_AE_SCENE_SPORTS
    p->paraIdxForceOn[14] =1;   //LIB3A_AE_SCENE_STEADYPHOTO
    p->paraIdxForceOn[15] =1;   //LIB3A_AE_SCENE_SUNSET
    p->paraIdxForceOn[16] =1;   //LIB3A_AE_SCENE_THEATRE
    p->paraIdxForceOn[17] =1;   //LIB3A_AE_SCENE_ISO_ANTI_SHAKE
    p->paraIdxForceOn[18] =1;   //LIB3A_AE_SCENE_BACKLIGHT

    p->paraIdxAuto[0] =1;  //default
    p->paraIdxAuto[1] =0;  //LIB3A_AE_SCENE_OFF
    p->paraIdxAuto[2] =0;  //LIB3A_AE_SCENE_AUTO
    p->paraIdxAuto[3] =1;  //LIB3A_AE_SCENE_NIGHT
    p->paraIdxAuto[4] =1;  //LIB3A_AE_SCENE_ACTION
    p->paraIdxAuto[5] =1;  //LIB3A_AE_SCENE_BEACH
    p->paraIdxAuto[6] =1;  //LIB3A_AE_SCENE_CANDLELIGHT
    p->paraIdxAuto[7] =1;  //LIB3A_AE_SCENE_FIREWORKS
    p->paraIdxAuto[8] =1;  //LIB3A_AE_SCENE_LANDSCAPE
    p->paraIdxAuto[9] =1;  //LIB3A_AE_SCENE_PORTRAIT
    p->paraIdxAuto[10] =1; //LIB3A_AE_SCENE_NIGHT_PORTRAIT
    p->paraIdxAuto[11] =1; //LIB3A_AE_SCENE_PARTY
    p->paraIdxAuto[12] =1; //LIB3A_AE_SCENE_SNOW
    p->paraIdxAuto[13] =1; //LIB3A_AE_SCENE_SPORTS
    p->paraIdxAuto[14] =1; //LIB3A_AE_SCENE_STEADYPHOTO
    p->paraIdxAuto[15] =1; //LIB3A_AE_SCENE_SUNSET
    p->paraIdxAuto[16] =1; //LIB3A_AE_SCENE_THEATRE
    p->paraIdxAuto[17] =1; //LIB3A_AE_SCENE_ISO_ANTI_SHAKE
    p->paraIdxAuto[18] =1; //LIB3A_AE_SCENE_BACKLIGHT

	//--------------------
	//eng level
	//index mode
#if defined(LM3644) //sanford.lin
	//torch
    p->engLevel.torchDuty = 3;
	//af
    p->engLevel.torchDutyEx[0] = 0;
	//pf, mf, normal
    p->engLevel.torchDutyEx[1] = 0;
    p->engLevel.torchDutyEx[2] = 0;
    p->engLevel.torchDutyEx[3] = 0;
	//low bat
    p->engLevel.torchDutyEx[4] = 0;
    p->engLevel.torchDutyEx[5] = 0;
    p->engLevel.torchDutyEx[6] = 0;
    p->engLevel.torchDutyEx[7] = 0;
    p->engLevel.torchDutyEx[8] = 0;
    p->engLevel.torchDutyEx[9] = 0;
    p->engLevel.torchDutyEx[10] = 0;
    p->engLevel.torchDutyEx[11] = 0;
    p->engLevel.torchDutyEx[12] = 0;
    p->engLevel.torchDutyEx[13] = 0;
    p->engLevel.torchDutyEx[14] = 0;
    p->engLevel.torchDutyEx[15] = 0;
    p->engLevel.torchDutyEx[16] = 0;
    p->engLevel.torchDutyEx[17] = 0;
    p->engLevel.torchDutyEx[18] = 0;
	//torch
    p->engLevel.torchDutyEx[19] = 0;
	//af
    p->engLevel.afDuty = 3;
	//pf, mf, normal
    p->engLevel.pfDuty = 3;
    p->engLevel.mfDutyMax = 25;
    p->engLevel.mfDutyMin = -1;
	//low bat
	p->engLevel.IChangeByVBatEn=0;
    p->engLevel.vBatL = 3400;
    p->engLevel.pfDutyL = 3;
	p->engLevel.mfDutyMaxL = 3;
    p->engLevel.mfDutyMinL = -1;
	//burst setting
    p->engLevel.IChangeByBurstEn = 0;
    p->engLevel.pfDutyB = 3;
    p->engLevel.mfDutyMaxB = 3;
    p->engLevel.mfDutyMinB = -1;
	//high current setting
    p->engLevel.decSysIAtHighEn = 0;
    p->engLevel.dutyH = 25;

	//torch
    p->engLevelLT.torchDuty = -1;
	//af
    p->engLevelLT.torchDutyEx[0] = 0;
	//pf, mf, normal
    p->engLevelLT.torchDutyEx[1] = 0;
    p->engLevelLT.torchDutyEx[2] = 0;
    p->engLevelLT.torchDutyEx[3] = 0;
	//low bat
    p->engLevelLT.torchDutyEx[4] = 0;
    p->engLevelLT.torchDutyEx[5] = 0;
    p->engLevelLT.torchDutyEx[6] = 0;
    p->engLevelLT.torchDutyEx[7] = 0;
    p->engLevelLT.torchDutyEx[8] = 0;
    p->engLevelLT.torchDutyEx[9] = 0;
    p->engLevelLT.torchDutyEx[10] = 0;
    p->engLevelLT.torchDutyEx[11] = 0;
    p->engLevelLT.torchDutyEx[12] = 0;
    p->engLevelLT.torchDutyEx[13] = 0;
    p->engLevelLT.torchDutyEx[14] = 0;
    p->engLevelLT.torchDutyEx[15] = 0;
    p->engLevelLT.torchDutyEx[16] = 0;
    p->engLevelLT.torchDutyEx[17] = 0;
    p->engLevelLT.torchDutyEx[18] = 0;
	//torch
    p->engLevelLT.torchDutyEx[19] = 0;
	//af
    p->engLevelLT.afDuty = 0;
	//pf, mf, normal
    p->engLevelLT.pfDuty = 0;
    p->engLevelLT.mfDutyMax = 25;
    p->engLevelLT.mfDutyMin = -1;
	//low bat
    p->engLevelLT.pfDutyL = 0;
    p->engLevelLT.mfDutyMaxL = 3;
    p->engLevelLT.mfDutyMinL = -1;
	//burst setting
    p->engLevelLT.pfDutyB = 0;
    p->engLevelLT.mfDutyMaxB = 3;
    p->engLevelLT.mfDutyMinB = -1;
#elif defined(SKY81294)
	//torch
	p->engLevel.torchDuty = 1;
	//af
	p->engLevel.afDuty = 1;
	//pf, mf, normal
	p->engLevel.pfDuty = 1;
	p->engLevel.mfDutyMax = 15;
	p->engLevel.mfDutyMin = 3;
	//low bat
	p->engLevel.IChangeByVBatEn=0;
	p->engLevel.vBatL = 3250;	//mv
	p->engLevel.pfDutyL = 1;
	p->engLevel.mfDutyMaxL = 3;
	p->engLevel.mfDutyMinL = 0;
	//burst setting
	p->engLevel.IChangeByBurstEn=1;
	p->engLevel.pfDutyB = 1;
	p->engLevel.mfDutyMaxB = 2;
	p->engLevel.mfDutyMinB = 0;
	//high current setting
	p->engLevel.decSysIAtHighEn = 1;
	p->engLevel.dutyH = 15;

#elif defined(LM3642TLX) || defined(SY7804)
	//torch
	p->engLevel.torchDuty = 1;
	//af
	p->engLevel.afDuty = 1;
	//pf, mf, normal
	p->engLevel.pfDuty = 1;
	p->engLevel.mfDutyMax = 10;
	p->engLevel.mfDutyMin = 3;
	//low bat
	p->engLevel.IChangeByVBatEn=0;
	p->engLevel.vBatL = 3250;	//mv
	p->engLevel.pfDutyL = 1;
	p->engLevel.mfDutyMaxL = 3;
	p->engLevel.mfDutyMinL = 0;
	//burst setting
	p->engLevel.IChangeByBurstEn=1;
	p->engLevel.pfDutyB = 1;
	p->engLevel.mfDutyMaxB = 2;
	p->engLevel.mfDutyMinB = 0;
	//high current setting
	p->engLevel.decSysIAtHighEn = 1;
	p->engLevel.dutyH = 10;

#elif defined(SGM37891)
	//torch
	p->engLevel.torchDuty = 0;
	//af
	p->engLevel.afDuty = 0;
	//pf, mf, normal
	p->engLevel.pfDuty = 0;
	p->engLevel.mfDutyMax = 6;
	p->engLevel.mfDutyMin = 0;
	//low bat
	p->engLevel.IChangeByVBatEn=0;
	p->engLevel.vBatL = 3250;	//mv
	p->engLevel.pfDutyL = 0;
	p->engLevel.mfDutyMaxL = 3;
	p->engLevel.mfDutyMinL = 0;
	//burst setting
	p->engLevel.IChangeByBurstEn=1;
	p->engLevel.pfDutyB = 0;
	p->engLevel.mfDutyMaxB = 2;
	p->engLevel.mfDutyMinB = 0;
	//high current setting
	p->engLevel.decSysIAtHighEn = 1;
	p->engLevel.dutyH = 6;

#else //MTK MOS
	//torch
	p->engLevel.torchDuty = 0;
	//af
	p->engLevel.afDuty = 0;
	//pf, mf, normal
	p->engLevel.pfDuty = 0;
	p->engLevel.mfDutyMax = 1;
	p->engLevel.mfDutyMin = 0;
	//low bat
	p->engLevel.IChangeByVBatEn=0;
	p->engLevel.vBatL = 3250;	//mv
	p->engLevel.pfDutyL = 0;
	p->engLevel.mfDutyMaxL = 0;
	p->engLevel.mfDutyMinL = 0;
	//burst setting
	p->engLevel.IChangeByBurstEn=1;
	p->engLevel.pfDutyB = 0;
	p->engLevel.mfDutyMaxB = 0;
	p->engLevel.mfDutyMinB = 0;
	//high current setting
	p->engLevel.decSysIAtHighEn = 1;
	p->engLevel.dutyH = 1;
#endif

	p->dualTuningPara.toleranceEV_pos = 10; //0.1 EV
	p->dualTuningPara.toleranceEV_neg = 10; //0.1 EV

	p->dualTuningPara.XYWeighting = 64;  //0.5  , 128 base
	p->dualTuningPara.useAwbPreferenceGain = 0; //the same with environment lighting condition
	p->dualTuningPara.envOffsetIndex[0] = -200;
	p->dualTuningPara.envOffsetIndex[1] = -100;
	p->dualTuningPara.envOffsetIndex[2] = 50;
	p->dualTuningPara.envOffsetIndex[3] = 150;

	p->dualTuningPara.envXrOffsetValue[0] = 0;
	p->dualTuningPara.envXrOffsetValue[1] = 0;
	p->dualTuningPara.envXrOffsetValue[2] = 0;
	p->dualTuningPara.envXrOffsetValue[3] = 0;

    p->dualTuningPara.envYrOffsetValue[0] = 0;
    p->dualTuningPara.envYrOffsetValue[1] = 0;
    p->dualTuningPara.envYrOffsetValue[2] = 0;
    p->dualTuningPara.envYrOffsetValue[3] = 0;

	return 0;
}

