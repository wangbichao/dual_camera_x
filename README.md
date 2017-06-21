# dual_camera_x

lunch full_aeon6737m_65_d_n-eng //

vendor/mediatek/proprietary/hardware/mtkcam/
                    ----main
                    ----legacy/platform/
                            ----mt6735
                            ----mt6735m/
                                ----v1
                                ----v3
vendor/mediatek/proprietary/custom/mt6735/hal/
                    ----D2
                    ----D1/imgsensor_src/sensorlist.cpp

20170621会议记录：
后续工作：
1：申请buffer方式，13M+5M  HAL申请13MX1.5 buffer +申请5MX1.5 buffer 。
2：接受frame precess 方法位置。
3：新建 xchip_Node 拆分数据。
4：xchip算法库放置位置。
客户沟通：
需客户尽快提供算法库。