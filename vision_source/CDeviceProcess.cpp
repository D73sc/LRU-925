#include "CDeviceProcess.h"

//------------------------------------------------------------
/**
\file    DeviceProcess.cpp
\brief   Device Process
*/
//------------------------------------------------------------
CDeviceProcess::CDeviceProcess(void)
{
	m_pBitmap = NULL;
	m_pSampleCaptureEventHandle = NULL;
	m_bIsOpen = false;
	m_bIsSnap = false;
	m_bShowDeviceSN = true;
	m_nDeviceIndex = 0;
	m_strDeviceSN = "";
	m_pSampleCaptureEventHandle = new CSampleCaptureEventHandler();
	m_pRecordDeviceSN = new char[1024];
	m_strSavePath = ".\\Image\\";
}

CDeviceProcess::~CDeviceProcess(void)
{
	if (m_pSampleCaptureEventHandle != NULL)
	{
		delete m_pSampleCaptureEventHandle;
		m_pSampleCaptureEventHandle = NULL;
	}

	if (m_pRecordDeviceSN != NULL)
	{
		delete[] m_pRecordDeviceSN;
		m_pRecordDeviceSN = NULL;
	}
}

//------------------------------------------------------------
/**
\brief   Open Device
\param   strDeviceSN   [in]         设备序列号
\param   pBitmap       [in]         窗口句柄
\param   nDeviceIndex  [in]         设备序号
\return  void
*/
//------------------------------------------------------------
void CDeviceProcess::OpenDevice(gxstring strDeviceSN, CGXBitmap* pBitmap, int nDeviceIndex)
{
	bool bIsDeviceOpen = false;      ///< 判断设备是否已打开标志
	bool bIsStreamOpen = false;      ///< 判断设备流是否已打开标志
	try
	{
		if (m_bIsOpen || pBitmap == NULL || strDeviceSN == "")
		{
            qDebug() << "无效参数";
		}

		m_strDeviceSN = strDeviceSN;
		m_nDeviceIndex = nDeviceIndex;

		//打开设备
		m_objDevicePtr = IGXFactory::GetInstance().OpenDeviceBySN(strDeviceSN, GX_ACCESS_EXCLUSIVE);
		bIsDeviceOpen = true;

		//获取属性控制器
		m_objFeatureControlPtr = m_objDevicePtr->GetRemoteFeatureControl();

		//设置采集模式为连续采集
		m_objFeatureControlPtr->GetEnumFeature("AcquisitionMode")->SetValue("Continuous");

		//设置触发模式为关
		m_objFeatureControlPtr->GetEnumFeature("TriggerMode")->SetValue("Off");

		//获取设备流个数
		int nCount = m_objDevicePtr->GetStreamCount();

		if (nCount > 0)
		{
			m_objStreamPtr = m_objDevicePtr->OpenStream(0);
			m_objStreamFeatureControlPtr = m_objStreamPtr->GetFeatureControl();
			bIsStreamOpen = true;
		}
		else
		{
            qDebug() << "未发现设备流";
		}

		// 建议用户在打开网络相机之后，根据当前网络环境设置相机的流通道包长值，
		// 以提高网络相机的采集性能,设置方法参考以下代码。
		GX_DEVICE_CLASS_LIST objDeviceClass = m_objDevicePtr->GetDeviceInfo().GetDeviceClass();
		if (GX_DEVICE_CLASS_GEV == objDeviceClass)
		{
			// 判断设备是否支持流通道数据包功能
			if (true == m_objFeatureControlPtr->IsImplemented("GevSCPSPacketSize"))
			{
				// 获取当前网络环境的最优包长值
				int nPacketSize = m_objStreamPtr->GetOptimalPacketSize();
				// 将最优包长值设置为当前设备的流通道包长值
				m_objFeatureControlPtr->GetIntFeature("GevSCPSPacketSize")->SetValue(nPacketSize);
			}
		}

		m_bIsOpen = true;

		m_pBitmap = pBitmap;
		m_pBitmap->addDevicePtr(m_objDevicePtr);
	}
	catch (CGalaxyException& e)
	{

		//判断设备流是否已打开
		if (bIsStreamOpen)
		{
			m_objStreamPtr->Close();
		}

		//判断设备是否已打开
		if (bIsDeviceOpen)
		{
			m_objDevicePtr->Close();
		}

		m_bIsOpen = false;

		throw e;

	}
	catch (std::exception& e)
	{
		//判断设备流是否已打开
		if (bIsStreamOpen)
		{
			m_objStreamPtr->Close();
		}

		//判断设备是否已打开
		if (bIsDeviceOpen)
		{
			m_objDevicePtr->Close();
		}

		m_bIsOpen = false;

		throw e;
	}

}
//------------------------------------------------------------
/**
\brief   Close Device

\return  void
*/
//------------------------------------------------------------
void CDeviceProcess::CloseDevice()
{
	if (!m_bIsOpen)
	{
		return;
	}

	try
	{
		//判断是否停止采集
		if (m_bIsSnap)
		{

			//发送停采命令
			m_objFeatureControlPtr->GetCommandFeature("AcquisitionStop")->Execute();

			//关闭流层采集
			m_objStreamPtr->StopGrab();

			//注销回调函数
			m_objStreamPtr->UnregisterCaptureCallback();
		}
	}
	catch (CGalaxyException)
	{
		//do noting
	}

	try
	{
		//关闭流对象
		m_objStreamPtr->Close();
	}
	catch (CGalaxyException)
	{
		//do noting
	}

	try
	{
		//关闭设备
		m_objDevicePtr->Close();
	}
	catch (CGalaxyException)
	{
		//do noting
	}

	m_bIsSnap = false;
	m_bIsOpen = false;

}
//------------------------------------------------------------
/**
\brief   Start Snap

\return  void
*/
//------------------------------------------------------------
void CDeviceProcess::StartSnap()
{
	//判断设备是否已打开
	if (!m_bIsOpen)
	{
		return;
	}

	try
	{
		try
		{
			//设置Buffer处理模式
			m_objStreamFeatureControlPtr->GetEnumFeature("StreamBufferHandlingMode")->SetValue("OldestFirst");
		}
		catch (...)
		{
		}

		//注册回调函数
		m_objStreamPtr->RegisterCaptureCallback(m_pSampleCaptureEventHandle, this);
	}
	catch (CGalaxyException& e)
	{
		throw e;
	}

	try
	{

		//开启流层采集
		m_objStreamPtr->StartGrab();
	}
	catch (CGalaxyException& e)
	{
		//注销回调函数
		m_objStreamPtr->UnregisterCaptureCallback();
		throw e;
	}

	try
	{
		//发送开采命令
		m_objFeatureControlPtr->GetCommandFeature("AcquisitionStart")->Execute();

		m_bIsSnap = true;
	}
	catch (CGalaxyException& e)
	{
		//关闭流层采集
		m_objStreamPtr->StopGrab();

		//注销回调函数
		m_objStreamPtr->UnregisterCaptureCallback();
		throw e;
	}
	catch (std::exception& e)
	{
		//关闭流层采集
		m_objStreamPtr->StopGrab();

		//注销回调函数
		m_objStreamPtr->UnregisterCaptureCallback();
		throw e;
	}

}

//------------------------------------------------------------
/**
\brief   Stop Snap

\return  void
*/
//------------------------------------------------------------
void CDeviceProcess::StopSnap()
{
	//判断设备是否已打开
	if (!m_bIsOpen || !m_bIsSnap)
	{
		return;
	}

	try
	{
		//发送停采命令
		m_objFeatureControlPtr->GetCommandFeature("AcquisitionStop")->Execute();

		//关闭流层采集
		m_objStreamPtr->StopGrab();

		//注销回调函数
		m_objStreamPtr->UnregisterCaptureCallback();

		m_bIsSnap = false;
	}
	catch (CGalaxyException& e)
	{
		throw e;

	}
	catch (std::exception& e)
	{
		throw e;

	}
}

//------------------------------------------------------------
/**
\brief   Get Device Open Flag

\return  void
*/
//------------------------------------------------------------
bool CDeviceProcess::IsOpen()
{
	return m_bIsOpen;
}

//------------------------------------------------------------
/**
\brief   Get Device Snap Flag

\return  void
*/
//------------------------------------------------------------
bool CDeviceProcess::IsSnap()
{
	return m_bIsSnap;
}

//------------------------------------------------------------
/**
\brief   Set Show frame Frequency Flag

\return  void
*/
//------------------------------------------------------------
void CDeviceProcess::SetShowSN(bool bIsShowFps)
{
	m_bShowDeviceSN = bIsShowFps;
}

//------------------------------------------------------------
/**
\brief   Get Show frame Frequency Flag

\return  void
*/
//------------------------------------------------------------
bool CDeviceProcess::GetShowSN()
{
	return m_bShowDeviceSN;
}


//------------------------------------------------------------
/**
\brief   Refresh Device SN

\return  void
*/
//------------------------------------------------------------
void CDeviceProcess::RefreshDeviceSN()
{
	sprintf_s(m_pRecordDeviceSN, 1024,
		"序号:%d SN:%s",
		m_nDeviceIndex,
		m_strDeviceSN.c_str());
}

//------------------------------------------------------------
/**
\brief   Show Image
\param   objImageDataPointer [in]       图像信息

\return  void
*/
//------------------------------------------------------------
void CDeviceProcess::__ShowPicture(CImageDataPointer& objImageDataPointer)
{
    cv::Mat image;
    //显示图像
    image = m_pBitmap->Show(objImageDataPointer);
    SetLatestMatFrame(image);
}
//----------------------------------------------------------------------------------
/**
\brief   将图像保存成BMP图片
\param   objImageDataPointer	[in]         图像数据指针
\param   qstrImageName			[in]         保存图像的名称
\return  无
*/
//----------------------------------------------------------------------------------
void CDeviceProcess::SavePicture(CImageDataPointer& objImageDataPointer)
{


}
void CDeviceProcess::setSavePath(QString strFileName)
{
	m_strSavePath = strFileName.toStdString();
}
//----------------------------------------------------------------------------------
/**
\brief   将string转换为LPCWSTR格式
\param   orig		[in]         string格式数据
\return  LPCWSTR
\param   wcstring	[out]		LPCWSTR格式数据
*/
//----------------------------------------------------------------------------------
LPCWSTR CDeviceProcess::stringToLPCWSTR(std::string orig)
{
	size_t origsize = orig.length() + 1;
	const size_t newsize = 100;
	size_t convertedChars = 0;
	wchar_t* wcstring = (wchar_t*)malloc(sizeof(wchar_t) * (orig.length() - 1));
	mbstowcs_s(&convertedChars, wcstring, origsize, orig.c_str(), _TRUNCATE);

	return wcstring;
}
//----------------------------------------------------------------------------------
/**
\brief   设置保存图像标识和保存图像的名称
\param   bSaveImage				[in]         保存图像标识
\param   qstrImageName			[in]         保存图像的名称
\return  无
*/
//----------------------------------------------------------------------------------
void CDeviceProcess::setSaveImage(bool bSaveImage, QString qstrImageName)
{
	m_bSaveImage = bSaveImage;
	m_strFileName = qstrImageName.toStdString();
}


void CDeviceProcess::SetLatestMatFrame(const cv::Mat& mat)
{
    QMutexLocker locker(&m_matMutex);
    m_latestMat.release();
    m_latestMat = mat;
}

cv::Mat CDeviceProcess::GetLatestMatFrame()
{
    QMutexLocker locker(&m_matMutex);
    return m_latestMat.clone();  // 返回拷贝，避免外部修改
}
