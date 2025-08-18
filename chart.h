#pragma once

#include <QMainWindow>
#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>
#include <QAction>
#include <QLabel>
#include <QGraphicsLineItem>
#include <vector>
#include <utility>
#include <string>


class TraceChartView : public QtCharts::QChartView {
    Q_OBJECT
public:
    explicit TraceChartView(QtCharts::QChart* chart, QWidget* parent = nullptr);

protected:
    void mousePressEvent(QMouseEvent* ev) override;
    void mouseMoveEvent(QMouseEvent* ev) override;
    void mouseDoubleClickEvent(QMouseEvent* ev) override;

signals:
    void coordinatesChanged(const QPointF& p);
    void deltaValuesChanged(const QPointF& p1, const QPointF& p2);

private:
    void handleContext(const QPoint& pos);
    void updateCursorPosition(int cursorId, const QPointF& scenePos);

    
    QGraphicsLineItem* m_cursor1H = nullptr; 
    QGraphicsLineItem* m_cursor1V = nullptr; 
    QGraphicsLineItem* m_cursor2H = nullptr; 
    QGraphicsLineItem* m_cursor2V = nullptr; 

    bool m_cursor1Active = false;
    bool m_cursor2Active = false;
    bool m_isDragging = false;
    int m_draggingCursorId = 0; 
};


struct SeriesIn {
    QString name;
    std::vector<std::pair<float, float>> samples; 
    bool isCurrent = false;                      
    QString colorCss;                            
};

class ChartWindow : public QMainWindow {
    Q_OBJECT
public:
    
    explicit ChartWindow(const QString& title,
        const QString& yLabel,
        const std::vector<std::pair<float, float>>& samples,
        QWidget* parent = nullptr);

    
    explicit ChartWindow(const QString& title,
        const std::vector<SeriesIn>& seriesList,
        QWidget* parent = nullptr);

private slots:
    void onMathActionTriggered();

private:
    
    QtCharts::QChart* m_chart = nullptr;
    QtCharts::QValueAxis* m_axisX = nullptr;
    QtCharts::QValueAxis* m_axisYLeft = nullptr;
    QtCharts::QValueAxis* m_axisYRight = nullptr;
    std::vector<QtCharts::QLineSeries*> m_leftSeries;
    std::vector<QtCharts::QLineSeries*> m_rightSeries;

    bool m_autoZoom = true;
    QAction* m_actAuto = nullptr;
    QLabel* m_coordsLabel = nullptr;

    void recomputeAndApplyRanges();
    void switchToManual();
    void scaleX(double factor);
    void scaleY(double factor);
    void processMathFormula(const QString& formula);
};