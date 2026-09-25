#include "dahengTwoCams_qt_vs.h"

dahengTwoCams_qt_vs::dahengTwoCams_qt_vs(QWidget *parent)
    : QMainWindow(parent)
    , m_dEditShutter(0)
    , m_dEditGain(0)
    , m_nEditSnapSpeed(0)
    , m_bCheckShowDevice(false)
{
    this->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    ui.setupUi(this);
    m_pLLabelStaBar = new QLabel(this);
    m_pRLabelStaBar = new QLabel(this);


    statusBar()->addWidget(m_pLLabelStaBar);
    statusBar()->addWidget(m_pRLabelStaBar);



    m_Timer = new QTimer(this);

    m_bIsSnapSpeed = false;
    m_bIsColorFilter = false;
    m_dShutterMax = 0;
    m_dShutterMin = 0;
    m_dGainMax = 0;
    m_dGainMin = 0;
    m_nSnapSpeedMax = 0;
    m_nSnapSpeedMin = 0;
    m_nDeviceListCurrent = 0;
    m_nCurrentBalanceAutoWhiteSel = 0;
    m_strBalanceWhiteAuto = "Off";
    m_pDeviceProcessCurrent = NULL;


    //??????豸??????
    for (int i = 0; i < DEVICE_CONTS; i++)
    {
        m_pDeviceProcess[i] = NULL;
    }

    //????????????
    connect(m_Timer, &QTimer::timeout, this, &dahengTwoCams_qt_vs::time_update);
    connect(ui.pB_refreshDeviceList, &QPushButton::clicked, this, &dahengTwoCams_qt_vs::on_Btn_refreshDeviceList_clicked);
    connect(ui.start0,&QPushButton::clicked,this,&dahengTwoCams_qt_vs::visionprocess);
    //?????API
    __InitCGXAPI();

    //????????????
    connect(ui.cB_DeviceList, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &dahengTwoCams_qt_vs::on_Cb_DeviceList_currentIndexChanged);
}

void dahengTwoCams_qt_vs::closeEvent(QCloseEvent *event)
{
    // 无论事件来源（自发/非自发），都只隐藏窗口，不释放资源
    this->hide();
    event->ignore(); // 忽略关闭事件，窗口不被销毁
    // qDebug() << "dahengForm 已隐藏，资源未释放";
}

// dahengTwoCams_qt_vs.cpp 中实现析构函数
dahengTwoCams_qt_vs::~dahengTwoCams_qt_vs()
{
    // qDebug() << "dahengForm 析构，开始释放资源";

    // 1. 关闭所有相机设备（SDK资源，安全）
    try
    {
        map<int, CDeviceProcess*>::iterator it;
        for (it = m_mapDeviceInformation.begin(); it != m_mapDeviceInformation.end(); ++it)
        {
            if (it->second != nullptr) { // 防空指针（双重保险）
                it->second->CloseDevice();
            }
        }
        m_mapDeviceInformation.clear();
        disconnect(); // 断开信号槽，避免无效回调
    }
    catch (CGalaxyException)
    {
        // qDebug() << "相机关闭异常"; // 建议保留日志，方便调试
    }
    catch (std::exception)
    {
        // qDebug() << "标准库异常";
    }

    // 2. 反初始化大恒SDK（必须在所有相机关闭后执行）
    try
    {
        IGXFactory::GetInstance().Uninit();
    }
    catch (CGalaxyException)
    {
        // qDebug() << "SDK反初始化异常";
    }

    // 3. 释放堆内存（关键：判断非空，释放后置空）
    for (int i = 0; i < DEVICE_CONTS; i++)
    {
        if (m_pDeviceProcess[i] != nullptr)
        {
            delete m_pDeviceProcess[i];
            m_pDeviceProcess[i] = nullptr; // 避免野指针（极端情况防护）
        }
    }

    // qDebug() << "dahengForm 资源释放完成";
}

void dahengTwoCams_qt_vs::__InitCGXAPI()
{
    try
    {
        for (int i = 0; i < DEVICE_CONTS; i++)
        {
            m_pDeviceProcess[i] = new CDeviceProcess;
        }
        IGXFactory::GetInstance().Init();
        IGXFactory::GetInstance().UpdateDeviceList(1000, m_vectorDeviceInfo);
        __FilterUsbOnly(m_vectorDeviceInfo);
        if (m_vectorDeviceInfo.size() <= 0)
        {
            return;
        }
        // 1) 构造一个索引数组 [0,1,2,...]
        m_sortedIndexBySN.clear();
        for (size_t i = 0; i < m_vectorDeviceInfo.size(); ++i)
            m_sortedIndexBySN.push_back(static_cast<int>(i));
        // 2) 按 SN 字符串从小到大排序
        std::sort(m_sortedIndexBySN.begin(), m_sortedIndexBySN.end(),
                  [this](int a, int b)
                  {
                      return std::string(m_vectorDeviceInfo[a].GetSN()) <
                             std::string(m_vectorDeviceInfo[b].GetSN());
                  });
        // 3) 根据排序后的结果填充 UI 和 m_mapDeviceInformation
        ui.cB_DeviceList->clear();
        m_mapDeviceInformation.clear();
        int deviceCount = std::min<int>(DEVICE_CONTS,
                                        static_cast<int>(m_sortedIndexBySN.size()));
        for (int logicalIndex = 0; logicalIndex < deviceCount; ++logicalIndex)
        {
            int enumIndex = m_sortedIndexBySN[logicalIndex]; // 在 m_vectorDeviceInfo 里的下标
            gxstring strDeviceInformation = m_vectorDeviceInfo[enumIndex].GetDisplayName();
            ui.cB_DeviceList->addItem(strDeviceInformation.c_str());
            // 用“逻辑索引”作为 key：0 -> SN 最小，1 -> 第二小
            m_mapDeviceInformation.insert(
                std::map<int, CDeviceProcess*>::value_type(
                    logicalIndex,
                    m_pDeviceProcess[logicalIndex]
                    ));
            __UpdateUI(m_pDeviceProcess[logicalIndex]);
        }
        m_Timer->start(1000);
        ui.cB_DeviceList->setCurrentIndex(0);
        // 默认当前设备为逻辑索引 0（SN 最小）
        m_nDeviceListCurrent  = 0;
        m_pDeviceProcessCurrent = m_mapDeviceInformation[0];
    }
    catch (CGalaxyException& e)
    {
        m_pRLabelStaBar->setText(e.what());
        return;
    }
    catch (std::exception& e)
    {
        m_pRLabelStaBar->setText(e.what());
        return;
    }
}
void dahengTwoCams_qt_vs::__InitUI(CGXFeatureControlPointer objFeatureControlPtr)
{
    if (objFeatureControlPtr.IsNull())
    {
        return;
    }
    //???????????????????????????
    bool bIsBalanceWhiteAutoRead = false;        // ??????????????
    bool bBalanceWhiteAuto = false;        // ??????????????

    //??????Bayer???
    m_bIsColorFilter = objFeatureControlPtr->IsImplemented("PixelColorFilter");

    if (m_bIsColorFilter)
    {
        //??????????????????
        //??????????
        bBalanceWhiteAuto = objFeatureControlPtr->IsImplemented("BalanceWhiteAuto");

        //???????????
        bIsBalanceWhiteAutoRead = objFeatureControlPtr->IsReadable("BalanceWhiteAuto");
        if (bBalanceWhiteAuto)
        {
            if (bIsBalanceWhiteAutoRead)
            {
                m_strBalanceWhiteAuto = objFeatureControlPtr->GetEnumFeature("BalanceWhiteAuto")->GetValue();
            }
            int nCursel = 0;
            gxstring strCurEnumList = "";
            GxIAPICPP::gxstring_vector vectorEnumEntryList;

            //???????豸?????
            strCurEnumList = objFeatureControlPtr->GetEnumFeature("BalanceWhiteAuto")->GetValue();

            //????豸?????????
            vectorEnumEntryList = objFeatureControlPtr->GetEnumFeature("BalanceWhiteAuto")->GetEnumEntryList();

            //??????????б?
            for (uint32_t i = 0; i < vectorEnumEntryList.size(); i++)
            {
                std::string strEnumList = vectorEnumEntryList[i].c_str();
                if (strCurEnumList == vectorEnumEntryList[i])
                {
                    nCursel = i;
                }

            }
            m_nCurrentBalanceAutoWhiteSel = nCursel;
        }
    }


    //???????????
    QString  strShutterTimeRange = "";          // ??????Χ
    gxstring strShutterTimeUint = "";          // ??????λ

    m_dEditShutter = objFeatureControlPtr->GetFloatFeature("ExposureTime")->GetValue();
    strShutterTimeUint = objFeatureControlPtr->GetFloatFeature("ExposureTime")->GetUnit();
    m_dShutterMax = objFeatureControlPtr->GetFloatFeature("ExposureTime")->GetMax();
    m_dShutterMin = objFeatureControlPtr->GetFloatFeature("ExposureTime")->GetMin();
    // qDebug()<<"bao guang shi jian:"<<m_dEditShutter;
    strShutterTimeRange = QString().sprintf("ExposureTime(%.4f~%.4f)%s", m_dShutterMin, m_dShutterMax, strShutterTimeUint.c_str());
    //?????????
    QString  strGainRange = "";                // ???????Χ
    gxstring strGainUint = "";                // ????λ

    m_dEditGain = objFeatureControlPtr->GetFloatFeature("Gain")->GetValue();
    strGainUint = objFeatureControlPtr->GetFloatFeature("Gain")->GetUnit();
    m_dGainMax = objFeatureControlPtr->GetFloatFeature("Gain")->GetMax();
    m_dGainMin = objFeatureControlPtr->GetFloatFeature("Gain")->GetMin();


    strGainRange = QString().sprintf("Gain(%.4f~%.4f)%s", m_dGainMin, m_dGainMax, strGainUint.c_str());

    //??????????????
    //??????????????
    m_bIsSnapSpeed = objFeatureControlPtr->IsImplemented("AcquisitionSpeedLevel");
    if (m_bIsSnapSpeed)
    {
        QString  strSnapSpeedRange = "";
        m_nEditSnapSpeed = objFeatureControlPtr->GetIntFeature("AcquisitionSpeedLevel")->GetValue();
        m_nSnapSpeedMax = objFeatureControlPtr->GetIntFeature("AcquisitionSpeedLevel")->GetMax();
        m_nSnapSpeedMin = objFeatureControlPtr->GetIntFeature("AcquisitionSpeedLevel")->GetMin();

        strSnapSpeedRange = QString().sprintf("AcquisitionSpeedLevel(%lld~%lld)", m_nSnapSpeedMin, m_nSnapSpeedMax);
    }
    else
    {
        //??±???
        m_nEditSnapSpeed = NULL;
    }

    //??????豸???????豸??????????
    m_bCheckShowDevice = m_pDeviceProcessCurrent->GetShowSN();

}

void dahengTwoCams_qt_vs::__UpdateUI(CDeviceProcess* pDeviceProcess)
{
    if (pDeviceProcess == NULL)
    {
        return;
    }

    bool IsOpen = pDeviceProcess->IsOpen();
    bool IsSnap = pDeviceProcess->IsSnap();
}


void dahengTwoCams_qt_vs::time_update()
{
    try
    {
        //??????????????Once,???ó?????????????????????off
        //??????????????豸????????????????UI?????????
        QString strCurText = "";
        if (m_strBalanceWhiteAuto == "Once")
        {
            //????豸?????????
            m_strBalanceWhiteAuto = m_pDeviceProcessCurrent->m_objFeatureControlPtr->GetEnumFeature("BalanceWhiteAuto")
                ->GetValue();
            //????豸??????????
            GxIAPICPP::gxstring_vector vectorEnumEntryList;
            vectorEnumEntryList = m_pDeviceProcessCurrent->m_objFeatureControlPtr->GetEnumFeature("BalanceWhiteAuto")
                ->GetEnumEntryList();
            //?ж?????????????Off
            if (m_strBalanceWhiteAuto == "Off")
            {
                for (uint32_t i = 0; i < vectorEnumEntryList.size(); i++)
                {
                    if (strCurText == "Off")
                    {
                        break;
                    }
                }

            }

        }
        m_Timer->stop();
    }
    catch (CGalaxyException)
    {
        m_Timer->stop();
        return;
    }
    catch (std::exception)
    {
        m_Timer->stop();
        return;
    }
}

void dahengTwoCams_qt_vs::on_Btn_refreshDeviceList_clicked()
{
    try
    {
        //????map??????????????豸??????
        map<int, CDeviceProcess*>::iterator it;
        for (it = m_mapDeviceInformation.begin(); it != m_mapDeviceInformation.end(); ++it)
        {
            it->second->CloseDevice();

        }
        //?????豸???????
        m_mapDeviceInformation.clear();
        m_vectorDeviceInfo.clear();

    }
    catch (CGalaxyException)
    {
        //do noting

    }
    catch (std::exception)
    {
        //do noting
    }

    //??????ж???
    for (int i = 0; i < DEVICE_CONTS; i++)
    {
        if (m_pDeviceProcess[i] != NULL)
        {
            delete m_pDeviceProcess[i];
            m_pDeviceProcess[i] = NULL;
        }
    }

    disconnect(ui.cB_DeviceList, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &dahengTwoCams_qt_vs::on_Cb_DeviceList_currentIndexChanged);

    try
    {
        for (int i = 0; i < DEVICE_CONTS; i++)
        {
            m_pDeviceProcess[i] = new CDeviceProcess;
        }

        //????豸
        IGXFactory::GetInstance().UpdateDeviceList(1000, m_vectorDeviceInfo);


        __FilterUsbOnly(m_vectorDeviceInfo);  // ← 新增此行

        //δ?????豸
        if (m_vectorDeviceInfo.size() <= 0)
        {
            return;
        }

        ui.cB_DeviceList->clear();
        //???豸??????????豸?б???
        m_sortedIndexBySN.clear();
        for (size_t i = 0; i < m_vectorDeviceInfo.size(); ++i)
            m_sortedIndexBySN.push_back(static_cast<int>(i));
        // 2) 按 SN 字符串从小到大排序
        std::sort(m_sortedIndexBySN.begin(), m_sortedIndexBySN.end(),
                  [this](int a, int b)
                  {
                      return std::string(m_vectorDeviceInfo[a].GetSN()) <
                             std::string(m_vectorDeviceInfo[b].GetSN());
                  });
        // 3) 根据排序后的结果填充 UI 和 m_mapDeviceInformation
        ui.cB_DeviceList->clear();
        m_mapDeviceInformation.clear();
        int deviceCount = std::min<int>(DEVICE_CONTS,
                                        static_cast<int>(m_sortedIndexBySN.size()));
        for (int logicalIndex = 0; logicalIndex < deviceCount; ++logicalIndex)
        {
            int enumIndex = m_sortedIndexBySN[logicalIndex]; // 在 m_vectorDeviceInfo 里的下标
            gxstring strDeviceInformation = m_vectorDeviceInfo[enumIndex].GetDisplayName();
            ui.cB_DeviceList->addItem(strDeviceInformation.c_str());
            // 用“逻辑索引”作为 key：0 -> SN 最小，1 -> 第二小
            m_mapDeviceInformation.insert(
                std::map<int, CDeviceProcess*>::value_type(
                    logicalIndex,
                    m_pDeviceProcess[logicalIndex]
                    ));
            __UpdateUI(m_pDeviceProcess[logicalIndex]);
        }

        ui.cB_DeviceList->setCurrentIndex(0);

        connect(ui.cB_DeviceList, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &dahengTwoCams_qt_vs::on_Cb_DeviceList_currentIndexChanged);

        //????????????豸??????????
        m_pDeviceProcessCurrent = m_mapDeviceInformation[0];

    }
    catch (CGalaxyException& e)
    {
        connect(ui.cB_DeviceList, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &dahengTwoCams_qt_vs::on_Cb_DeviceList_currentIndexChanged);
        m_pRLabelStaBar->setText(e.what());
        return;
    }
    catch (std::exception& e)
    {
        connect(ui.cB_DeviceList, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &dahengTwoCams_qt_vs::on_Cb_DeviceList_currentIndexChanged);
        m_pRLabelStaBar->setText(e.what());
        return;
    }

}

void dahengTwoCams_qt_vs::on_Btn_OpenDevice_clicked()
{
    try
    {
        //??????????????????
        CGXBitmap *pBitmap;

        switch (m_nDeviceListCurrent)
        {
        case 0:
            pBitmap = (CGXBitmap *)ui.bitMap_LCam;
            break;
        case 1:
            pBitmap = (CGXBitmap *)ui.bitMap_RCam;
            break;
        default: break;
        }

        //????豸????
        int nDeviceIndex = m_nDeviceListCurrent + 1;

        //?????豸
        m_pDeviceProcessCurrent->OpenDevice(m_vectorDeviceInfo[m_nDeviceListCurrent].GetSN(), pBitmap, nDeviceIndex);

        //?????????
        __InitUI(m_pDeviceProcessCurrent->m_objFeatureControlPtr);

        //???????
        __UpdateUI(m_pDeviceProcessCurrent);

    }
    catch (CGalaxyException& e)
    {
        m_pRLabelStaBar->setText(e.what());
        return;
    }
    catch (std::exception& e)
    {
        m_pRLabelStaBar->setText(e.what());
        return;
    }
}

void dahengTwoCams_qt_vs::on_Btn_CloseDevice_clicked()
{
    try
    {
        //????豸
        m_pDeviceProcessCurrent->CloseDevice();


        //???????
        m_bVisionStarted = false;
        __UpdateUI(m_pDeviceProcessCurrent);
    }
    catch (CGalaxyException)
    {
        //do noting
    }
    catch (std::exception)
    {
        //do noting
        return;
    }
}

void dahengTwoCams_qt_vs::on_Btn_StartCapture_clicked()
{
    // TODO: Add your control notification handler code here
    try
    {
        //??????
        m_pDeviceProcessCurrent->StartSnap();

        //???????
        __UpdateUI(m_pDeviceProcessCurrent);

        //?????豸??????????
        m_pDeviceProcessCurrent->RefreshDeviceSN();

    }
    catch (CGalaxyException& e)
    {
        m_pRLabelStaBar->setText(e.what());
        return;
    }
    catch (std::exception& e)
    {
        m_pRLabelStaBar->setText(e.what());
        return;
    }
}

void dahengTwoCams_qt_vs::on_Btn_StopCapture_clicked()
{
    try
    {
        //?????
        m_pDeviceProcessCurrent->StopSnap();

        //???????
        __UpdateUI(m_pDeviceProcessCurrent);

    }
    catch (CGalaxyException& e)
    {
        m_pRLabelStaBar->setText(e.what());
        return;
    }
    catch (std::exception& e)
    {
        m_pRLabelStaBar->setText(e.what());
        return;
    }
}

void dahengTwoCams_qt_vs::on_Cb_DeviceList_currentIndexChanged(int nIndex)
{
    try
    {
        m_nDeviceListCurrent = nIndex;

        m_pDeviceProcessCurrent = m_mapDeviceInformation[m_nDeviceListCurrent];

        //?ж????豸????????
        bool bIsOpen = m_pDeviceProcessCurrent->IsOpen();
        if (!bIsOpen)
        {
            __UpdateUI(m_pDeviceProcessCurrent);
        }

        else
        {
            //??????
            __InitUI(m_pDeviceProcessCurrent->m_objFeatureControlPtr);

            //???????
            __UpdateUI(m_pDeviceProcessCurrent);

        }
    }
    catch (CGalaxyException& e)
    {
        m_pRLabelStaBar->setText(e.what());

        return;

    }
    catch (std::exception& e)
    {
        m_pRLabelStaBar->setText(e.what());
        return;
    }

}

void dahengTwoCams_qt_vs::on_lE_Shutter_editingFinished()
{
    double dShutterOld = m_dEditShutter;
    if (!m_pDeviceProcessCurrent->IsOpen())
    {
        return;
    }
    try
    {
        //判断曝光时间值是否合理
        if (m_dEditShutter < m_dShutterMin)
        {
            m_dEditShutter = m_dShutterMin;
        }

        if (m_dEditShutter > m_dShutterMax)
        {
            m_dEditShutter = m_dShutterMax;
        }

        //设置曝光时间
        m_pDeviceProcessCurrent->m_objFeatureControlPtr->GetFloatFeature("ExposureTime")->SetValue(m_dEditShutter);

    }
    catch (CGalaxyException& e)
    {
        m_dEditShutter = dShutterOld;
        m_pRLabelStaBar->setText(e.what());
    }
    catch (std::exception& e)
    {
        m_dEditShutter = dShutterOld;
        m_pRLabelStaBar->setText(e.what());
    }
}

void dahengTwoCams_qt_vs::setShutter_us(int deviceIndex, double dExposureUs)
{
    double dShutterOld = m_dEditShutter;
    double dNewShutter = dExposureUs; // 曝光时间(us)，由外部传入
    // 检查设备索引是否有效
    auto it = m_mapDeviceInformation.find(deviceIndex);
    if (it == m_mapDeviceInformation.end() || it->second == nullptr)
    {
        // qDebug() << "wu xiao de she bei suo yin";
        return;
    }
    CDeviceProcess* pDeviceProcess = it->second;
    // 检查设备是否已打开
    if (!pDeviceProcess->IsOpen())
    {
        // qDebug() << "she bei wei da kai";
        return;
    }
    try
    {
        // 从相机重新获取一次曝光时间的合法范围（避免使用旧的成员变量范围）
        auto pExposureFeature = pDeviceProcess->m_objFeatureControlPtr->GetFloatFeature("ExposureTime");
        double dMin = pExposureFeature->GetMin();
        double dMax = pExposureFeature->GetMax();
        // 验证传入值是否在合法范围内，超出则截断
        if (dNewShutter < dMin)
        {
            dNewShutter = dMin;
        }
        if (dNewShutter > dMax)
        {
            dNewShutter = dMax;
        }
        // 更新成员变量并设置到相机
        m_dEditShutter = dNewShutter;
        pExposureFeature->SetValue(m_dEditShutter);
    }
    catch (CGalaxyException& e)
    {
        m_dEditShutter = dShutterOld;
        m_pRLabelStaBar->setText(e.what());
    }
    catch (std::exception& e)
    {
        m_dEditShutter = dShutterOld;
        m_pRLabelStaBar->setText(e.what());
    }
}


// 新函数：通过设备索引号打开指定设备
bool dahengTwoCams_qt_vs::OpenDeviceByIndex(int deviceIndex)
{
    // deviceIndex 是“逻辑索引”：0 => SN 最小那台

    if (deviceIndex < 0 || deviceIndex >= (int)m_sortedIndexBySN.size())
        return false;


    auto it = m_mapDeviceInformation.find(deviceIndex);
    if (it == m_mapDeviceInformation.end() || it->second == nullptr)
        return false;
    CDeviceProcess* pDeviceProcess = it->second;
    // 已经打开，直接返回 true
    if (pDeviceProcess->IsOpen())
    {
        qDebug() << deviceIndex << "号相机已经打开，跳过重复打开";
        return true;
    }

    int enumIndex = m_sortedIndexBySN[deviceIndex]; // 在 m_vectorDeviceInfo 里的真实下标

    try
    {
        // 更新当前设备为逻辑索引 deviceIndex 对应的处理对象
        m_nDeviceListCurrent = deviceIndex;
        m_pDeviceProcessCurrent = m_mapDeviceInformation[deviceIndex];

        // 选择对应的显示窗口
        CGXBitmap *pBitmap = nullptr;
        switch (deviceIndex)
        {
        case 0:
            pBitmap = (CGXBitmap *)ui.bitMap_LCam;
            break;
        case 1:
            pBitmap = (CGXBitmap *)ui.bitMap_RCam;
            break;
        default:
            // 如超过两个，根据实际需求扩展
            return false;
        }

        // 这里传入的是 enumIndex 的 SN
        m_pDeviceProcessCurrent->OpenDevice(
            m_vectorDeviceInfo[enumIndex].GetSN(),
            pBitmap,
            deviceIndex + 1 // 显示用的“序号”，保持 1/2 等即可
            );

        __InitUI(m_pDeviceProcessCurrent->m_objFeatureControlPtr);
        __UpdateUI(m_pDeviceProcessCurrent);
        return true;
    }
    catch (CGalaxyException& e)
    {
        m_pRLabelStaBar->setText(e.what());
        throw;
    }
    catch (std::exception& e)
    {
        m_pRLabelStaBar->setText(e.what());
        throw;
    }
}


// 新函数：通过设备索引号开始指定设备的采集
void dahengTwoCams_qt_vs::StartCaptureByIndex(int deviceIndex)
{
    if (deviceIndex < 0 || deviceIndex >= static_cast<int>(m_sortedIndexBySN.size()))
    {
        qDebug() << "无效设备索引";
        return;
    }
    auto it = m_mapDeviceInformation.find(deviceIndex);
    if (it == m_mapDeviceInformation.end() || it->second == nullptr)
    {
        qDebug() << "设备指针不存在";
        return;
    }
    CDeviceProcess* pDeviceProcess = it->second;
    if (!pDeviceProcess->IsOpen())
    {
        qDebug() << "设备未打开，无法开始采集";
        return;
    }
    // 已经采集，直接返回
    if (pDeviceProcess->IsSnap())
    {
        qDebug() << deviceIndex << "号相机已经在采集，跳过重复启动";
        return;
    }
    try
    {
        // 先检查deviceIndex是否为负数（有符号判断），再转换为无符号类型比较大小
        if (deviceIndex < 0 || static_cast<size_t>(deviceIndex) >= m_vectorDeviceInfo.size())
        {
            // qDebug() << "wu xiao de suo yin";
        }

        // 获取指定索引的设备处理对象
        CDeviceProcess* pDeviceProcess = m_mapDeviceInformation[deviceIndex];

        // 检查设备是否已打开
        if (!pDeviceProcess->IsOpen())
        {
            // qDebug() << "she bei wei da kai";
            return;
        }

        // 更新当前设备指针（如果需要）
        // m_nDeviceListCurrent = deviceIndex;
        // m_pDeviceProcessCurrent = pDeviceProcess;

        // 调用指定设备的开始采集方法
        pDeviceProcess->StartSnap();

        // 更新界面
        __UpdateUI(pDeviceProcess);

        // 刷新设备型号及帧率信息
        pDeviceProcess->RefreshDeviceSN();

    }
    catch (CGalaxyException& e)
    {
        m_pRLabelStaBar->setText(e.what());
        throw;
    }
    catch (std::exception& e)
    {
        m_pRLabelStaBar->setText(e.what());
        throw;
    }
}

bool dahengTwoCams_qt_vs::visionprocess()
{
    QMutexLocker locker(&m_cameraMutex); // 加锁保护相机操作，避免并发问题
    if (m_bVisionStarted)
    {
        dahengUpdateUI("相机已经启动，无需重复启动");
        return true;
    }
    int deviceCount = m_vectorDeviceInfo.size();

    // 检查相机数量是否满足要求（至少2台）
    if (deviceCount < 2) {
        QString msg = (deviceCount == 0)
        ? "未检索到任何相机，请检查硬件连接"
        : QString("仅检索到%1台相机，需要至少2台").arg(deviceCount);
        dahengUpdateUI(msg);
        return false;
    }

    // 检查设备映射表中是否存在0号和1号设备
    auto it0 = m_mapDeviceInformation.find(0);
    auto it1 = m_mapDeviceInformation.find(1);
    if (it0 == m_mapDeviceInformation.end() || it0->second == nullptr) {
        dahengUpdateUI("0号相机设备指针不存在，无法操作");
        return false;
    }
    if (it1 == m_mapDeviceInformation.end() || it1->second == nullptr) {
        dahengUpdateUI("1号相机设备指针不存在，无法操作");
        return false;
    }

    CDeviceProcess* pDevice0 = it0->second;
    CDeviceProcess* pDevice1 = it1->second;

    // 操作0号相机
    if (!OpenDeviceByIndex(0)) {
        dahengUpdateUI("0号相机打开失败");
        return false;
    }
    if (!pDevice0->IsOpen()) { // 二次确认设备是否真的打开
        dahengUpdateUI("0号相机打开后状态异常");
        return false;
    }

    StartCaptureByIndex(0);
    if (!pDevice0->IsSnap()) { // 确认是否开始采集
        dahengUpdateUI("0号相机启动采集失败");
        pDevice0->CloseDevice(); // 失败时关闭已打开的设备
        return false;
    }

    setShutter_us(0,camera0_exposureTime);
    // 可根据实际需求添加曝光设置后的校验逻辑


    // 操作1号相机
    if (!OpenDeviceByIndex(1)) {
        dahengUpdateUI("1号相机打开失败");
        pDevice0->CloseDevice();
        return false;
    }
    if (!pDevice1->IsOpen()) {
        dahengUpdateUI("1号相机打开后状态异常");
        pDevice0->CloseDevice();
        return false;
    }

    StartCaptureByIndex(1);
    if (!pDevice1->IsSnap()) {
        dahengUpdateUI("1号相机启动采集失败");
        // 清理所有已打开的设备
        pDevice1->CloseDevice();
        pDevice0->CloseDevice();
        return false;
    }

    setShutter_us(1,camera1_exposureTime);
    // 可根据实际需求添加曝光设置后的校验逻辑

    m_bVisionStarted = true;
    dahengUpdateUI("2台相机均已成功连接并启动采集");
    return true;
}
cv::Mat dahengTwoCams_qt_vs::getlatestframe(int deviceIndex)
{

    QMutexLocker locker(&m_cameraMutex);
    CDeviceProcess* pDevice = m_mapDeviceInformation[deviceIndex];
    cv::Mat latestFrame9;
    if (pDevice == nullptr) {
        dahengUpdateUI("相机指针为空");
        return latestFrame9; // 或其他错误处理
    }
    cv::Mat latestFrame = pDevice->GetLatestMatFrame();
    return latestFrame.clone();
}



void dahengTwoCams_qt_vs::on_closecamera_clicked()
{
    QMutexLocker locker(&m_cameraMutex);
    try {
        // 遍历所有设备并关闭
        for (auto it = m_mapDeviceInformation.begin(); it != m_mapDeviceInformation.end(); ++it) {
            int deviceIndex = it->first;
            CDeviceProcess* pDevice = it->second;
            if (pDevice == nullptr) continue;

            // 停止采集
            if (pDevice->IsSnap()) {
                pDevice->StopSnap();
            }

            // 关闭设备
            if (pDevice->IsOpen()) {
                pDevice->CloseDevice();
            }

            // 清空显示框
            CGXBitmap* pBitmap = nullptr;
            switch (deviceIndex) {
            case 0:
                pBitmap = (CGXBitmap*)ui.bitMap_LCam;
                break;
            case 1:
                pBitmap = (CGXBitmap*)ui.bitMap_RCam;
                break;
            default:
                break;
            }
            if (pBitmap) {
                pBitmap->ClearDisplay();
            }
            m_bVisionStarted = false;
            // 更新UI
            __UpdateUI(pDevice);
        }
        dahengUpdateUI("所有相机已关闭");
    }
    catch (CGalaxyException& e) {
        dahengUpdateUI(e.what());
    }
    catch (std::exception& e) {
        dahengUpdateUI(e.what());
    }
}


void dahengTwoCams_qt_vs::__FilterUsbOnly(GxIAPICPP::gxdeviceinfo_vector& vec)
{
    // 先收集需要保留的索引
    std::vector<size_t> keepIndices;
    for (size_t i = 0; i < vec.size(); ++i)
    {
        GX_DEVICE_CLASS_LIST cls = vec[i].GetDeviceClass();
        if (cls == GX_DEVICE_CLASS_USB2 || cls == GX_DEVICE_CLASS_U3V)
        {
            keepIndices.push_back(i);
        }
    }

    // 收集要保留的元素副本
    GxIAPICPP::gxdeviceinfo_vector filtered;
    for (size_t idx : keepIndices)
    {
        filtered.push_back(vec[idx]);
    }

    // 清空原容器，再把保留的塞回去
    vec.clear();
    for (size_t i = 0; i < filtered.size(); ++i)
    {
        vec.push_back(filtered[i]);
    }
}


void dahengTwoCams_qt_vs::onParamsReceived(const LRUInnerParams &params)
{
    camera0_exposureTime  = params.camera0_exposureTime;
    camera1_exposureTime  = params.camera1_exposureTime;

}

