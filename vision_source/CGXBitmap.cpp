#include "CGXBitmap.h"
//---------------------------------------------------------------------------------
/**
\brief   构造函数
\param   objCGXDevicePointer 图像设备指针
\param   parent 窗体指针
\return  无
*/
//----------------------------------------------------------------------------------
CGXBitmap::CGXBitmap(QWidget *parent)
	: QWidget(parent)
	, m_bIsColor(false)
	, m_nImageHeight(0)
	, m_nImageWidth(0)
	, m_pImage(NULL)
	, m_pBmpInfo(NULL)
	, m_pImageBuffer(NULL)
{
	ui.setupUi(this);
	//初始化绘图框
    m_pLabel = new ImageViewer(this);
    m_pLabel->move(0, 0);
    m_pLabel->resize(864, 648);

    // m_pFps = new QLabel(this);
    // m_pFps->move(0, 0);
    // m_pFps->resize(300, 30);
    // m_pFps->setStyleSheet("color:red;");

}

void CGXBitmap::addDevicePtr(CGXDevicePointer& objCGXDevicePointer)
{
	if (objCGXDevicePointer.IsNull())
	{
		throw std::runtime_error("Argument is error");
	}

	memset(m_chBmpBuf, 0, sizeof(m_chBmpBuf));
	gxstring strValue = "";

	//获得图像宽度、高度等

	m_nImageWidth = (int64_t)objCGXDevicePointer->GetRemoteFeatureControl()->GetIntFeature("Width")->GetValue();
	m_nImageHeight = (int64_t)objCGXDevicePointer->GetRemoteFeatureControl()->GetIntFeature("Height")->GetValue();

	//获取是否为彩色相机
	__IsSupportColor(objCGXDevicePointer, m_bIsColor);

	if (m_bIsColor)
	{
		__ColorPrepareForShowImg();
	}
	else
	{
		__MonoPrepareForShowImg();
	}
}
//---------------------------------------------------------------------------------
/**
\brief   析构函数

\return  无
*/
//----------------------------------------------------------------------------------
CGXBitmap::~CGXBitmap()
{

}

//----------------------------------------------------------------------------------
/**
\brief     判断PixelFormat是否为8位
\param     emPixelFormatEntry 图像数据格式
\return    true为8为数据，false为非8位数据
*/
//----------------------------------------------------------------------------------
bool CGXBitmap::__IsPixelFormat8(GX_PIXEL_FORMAT_ENTRY emPixelFormatEntry)
{
	bool bIsPixelFormat8 = false;
	const unsigned  PIXEL_FORMATE_BIT = 0x00FF0000;  ///<用于与当前的数据格式进行与运算得到当前的数据位数
	unsigned uiPixelFormatEntry = (unsigned)emPixelFormatEntry;
	if ((uiPixelFormatEntry & PIXEL_FORMATE_BIT) == GX_PIXEL_8BIT)
	{
		bIsPixelFormat8 = true;
	}
	return bIsPixelFormat8;
}

//----------------------------------------------------------------------------------
/**
\brief     通过GX_PIXEL_FORMAT_ENTRY获取最优Bit位
\param     emPixelFormatEntry 图像数据格式
\return    最优Bit位
*/
//----------------------------------------------------------------------------------
GX_VALID_BIT_LIST CGXBitmap::GetBestValudBit(GX_PIXEL_FORMAT_ENTRY emPixelFormatEntry)
{
	GX_VALID_BIT_LIST emValidBits = GX_BIT_0_7;
	switch (emPixelFormatEntry)
	{
	case GX_PIXEL_FORMAT_MONO8:
	case GX_PIXEL_FORMAT_BAYER_GR8:
	case GX_PIXEL_FORMAT_BAYER_RG8:
	case GX_PIXEL_FORMAT_BAYER_GB8:
	case GX_PIXEL_FORMAT_BAYER_BG8:
	{
		emValidBits = GX_BIT_0_7;
		break;
	}
	case GX_PIXEL_FORMAT_MONO10:
	case GX_PIXEL_FORMAT_BAYER_GR10:
	case GX_PIXEL_FORMAT_BAYER_RG10:
	case GX_PIXEL_FORMAT_BAYER_GB10:
	case GX_PIXEL_FORMAT_BAYER_BG10:
	{
		emValidBits = GX_BIT_2_9;
		break;
	}
	case GX_PIXEL_FORMAT_MONO12:
	case GX_PIXEL_FORMAT_BAYER_GR12:
	case GX_PIXEL_FORMAT_BAYER_RG12:
	case GX_PIXEL_FORMAT_BAYER_GB12:
	case GX_PIXEL_FORMAT_BAYER_BG12:
	{
		emValidBits = GX_BIT_4_11;
		break;
	}
	case GX_PIXEL_FORMAT_MONO14:
	{
		//暂时没有这样的数据格式待升级
		break;
	}
	case GX_PIXEL_FORMAT_MONO16:
	case GX_PIXEL_FORMAT_BAYER_GR16:
	case GX_PIXEL_FORMAT_BAYER_RG16:
	case GX_PIXEL_FORMAT_BAYER_GB16:
	case GX_PIXEL_FORMAT_BAYER_BG16:
	{
		//暂时没有这样的数据格式待升级
		break;
	}
	default:
		break;
	}
	return emValidBits;
}

//---------------------------------------------------------------------------------
/**
\brief   为彩色相机图像显示准备资源

\return  无
*/
//----------------------------------------------------------------------------------
void CGXBitmap::__ColorPrepareForShowImg()
{
	//--------------------------------------------------------------------
	//---------------------------初始化bitmap头---------------------------
	m_pBmpInfo = (BITMAPINFO *)m_chBmpBuf;
	m_pBmpInfo->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	m_pBmpInfo->bmiHeader.biWidth = (LONG)m_nImageWidth;
	m_pBmpInfo->bmiHeader.biHeight = (LONG)m_nImageHeight;

	m_pBmpInfo->bmiHeader.biPlanes = 1;
	m_pBmpInfo->bmiHeader.biBitCount = 24;
	m_pBmpInfo->bmiHeader.biCompression = BI_RGB;
	m_pBmpInfo->bmiHeader.biSizeImage = 0;
	m_pBmpInfo->bmiHeader.biXPelsPerMeter = 0;
	m_pBmpInfo->bmiHeader.biYPelsPerMeter = 0;
	m_pBmpInfo->bmiHeader.biClrUsed = 0;
	m_pBmpInfo->bmiHeader.biClrImportant = 0;

	// 根据像素格式决定位深度和是否需要调色板
		// 彩色图像
		m_pBmpInfo->bmiHeader.biBitCount = 24; // RGB24
		// 分配RGB转换后的缓存
		if (m_pImageBuffer != NULL)
		{
			delete[] m_pImageBuffer;
			m_pImageBuffer = NULL;
		}
		m_pImageBuffer = new BYTE[(size_t)(m_nImageWidth * m_nImageHeight * 3)];


	if (m_pImageBuffer == NULL)
	{
        qDebug() << "Failed to allocate memory";
	}
}

//---------------------------------------------------------------------------------
/**
\brief   为黑白相机图像显示准备资源

\return  无
*/
//----------------------------------------------------------------------------------
void CGXBitmap::__MonoPrepareForShowImg()
{
	//---------------------------------------------------------------------
	//----------------------初始化bitmap头---------------------------------
	m_pBmpInfo = (BITMAPINFO *)m_chBmpBuf;
	m_pBmpInfo->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	m_pBmpInfo->bmiHeader.biWidth = (LONG)m_nImageWidth;
	m_pBmpInfo->bmiHeader.biHeight = (LONG)m_nImageHeight;

	m_pBmpInfo->bmiHeader.biPlanes = 1;
	m_pBmpInfo->bmiHeader.biBitCount = 8; // 黑白图像为8
	m_pBmpInfo->bmiHeader.biCompression = BI_RGB;
	m_pBmpInfo->bmiHeader.biSizeImage = 0;
	m_pBmpInfo->bmiHeader.biXPelsPerMeter = 0;
	m_pBmpInfo->bmiHeader.biYPelsPerMeter = 0;
	m_pBmpInfo->bmiHeader.biClrUsed = 0;
	m_pBmpInfo->bmiHeader.biClrImportant = 0;

	// 黑白图像需要初始化调色板
	for (int i = 0; i < 256; i++)
	{
		m_pBmpInfo->bmiColors[i].rgbBlue = i;
		m_pBmpInfo->bmiColors[i].rgbGreen = i;
		m_pBmpInfo->bmiColors[i].rgbRed = i;
		m_pBmpInfo->bmiColors[i].rgbReserved = 0;
	}

	//为经过翻转后的图像数据分配空间
	if (m_pImageBuffer != NULL)
	{
		delete m_pImageBuffer;
		m_pImageBuffer = NULL;
	}

	m_pImageBuffer = new BYTE[(size_t)(m_nImageWidth * m_nImageHeight)];
	if (m_pImageBuffer == NULL)
	{
		throw std::runtime_error("Fail to allocate memory");
	}
}

//----------------------------------------------------------------------------------
/**
\brief     判断是否兼容
\param     pBmpInfo BITMAPINFO指针
\param     nWidth 图像宽
\param     nHeight 图像高
\return    true为一样，false不一样
*/
//----------------------------------------------------------------------------------
bool CGXBitmap::__IsCompatible(BITMAPINFO *pBmpInfo, uint64_t nWidth, uint64_t nHeight)
{
	if (pBmpInfo == NULL
		|| pBmpInfo->bmiHeader.biHeight != nHeight
		|| pBmpInfo->bmiHeader.biWidth != nWidth
		)
	{
		return false;
	}
	return true;
}

//----------------------------------------------------------------------------------
/**
\brief     检查图像是否改变并更新Buffer并为图像显示准备资源
\param     objCImageDataPointer  图像数据对象
\return    无
*/
//----------------------------------------------------------------------------------
void CGXBitmap::__UpdateBitmap(CImageDataPointer& objCImageDataPointer)
{
	if (!__IsCompatible(m_pBmpInfo, objCImageDataPointer->GetWidth(), objCImageDataPointer->GetHeight()))
	{
		m_nImageWidth = objCImageDataPointer->GetWidth();
		m_nImageHeight = objCImageDataPointer->GetHeight();
		if (m_bIsColor)
		{
			__ColorPrepareForShowImg();
		}
		else
		{
			__MonoPrepareForShowImg();
		}
	}
}

//---------------------------------------------------------------------------------
/**
\brief   将m_pBufferRGB中图像显示到界面
\param   pBuffer  图像数据Buffer指针
\return  无
*/
// ----------------------------------------------------------------------------------
void CGXBitmap::__DrawImg(uchar* pBuffer)
{
    // 释放旧图像
    // if (m_pImage != NULL)
    // {
    //     delete m_pImage;
    //     m_pImage = NULL;
    // }

    // 1. 确保使用RGB888格式（彩色图像专用）
    // QImage::Format qFormat = QImage::Format_RGB888;

    // // 2. 处理彩色图像数据（从BGR转换为RGB并深拷贝）
    // cv::Mat bgrMat(m_nImageHeight, m_nImageWidth, CV_8UC3, pBuffer);

    // // 3. 同步Show函数中的垂直翻转
    // // cv::Mat flippedMat;
    // // cv::flip(bgrMat, flippedMat, 0);

    // // 4. 转换BGR→RGB（Qt显示需要RGB顺序）
    // cv::Mat rgbMat;
    // cv::cvtColor(bgrMat, rgbMat, cv::COLOR_BGR2RGB);
    // cv::flip(rgbMat, rgbMat, 0); // 垂直翻转

    // // 5. 创建QImage并强制深拷贝数据（确保内存安全）
    // QImage tempImage(
    //     rgbMat.data,
    //     rgbMat.cols,
    //     rgbMat.rows,
    //     rgbMat.step,  // 关键：传递正确的行字节数，避免条纹
    //     qFormat
    //     );
    // tempImage = tempImage.copy(); // 深拷贝，脱离对rgbMat的依赖
    QImage tempImage(
        pBuffer, m_nImageWidth, m_nImageHeight,
        m_nImageWidth * 3,  // 行字节数 = 宽度 * 3（RGB888每个像素3字节）
        QImage::Format_RGB888
        );

    QPixmap pixmap = QPixmap::fromImage(tempImage);
    // if (m_pLabel) {
    //     // 使用QueuedConnection确保在主线程异步执行
    //     QMetaObject::invokeMethod(
    //         m_pLabel,
    //         "setPixmap",
    //         Qt::QueuedConnection,
    //         Q_ARG(QPixmap, pixmap)
    //         );
    // }
    QMetaObject::invokeMethod(
        m_pLabel,
        "setPixmap",
        Qt::QueuedConnection,
        Q_ARG(QPixmap, pixmap)
        );

    // 6. 保存深拷贝后的图像
    // m_pImage = new QImage(tempImage);
}

/**
\brief     计算宽度所占的字节数
\param     nWidth  图像宽度
\param     bIsColor  是否是彩色相机
\return    图像一行所占的字节数
*/
//----------------------------------------------------------------------------------
int64_t CGXBitmap::__GetStride(int64_t nWidth, bool bIsColor)
{
	return bIsColor ? nWidth * 3 : nWidth;
}

//----------------------------------------------------------------------------------
/**
\brief     用于显示图像
\param     objCImageDataPointer  图像数据对象
\return    无
*/
//----------------------------------------------------------------------------------


cv::Mat CGXBitmap::Show(CImageDataPointer& objCImageDataPointer)
{
    GX_VALID_BIT_LIST emValidBits = GX_BIT_0_7;
    uchar* pBuffer = NULL;

    if (objCImageDataPointer.IsNull())
    {
        qDebug() << "NULL pointer dereferenced";
        return cv::Mat(); // 这里也要加return，否则会继续执行
    }

    __UpdateBitmap(objCImageDataPointer);

    emValidBits = GetBestValudBit(objCImageDataPointer->GetPixelFormat());

    // 1. SDK内部分配内存
    pBuffer = (uchar*)objCImageDataPointer->ConvertToRGB24(emValidBits, GX_RAW2RGB_NEIGHBOUR, true);

    // 【重要】如果转换失败，直接返回，不要释放
    if (pBuffer == NULL)
    {
        qWarning() << "ConvertToRGB24 failed, no memory allocated";
        return cv::Mat();
    }

    // 2. 所有使用pBuffer的代码都放在这里
    QImage tempImage(
        pBuffer, m_nImageWidth, m_nImageHeight,
        m_nImageWidth * 3,
        QImage::Format_RGB888
        );
    tempImage = tempImage.mirrored(false,true);
    QPixmap pixmap = QPixmap::fromImage(tempImage); // 深拷贝完成，不再依赖pBuffer
    QMetaObject::invokeMethod(
        m_pLabel,
        "setPixmap",
        Qt::QueuedConnection,
        Q_ARG(QPixmap, pixmap)
        );

    cv::Mat image = cv::Mat(m_nImageHeight, m_nImageWidth, CV_8UC3, pBuffer);
    cv::cvtColor(image, image, cv::COLOR_RGB2GRAY);
    cv::flip(image, image, 0); // 深拷贝完成，不再依赖pBuffer


    pBuffer = NULL; // 置空防止野指针

    // 3. 释放临时对象（可选，但建议添加）
    // image.release();
    tempImage = QImage();

    // 4. 返回深拷贝的flipimage，完全不依赖pBuffer
    return image;
}

//----------------------------------------------------------------------------------
/**
\brief     是否支持彩色
\param     objCGXDevicePointer  [in]    设备句柄
\param     bIsColorFilter       [out]   是否支持彩色

\return
*/
//----------------------------------------------------------------------------------
void CGXBitmap::__IsSupportColor(CGXDevicePointer& objCGXDevicePointer, bool &bIsColorFilter)
{
    // GX_STATUS emStatus = GX_STATUS_SUCCESS;
	bool      bIsImplemented = false;
	bool      bIsMono = false;
	gxstring  strPixelFormat = "";

	strPixelFormat = objCGXDevicePointer->GetRemoteFeatureControl()->GetEnumFeature("PixelFormat")->GetValue();
	strPixelFormat.substr(0, 4);

	if (0 == memcmp(strPixelFormat.c_str(), "Mono", 4))
	{
		bIsMono = true;
	}
	else
	{
		bIsMono = false;
	}

	bIsImplemented = objCGXDevicePointer->GetRemoteFeatureControl()->IsImplemented("PixelColorFilter");

	// 若当前为非黑白且支持PixelColorFilter则为彩色
	if ((!bIsMono) && (bIsImplemented))
	{
		bIsColorFilter = true;
	}
	else
	{
		bIsColorFilter = false;
	}
}

LPCWSTR CGXBitmap::stringToLPCWSTR(std::string orig)
{
	size_t origsize = orig.length() + 1;
	const size_t newsize = 100;
	size_t convertedChars = 0;
	wchar_t* wcstring = (wchar_t*)malloc(sizeof(wchar_t) * (orig.length() - 1));
	mbstowcs_s(&convertedChars, wcstring, origsize, orig.c_str(), _TRUNCATE);

	return wcstring;
}

// 新增：清空显示框为白色
void CGXBitmap::ClearDisplay()
{
    if (m_pLabel) {
        // 创建与显示框大小一致的白色Pixmap
        QPixmap whitePixmap(m_pLabel->size());
        whitePixmap.fill(Qt::white);

        // 使用QueuedConnection确保在主线程更新UI
        QMetaObject::invokeMethod(
            m_pLabel,
            "setPixmap",
            Qt::QueuedConnection,
            Q_ARG(QPixmap, whitePixmap)
            );
    }
}
