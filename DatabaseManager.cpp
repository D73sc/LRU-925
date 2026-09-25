#include "DatabaseManager.h"
#include <QMessageBox>
#include <QChart>
#include <QLineSeries>
#include <QBarSeries>
#include <QBarSet>
#include <QValueAxis>
#include <QBarCategoryAxis>
#include <QPainter>
#include <QFont>
#include <QtMath>


// 获取各系统完成任务的平均运行时间
QMap<QString, double> DatabaseManager::getSystemAverageDuration()
{
    QMap<QString, double> systemAvgDuration;
    if (!m_db.isOpen()) return systemAvgDuration;

    // 查询所有状态为"完成"的任务
    QSqlQuery query(m_db);
    QString queryStr = QString(
        "SELECT 系统名称, AVG(运行时间) as 平均时间 "
        "FROM task_status "
        "WHERE 任务状态 = :task_status "
        "GROUP BY 系统名称 "
        "ORDER BY 平均时间 DESC"
        );

    query.prepare(queryStr);
    query.bindValue(":task_status", TaskStateToString(eTaskstate::Complete));

    if (query.exec()) {
        while (query.next()) {
            QString systemName = query.value(0).toString();
            double avgDuration = query.value(1).toDouble();
            systemAvgDuration[systemName] = avgDuration;
        }
    } else {
        qWarning() << "查询平均运行时间失败：" << query.lastError().text();
    }

    return systemAvgDuration;
}


// 数据可视化-柱状图
void DatabaseManager::generateAverageDurationChart(QChartView* chartView, QWidget* parentWidget)
{
    QMap<QString, double> systemData = getSystemAverageDuration();
    if (systemData.isEmpty()) {
        QMessageBox::information(parentWidget, "提示", "暂无完成状态的任务数据");
        return;
    }

    // 创建图表
    QChart* chart = new QChart();
    chart->setTitle("系统-平均执行时间");
    chart->legend()->setVisible(true);
    chart->legend()->setAlignment(Qt::AlignBottom);
    chart->setTheme(QChart::ChartThemeLight);

    // 创建柱状系列
    QBarSeries* series = new QBarSeries();

    // 柱状图颜色
    QList<QColor> barColors = {
        Qt::blue, Qt::green, Qt::red, Qt::cyan,
        Qt::magenta, Qt::yellow, Qt::gray, Qt::darkRed
    };
    int colorIndex = 0;


    for (auto it = systemData.begin(); it != systemData.end(); ++it) {
        QBarSet* set = new QBarSet(it.key());
        *set << it.value();
        set->setColor(barColors[colorIndex % barColors.size()]);
        series->append(set);
        colorIndex++;
    }

    chart->addSeries(series);

    // 配置坐标轴
    QBarCategoryAxis* axisX = new QBarCategoryAxis();
    axisX->setTitleText("系统名称");
    axisX->setTitleFont(QFont("Microsoft YaHei", 10));

    QValueAxis* axisY = new QValueAxis();
    axisY->setTitleText("平均运行时间(s)");
    axisY->setTitleFont(QFont("Microsoft YaHei", 10));
    axisY->setMinorTickCount(5);
    QList<double> durations = systemData.values();
    double maxDuration = *std::max_element(durations.begin(), durations.end());
    axisY->setRange(0, maxDuration * 1.1);

    chart->addAxis(axisX, Qt::AlignBottom);
    chart->addAxis(axisY, Qt::AlignLeft);
    series->attachAxis(axisX);
    series->attachAxis(axisY);

    // 显示图表
    chartView->setRenderHint(QPainter::Antialiasing);
    chartView->setChart(chart);
    chartView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    chart->setParent(chartView);
    chartView->repaint();
}


//数据可视化-折线图
void DatabaseManager::generateSensorChart(QChartView *chartView, const QString &selectedMainType, QWidget *parentWidget)
{
    if (selectedMainType.isEmpty()) {
        QMessageBox::warning(parentWidget, "提示", "请选择传感器类型");
        return;
    }

    // 创建图表
    QChart* chart = new QChart();
    chart->setTitle(QString("%1").arg(selectedMainType));
    chart->legend()->setVisible(true);//启用图例,标明折线
    chart->legend()->setAlignment(Qt::AlignBottom);
    chart->setTheme(QChart::ChartThemeLight);

    QStringList subTypes = getSubSensorTypes(selectedMainType);
    if (subTypes.isEmpty()) {
        QMessageBox::warning(parentWidget, "提示", "该传感器类型无对应子类型");
        delete chart;
        return;
    }

    // 折线颜色
    QList<QColor> lineColors = {
        Qt::green, Qt::blue, Qt::red, Qt::darkYellow,
        Qt::magenta, Qt::cyan, Qt::darkGray, Qt::darkRed
    };
    int colorIndex = 0;

    // 循环每个子类型
    for (const QString& subType : subTypes) {
        QList<QPair<QDateTime, double>> dataPoints = this->getSensorTimeSeriesData("sensor_data", subType, 30);

        if (dataPoints.isEmpty()) {
            qWarning() << "子类型" << subType << "暂无数据";
            continue;
        }

        // 创建折线系列
        QLineSeries* lineSeries = new QLineSeries();
        lineSeries->setName(subType);  // 绑定折线名称,对应图例显示
        lineSeries->setColor(lineColors[colorIndex % lineColors.size()]);
        lineSeries->setPointsVisible(false);  //不显示数据点
        colorIndex++;

        int pointOrder = 1;
        for (const auto& point : dataPoints) {
            lineSeries->append(pointOrder, point.second);
            pointOrder++;
        }

        // 将折线添加到图表
        chart->addSeries(lineSeries);
    }

    if (chart->series().isEmpty()) {
        QMessageBox::information(parentWidget, "提示", "该传感器类型暂无有效数据");
        delete chart;
        return;
    }

    // 配置坐标轴
    // 配置X轴（替换原X轴代码）
    QValueAxis* axisX = new QValueAxis();
    axisX->setTitleText("数据点次序");
    axisX->setTitleFont(QFont("Microsoft YaHei", 10));


    int maxPointCount = 0;
    for (QAbstractSeries* s : chart->series()) {
        if (QLineSeries* ls = qobject_cast<QLineSeries*>(s)) {
            maxPointCount = qMax(maxPointCount, ls->count());
        }
    }

    // 刻度数量设置为数据点最大数量
    axisX->setTickCount(maxPointCount);
    axisX->setMinorTickCount(0);  // 不显示小刻度
    // 通过设置刻度间隔为1，确保每个整数位置只显示一个标签
    axisX->setTickInterval(1);
    axisX->setRange(1, maxPointCount);  // 动态设置X轴范围
    axisX->setLabelFormat("%d");        //标签格式为整数
    chart->addAxis(axisX, Qt::AlignBottom);

    QValueAxis* axisY = new QValueAxis();
    axisY->setTitleText("传感器数值");
    axisY->setTitleFont(QFont("Microsoft YaHei", 10));
    axisY->setMinorTickCount(5);  // 显示小刻度

    //Y轴自动适配大小
    double yMin = 1e18;
    double yMax = -1e18;

    // 遍历所有折线系列
    for (QAbstractSeries* series : chart->series()) {
        QLineSeries* lineSeries = qobject_cast<QLineSeries*>(series);
        if (!lineSeries) continue;

        // 遍历当前折线的所有数据点
        for (const QPointF& point : lineSeries->points()) {
            double yValue = point.y();
            yMin = qMin(yMin, yValue);
            yMax = qMax(yMax, yValue);
        }
    }

    if (qFuzzyCompare(yMin, yMax)) {
        yMin -= 1.0;
        yMax += 1.0;
    }

    axisY->setRange(yMin, yMax);
    chart->addAxis(axisY, Qt::AlignLeft);

    // 将所有折线关联到坐标轴
    for (QAbstractSeries* series : chart->series()) {
        series->attachAxis(axisX);
        series->attachAxis(axisY);
    }

    //显示图表
    chartView->setRenderHint(QPainter::Antialiasing);  // 抗锯齿，圆滑曲线
    chartView->setChart(chart);  // 关联图表
    chartView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    chart->setParent(chartView); // 确保关闭窗口时，chart 能被自动释放
    chartView->repaint();
}

