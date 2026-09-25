#pragma once
#include "vision_source/CGXBitmap.h"
//---------------------------------------------------------------------------------
/**
\brief   设备处理类
*/
//----------------------------------------------------------------------------------
class CDeviceProcess
{
public:

	CDeviceProcess(void);
	~CDeviceProcess(void);

	//---------------------------------------------------------------------------------
	/**
	\brief   用户继承采集事件处理类
	*/
	//----------------------------------------------------------------------------------
	class CSampleCaptureEventHandler :public ICaptureEventHandler
	{
		//---------------------------------------------------------------------------------
		/**
		\brief   采集回调函数
		\param   objImageDataPointer      图像处理参数
		\param   pFrame                   用户参数

		\return  无
		*/
		//----------------------------------------------------------------------------------
		void DoOnImageCaptured(CImageDataPointer& objImageDataPointer, void* pUserParam)
		{
			try
			{
				CDeviceProcess* pDeviceProcess = (CDeviceProcess*)pUserParam;

                // 显示图像并保存转换后的cv::Mat
                pDeviceProcess->__ShowPicture(objImageDataPointer);
			}
			catch (CGalaxyException)
			{
				//do nothing

			}
			catch (std::exception)
			{
				//do nothing
			}
		}
	};



public:
	// 打开设备
	void OpenDevice(gxstring strDeviceSN, CGXBitmap* m_pBitmap, int nDeviceIndex);

	// 关闭设备
	void CloseDevice();

	// 开始采集
	void StartSnap();

	// 停止采集
	void StopSnap();

	// 获取设备打开标识
	bool IsOpen();

	// 获取设备采集标识
	bool IsSnap();

	// 设置是否显示帧率标志
	void SetShowSN(bool bIsShowFps);

	// 获取是否显示帧率标志
	bool GetShowSN();

	// 刷新设备型号及帧率信息
	void RefreshDeviceSN();

	//设置保存图像标志量
	void setSaveImage(bool bSaveImage, QString qstrImageName);

	// 保存图像
	void SavePicture(CImageDataPointer& objImageDataPointer);

	//设置保存路径
	void setSavePath(QString strFileName);


	//将string转换为LPCWSTR格式
	LPCWSTR stringToLPCWSTR(std::string orig);

	CGXFeatureControlPointer    m_objFeatureControlPtr;           ///< 属性控制器
	CGXFeatureControlPointer    m_objStreamFeatureControlPtr;     ///< 流层控制器对象


    // 设置最新的cv::Mat
    void SetLatestMatFrame(const cv::Mat& mat);

    // 获取最新的cv::Mat
    cv::Mat GetLatestMatFrame();

private:

	/// 显示图像
    void __ShowPicture(CImageDataPointer& objImageDataPointer);

	CGXDevicePointer            m_objDevicePtr;                  //设备句柄
	CGXStreamPointer            m_objStreamPtr;                  //设备流对象
	CGXBitmap*                  m_pBitmap;                       //画图指针
	CSampleCaptureEventHandler* m_pSampleCaptureEventHandle;     //回调指针
	bool                        m_bIsOpen;                       //设备是否打开标志
	bool                        m_bIsSnap;                       //设备是否开采标志
	bool                        m_bShowDeviceSN;                 //显示设备型号标志
	bool						m_bSaveImage;					 //是否保存图像标志
	char                        *m_pRecordDeviceSN;			     //设备型号信息
	gxstring                    m_strDeviceSN;                   //设备序列号
	int							m_nDeviceIndex;                  //设备序号
	string						m_strSavePath;					 //图像保存路径
	string						m_strFileName;					 //保存图像的名称
    cv::Mat m_latestMat;  // 缓存最新的cv::Mat
    QMutex m_matMutex;    // 保护m_latestMat的互斥锁

};

