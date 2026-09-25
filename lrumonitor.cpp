#include "lrumonitor.h"
#include "ui_lrumonitor.h"
#include "QDir"

lrumonitor::lrumonitor(QWidget *parent):
    QMainWindow(parent),
    ui(new Ui::lrumonitor)
{
    ui->setupUi(this);

    // 设置格式
    m_lru = new LRU(ui->robot3D_virtual);
    QSurfaceFormat format;
    format.setDepthBufferSize(24);      // 深度缓冲区
    format.setStencilBufferSize(8);     // 模板缓冲区
    format.setVersion(3, 3);            // OpenGL版本
    format.setProfile(QSurfaceFormat::CoreProfile); // Core Profile
    format.setSamples(4);               // 4x多重采样抗锯齿
    m_lru->setFormat(format);
    m_lru->setGeometry(0, 0, 800, 700);
    m_lru->show();


    // 初始化STL文件列表
    QString stlDir = QCoreApplication::applicationDirPath() + "/STL/";//从.exe文件所在目录下寻找STL文件夹
    QDir directory(stlDir);
    QStringList stlFiles = directory.entryList(QStringList() << "*.stl", QDir::Files);
    ui->comboBox_STLFiles->clear();
    ui->comboBox_STLFiles->addItems(stlFiles);
}

lrumonitor::~lrumonitor()
{
    delete ui;
}


//LRU数字孪生
//加载STL模型
void lrumonitor::on_btn_LoadLRU_clicked()
{
    // 获取选择的文件名
    QString selectedFile = ui->comboBox_STLFiles->currentText();
    if (selectedFile.isEmpty()) {
        // UpdateUI("未选择STL文件");
        return;
    }

    // 构建完整路径
    QString filePath = QCoreApplication::applicationDirPath()
                       + "/STL/"
                       + selectedFile;

    // 验证文件是否存在
    if (!QFile::exists(filePath)) {
        // UpdateUI("文件不存在 - " + filePath);
        return;
    }

    // 加载模型
    if (m_lru->loadSTLModel(filePath)) {
        // UpdateUI("模型加载成功: " + filePath);
    } else {
        // UpdateUI("模型加载失败: " + filePath);
    }

}


//实时监测模型位姿
void lrumonitor::on_btn_MonitorStart_clicked()
{
    QTimer *timer = new QTimer(this);

    if(ui->btn_MonitorStart->text()=="监测LRU姿态")
    {
        ui->btn_MonitorStart->setText("停止监测");
        connect(timer, &QTimer::timeout, this, [=]() {
            float motorXPosition = m_GapWidth;
            float motorYPosition = m_AxisPos;
            float motorZPosition = m_GapWidth1;
            float rotationAngleY = m_GapAngle;

            ui->lineEdit_LRUMoveX->setText(QString::number(motorXPosition));
            ui->lineEdit_LRUMoveZ->setText(QString::number(motorZPosition));
            ui->lineEdit_LRUMoveY->setText(QString::number(motorYPosition));
            ui->lineEdit_LRURotY->setText(QString::number(rotationAngleY));

            m_lru->updateModelPosition(motorXPosition, motorYPosition, motorZPosition, rotationAngleY);
        });
    }
    else
    {
        ui->btn_MonitorStart->setText("监测LRU姿态");
        timer->stop();
    }
    timer->start(200);
}


void lrumonitor::UpdateLRU(int mytype)
{
    ui->comboBox_STLFiles->setCurrentIndex(mytype);
    on_btn_LoadLRU_clicked();
    on_btn_MonitorStart_clicked();
}




