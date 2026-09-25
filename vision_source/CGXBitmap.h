#pragma once

#include <QWidget>
#include "ui_CGXBitmap.h"
#include <iostream>
#include <string>
#include <map>
#include <QSplitter>
#include <QHBoxLayout>
#include <QPushButton>
#include <QComboBox>
#include <QLineEdit>
#include <QCheckBox>
#include <QCloseEvent>
#include <QImage>
#include <QPixmap>
#include <QLabel>
#include <GalaxyIncludes.h>
#include <QCloseEvent>
#include <QTimer>
#include <opencv2/opencv.hpp>
#include <QMutex>
#include <QDebug>
#include "vision_source/imageviewer.h"


using namespace std;
class CGXBitmap : public QWidget
{
	Q_OBJECT

public:
	CGXBitmap(QWidget *parent = Q_NULLPTR);
	~CGXBitmap();
    void ClearDisplay();

private:
	Ui::CGXBitmap ui;
	bool               m_bIsColor;                       //是否支持彩色相机
	int64_t            m_nImageHeight;                   //原始图像高
	int64_t            m_nImageWidth;                    //原始图像宽
	BITMAPINFO         *m_pBmpInfo;	                     //BITMAPINFO 结构指针，显示图像时使用
	char               m_chBmpBuf[2048];	             //BIMTAPINFO 存储缓冲区，m_pBmpInfo即指向此缓冲区
	uchar              *m_pImageBuffer;                  //保存翻转后的图像用于显示
private:
	CGXBitmap& operator=(const CGXBitmap&);
	CGXBitmap(const CGXBitmap&);

public:

    // QLabel			   *m_pLabel;
	QImage			   *m_pImage;
    ImageViewer        *m_pLabel;
    // QLabel			   *m_pFps;
	//显示图像
    cv::Mat Show(CImageDataPointer& objCImageDataPointer);


	//通过GX_PIXEL_FORMAT_ENTRY获取最优Bit位
	GX_VALID_BIT_LIST GetBestValudBit(GX_PIXEL_FORMAT_ENTRY emPixelFormatEntry);

	LPCWSTR stringToLPCWSTR(std::string orig);

	void addDevicePtr(CGXDevicePointer& objCGXDevicePointer);

private:
	//判断PixelFormat是否为8位
	bool __IsPixelFormat8(GX_PIXEL_FORMAT_ENTRY emPixelFormatEntry);

	//为彩色相机图像显示准备资源
	void __ColorPrepareForShowImg();

	//为黑白相机图像显示准备资源
	void __MonoPrepareForShowImg();

	//判断是否兼容
	bool __IsCompatible(BITMAPINFO *pBmpInfo, uint64_t nWidth, uint64_t nHeight);

	//更新Bitmap的信息
	void __UpdateBitmap(CImageDataPointer& objCImageDataPointer);

	//将m_pBufferRGB中图像显示到界面
	void __DrawImg(uchar* pBuffer);

	//将m_pBufferRGB中图像和帧率显示到界面
	void __DrawImg(uchar* pBuffer, char* strDeviceSNFPS);
	//计算宽度所占的字节数
	int64_t __GetStride(int64_t nWidth, bool bIsColor);

	//是否支持彩色
	void __IsSupportColor(CGXDevicePointer& objCGXDevicePointer, bool &bIsColorFilter);

};
