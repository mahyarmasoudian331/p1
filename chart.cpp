#include "chart.h"
#include <QPainter>
#include <QToolBar>
#include <QAction>
#include <QtCharts/QLegendMarker>
#include <QColor>
#include <QMouseEvent>
#include <QMenu>
#include <QColorDialog>
#include <QInputDialog>
#include <QStatusBar>
#include <QFont>
#include <limits>
#include <cmath>
#include <QGraphicsScene>
#include <QMessageBox>
#include <QRegularExpression>
#include <QRegularExpressionMatchIterator>

using namespace QtCharts;


static double distPointToSegment(const QPointF& p, const QPointF& a, const QPointF& b)
{
    const double vx = b.x() - a.x();
    const double vy = b.y() - a.y();
    const double wx = p.x() - a.x();
    const double wy = p.y() - a.y();

    const double c1 = vx * wx + vy * wy;
    if (c1 <= 0.0) return std::hypot(p.x() - a.x(), p.y() - a.y());

    const double c2 = vx * vx + vy * vy;
    if (c2 <= 0.0) return std::hypot(p.x() - a.x(), p.y() - a.y());

    double t = c1 / c2;
    if (t >= 1.0) return std::hypot(p.x() - b.x(), p.y() - b.y());

    QPointF proj(a.x() + t * vx, a.y() + t * vy);
    return std::hypot(p.x() - proj.x(), p.y() - proj.y());
}


TraceChartView::TraceChartView(QtCharts::QChart* chart, QWidget* parent)
    : QtCharts::QChartView(chart, parent) {
    setRenderHint(QPainter::Antialiasing);
    setRubberBand(QChartView::RectangleRubberBand);
    setMouseTracking(true);

    
    m_cursor1V = new QGraphicsLineItem(this->rect().x(), this->rect().y(), this->rect().x(), this->rect().height());
    m_cursor1V->setPen(QPen(Qt::green, 1, Qt::DashLine));
    m_cursor1V->setZValue(10);
    m_cursor1V->hide();
    scene()->addItem(m_cursor1V);

    m_cursor1H = new QGraphicsLineItem(this->rect().x(), this->rect().y(), this->rect().width(), this->rect().y());
    m_cursor1H->setPen(QPen(Qt::green, 1, Qt::DashLine));
    m_cursor1H->setZValue(10);
    m_cursor1H->hide();
    scene()->addItem(m_cursor1H);

    m_cursor2V = new QGraphicsLineItem(this->rect().x(), this->rect().y(), this->rect().x(), this->rect().height());
    m_cursor2V->setPen(QPen(Qt::red, 1, Qt::DashLine));
    m_cursor2V->setZValue(10);
    m_cursor2V->hide();
    scene()->addItem(m_cursor2V);

    m_cursor2H = new QGraphicsLineItem(this->rect().x(), this->rect().y(), this->rect().width(), this->rect().y());
    m_cursor2H->setPen(QPen(Qt::red, 1, Qt::DashLine));
    m_cursor2H->setZValue(10);
    m_cursor2H->hide();
    scene()->addItem(m_cursor2H);
}

void TraceChartView::mousePressEvent(QMouseEvent* ev) {
    if (ev->button() == Qt::RightButton) {
        handleContext(ev->pos());
        return;
    }

    
    if (ev->button() == Qt::LeftButton) {
        double tolerance = 10.0;
        if (m_cursor1Active && std::hypot(ev->pos().x() - m_cursor1V->line().x1(), ev->pos().y() - m_cursor1H->line().y1()) < tolerance) {
            m_isDragging = true;
            m_draggingCursorId = 1;
        }
        else if (m_cursor2Active && std::hypot(ev->pos().x() - m_cursor2V->line().x1(), ev->pos().y() - m_cursor2H->line().y1()) < tolerance) {
            m_isDragging = true;
            m_draggingCursorId = 2;
        }
    }
    QChartView::mousePressEvent(ev);
}

void TraceChartView::mouseMoveEvent(QMouseEvent* ev) {
    QPointF p = chart()->mapToValue(ev->pos());
    emit coordinatesChanged(p);

    if (m_isDragging) {
        updateCursorPosition(m_draggingCursorId, ev->pos());

        if (m_cursor1Active && m_cursor2Active) {
            QPointF p1 = chart()->mapToValue(QPointF(m_cursor1V->line().x1(), m_cursor1H->line().y1()));
            QPointF p2 = chart()->mapToValue(QPointF(m_cursor2V->line().x1(), m_cursor2H->line().y1()));
            emit deltaValuesChanged(p1, p2);
        }
        ev->accept();
        return;
    }

    QChartView::mouseMoveEvent(ev);
}

void TraceChartView::mouseDoubleClickEvent(QMouseEvent* ev) {
    m_isDragging = false;
    QChartView::mouseDoubleClickEvent(ev);
}

void TraceChartView::updateCursorPosition(int cursorId, const QPointF& scenePos) {
    if (cursorId == 1) {
        m_cursor1V->setLine(scenePos.x(), this->rect().y(), scenePos.x(), this->rect().height());
        m_cursor1H->setLine(this->rect().x(), scenePos.y(), this->rect().width(), scenePos.y());
    }
    else if (cursorId == 2) {
        m_cursor2V->setLine(scenePos.x(), this->rect().y(), scenePos.x(), this->rect().height());
        m_cursor2H->setLine(this->rect().x(), scenePos.y(), this->rect().width(), scenePos.y());
    }
}


void TraceChartView::handleContext(const QPoint& pos) {
    QChart* ch = chart();
    if (!ch) return;

    const double PICK_TOL = 8.0;
    QLineSeries* best = nullptr;
    double bestDist = 1e100;

    for (QAbstractSeries* abs : ch->series()) {
        auto* ls = qobject_cast<QLineSeries*>(abs);
        if (!ls || !ls->isVisible()) continue;

        const auto pts = ls->pointsVector();
        if (pts.size() < 2) continue;

        for (int i = 1; i < pts.size(); ++i) {
            QPointF p1 = ch->mapToPosition(pts[i - 1], ls);
            QPointF p2 = ch->mapToPosition(pts[i], ls);
            double d = distPointToSegment(pos, p1, p2);
            if (d < bestDist) { bestDist = d; best = ls; }
        }
    }

    QMenu menu;
    QAction* actCursor1 = menu.addAction(m_cursor1Active ? "Remove Cursor 1" : "Add Cursor 1");
    QAction* actCursor2 = menu.addAction(m_cursor2Active ? "Remove Cursor 2" : "Add Cursor 2");

    if (best && bestDist <= PICK_TOL) {
        menu.addSeparator();
        menu.addAction("Change Color…");
        menu.addAction("Rename…");
    }

    QAction* chosen = menu.exec(mapToGlobal(pos));
    if (!chosen) return;

    if (chosen == actCursor1) {
        if (m_cursor1Active) {
            m_cursor1H->hide(); m_cursor1V->hide();
            m_cursor1Active = false;
        }
        else {
            updateCursorPosition(1, pos);
            m_cursor1H->show(); m_cursor1V->show();
            m_cursor1Active = true;
        }
    }
    else if (chosen == actCursor2) {
        if (m_cursor2Active) {
            m_cursor2H->hide(); m_cursor2V->hide();
            m_cursor2Active = false;
        }
        else {
            updateCursorPosition(2, pos);
            m_cursor2H->show(); m_cursor2V->show();
            m_cursor2Active = true;
        }
    }
    else {
        if (best && bestDist <= PICK_TOL) {
            if (chosen->text() == "Change Color…") {
                QColor start = best->pen().color();
                QColor c = QColorDialog::getColor(start, this, "Select Trace Color");
                if (c.isValid()) {
                    QPen pen = best->pen();
                    pen.setColor(c);
                    pen.setWidthF(2.0);
                    best->setPen(pen);
                }
            }
            else if (chosen->text() == "Rename…") {
                bool ok = false;
                QString newName = QInputDialog::getText(this, "Rename Trace",
                    "Label:", QLineEdit::Normal,
                    best->name(), &ok);
                if (ok && !newName.trimmed().isEmpty()) {
                    best->setName(newName.trimmed());
                }
            }
        }
    }

    if (m_cursor1Active && m_cursor2Active) {
        QPointF p1 = chart()->mapToValue(QPointF(m_cursor1V->line().x1(), m_cursor1H->line().y1()));
        QPointF p2 = chart()->mapToValue(QPointF(m_cursor2V->line().x1(), m_cursor2H->line().y1()));
        emit deltaValuesChanged(p1, p2);
    }
}


ChartWindow::ChartWindow(const QString& title,
    const QString& yLabel,
    const std::vector<std::pair<float, float>>& samples,
    QWidget* parent)
    : QMainWindow(parent)
{
    auto* series = new QLineSeries();
#if QT_VERSION >= QT_VERSION_CHECK(5, 8, 0)
    series->setUseOpenGL(true);
#endif
    for (const auto& p : samples) series->append(p.first, p.second);

    auto* chart = new QChart();
    chart->setTitle(title);
    chart->legend()->hide();

    auto* axisX = new QValueAxis(); axisX->setTitleText("Time (s)"); axisX->setTickCount(9);
    auto* axisY = new QValueAxis(); axisY->setTitleText(yLabel);     axisY->setTickCount(9);

    axisX->setLabelFormat("%.2f");
    axisY->setLabelFormat("%.2f");

    chart->addAxis(axisX, Qt::AlignBottom);
    chart->addAxis(axisY, Qt::AlignLeft);

    chart->addSeries(series);
    series->attachAxis(axisX);
    series->attachAxis(axisY);

    auto* view = new TraceChartView(chart);
    setCentralWidget(view);

    auto* tb = addToolBar("View");
    auto* actReset = tb->addAction("Reset Zoom");
    auto* actIn = tb->addAction("Zoom In");
    auto* actOut = tb->addAction("Zoom Out");

    connect(actReset, &QAction::triggered, chart, &QChart::zoomReset);
    connect(actIn, &QAction::triggered, this, [chart]() { chart->zoomIn(); });
    connect(actOut, &QAction::triggered, this, [chart]() { chart->zoomOut(); });

    setStatusBar(new QStatusBar(this));
    m_coordsLabel = new QLabel(this);

    QFont labelFont = m_coordsLabel->font();
    labelFont.setPointSize(12);
    m_coordsLabel->setFont(labelFont);

    statusBar()->addPermanentWidget(m_coordsLabel);

    
    auto formatValue = [](double val) -> QString {
        if (std::abs(val) > 100000 || (std::abs(val) < 1e-3 && val != 0.0)) {
            return QString::number(val, 'g', 3);
        }
        return QString::number(val, 'f', 2);
    };

    connect(view, &TraceChartView::coordinatesChanged, this, [this, formatValue](const QPointF& p) {
        m_coordsLabel->setText(QString("X = %1 s, Y = %2 V").arg(formatValue(p.x())).arg(formatValue(p.y())));
    });

    connect(view, &TraceChartView::deltaValuesChanged, this, [this, formatValue](const QPointF& p1, const QPointF& p2) {
        double dx = p2.x() - p1.x();
        double dy = p2.y() - p1.y();
        QString slopeText = (std::abs(dx) < 1e-9) ? "Inf" : formatValue(dy / dx);

        QString coordsText;
        if (m_axisYRight) { 
            coordsText = QString("C1: X=%1, YL=%2 | C2: X=%3, YL=%4 | dX=%5, dY=%6, Slope=%7")
                .arg(formatValue(p1.x()))
                .arg(formatValue(p1.y()))
                .arg(formatValue(p2.x()))
                .arg(formatValue(p2.y()))
                .arg(formatValue(dx))
                .arg(formatValue(dy))
                .arg(slopeText);
        }
        else {
            coordsText = QString("C1: X = %1, Y = %2 | C2 = %3, Y = %4 | dX = %5, dY = %6, Slope = %7")
                .arg(formatValue(p1.x()))
                .arg(formatValue(p1.y()))
                .arg(formatValue(p2.x()))
                .arg(formatValue(p2.y()))
                .arg(formatValue(dx))
                .arg(formatValue(dy))
                .arg(slopeText);
        }
        m_coordsLabel->setText(coordsText);
    });

    resize(1000, 650);
}


ChartWindow::ChartWindow(const QString& title,
    const std::vector<SeriesIn>& seriesList,
    QWidget* parent)
    : QMainWindow(parent)
{
    m_chart = new QChart();
    m_chart->setTitle(title);
    m_chart->legend()->setVisible(true);

    m_axisX = new QValueAxis();      m_axisX->setTitleText("Time (s)"); m_axisX->setTickCount(9);
    m_axisYLeft = new QValueAxis(); m_axisYLeft->setTitleText("Voltage (V)"); m_axisYLeft->setTickCount(9);

    bool hasCurrent = false;
    for (const auto& s : seriesList) if (s.isCurrent) { hasCurrent = true; break; }
    if (hasCurrent) { m_axisYRight = new QValueAxis(); m_axisYRight->setTitleText("Current (A)"); m_axisYRight->setTickCount(9); }

    m_axisX->setLabelFormat("%.2f");
    m_axisYLeft->setLabelFormat("%.2f");
    if (m_axisYRight) m_axisYRight->setLabelFormat("%.2f");

    m_chart->addAxis(m_axisX, Qt::AlignBottom);
    m_chart->addAxis(m_axisYLeft, Qt::AlignLeft);
    if (m_axisYRight) m_chart->addAxis(m_axisYRight, Qt::AlignRight);

    
    QList<QColor> defaultColors = {
        Qt::blue, Qt::red, Qt::green, Qt::cyan, Qt::magenta, Qt::yellow,
        Qt::darkBlue, Qt::darkRed, Qt::darkGreen, Qt::darkCyan, Qt::darkMagenta, Qt::darkYellow,
        QColor("#FFA07A"), QColor("#20B2AA"), QColor("#BA55D3"), QColor("#FF6347"),
        QColor("#4682B4"), QColor("#8FBC8F"), QColor("#D2691E"), QColor("#FFD700")
    };

    int colorIndex = 0;
    for (const auto& in : seriesList) {
        if (in.samples.empty()) {
            QMessageBox::warning(this, "Warning", QString("Series '%1' has no data points and will not be displayed.").arg(in.name));
            continue;
        }

        auto* s = new QLineSeries();
        s->setName(in.name);
#if QT_VERSION >= QT_VERSION_CHECK(5, 8, 0)
        s->setUseOpenGL(true);
#endif
        QPen pen = s->pen();
        pen.setWidthF(2.0);
        
        if (!in.colorCss.isEmpty()) {
            QColor c(in.colorCss);
            if (c.isValid()) pen.setColor(c);
        } else {
            
            pen.setColor(defaultColors.at(colorIndex % defaultColors.size()));
            colorIndex++;
        }
        s->setPen(pen);
        
        for (const auto& p : in.samples) s->append(p.first, p.second);

        m_chart->addSeries(s);
        s->attachAxis(m_axisX);
        if (in.isCurrent && m_axisYRight) {
            s->attachAxis(m_axisYRight);
            m_rightSeries.push_back(s);
        }
        else {
            s->attachAxis(m_axisYLeft);
            m_leftSeries.push_back(s);
        }
    }

    auto* view = new TraceChartView(m_chart);
    setCentralWidget(view);

    auto* tb = addToolBar("View");
    m_actAuto = tb->addAction("Auto Zoom");  m_actAuto->setCheckable(true); m_actAuto->setChecked(true);
    auto* actFit = tb->addAction("Fit");
    auto* actReset = tb->addAction("Reset");
    auto* actIn = tb->addAction("Zoom In");
    auto* actOut = tb->addAction("Zoom Out");
    tb->addSeparator();
    auto* actHXIn = tb->addAction("H+ (X In)");
    auto* actHXOut = tb->addAction("H- (X Out)");
    auto* actVYIn = tb->addAction("V+ (Y In)");
    auto* actVYOut = tb->addAction("V- (Y Out)");
    tb->addSeparator();
    auto* actMath = tb->addAction("Math"); 

    connect(actReset, &QAction::triggered, m_chart, &QChart::zoomReset);
    connect(actIn, &QAction::triggered, this, [this]() { m_chart->zoomIn(); });
    connect(actOut, &QAction::triggered, this, [this]() { m_chart->zoomOut(); });

    for (QLegendMarker* m : m_chart->legend()->markers()) {
        connect(m, &QLegendMarker::clicked, this, [this, m]() {
            auto* s = m->series();
            s->setVisible(!s->isVisible());
            m->setVisible(true);
            if (m_autoZoom) { m_chart->zoomReset(); recomputeAndApplyRanges(); }
        });
    }
    
    connect(m_actAuto, &QAction::toggled, this, [this](bool on) {
        m_autoZoom = on;
        if (m_autoZoom) { m_chart->zoomReset(); recomputeAndApplyRanges(); }
    });
    connect(actFit, &QAction::triggered, this, [this]() {
        m_chart->zoomReset(); recomputeAndApplyRanges();
    });

    connect(actHXIn, &QAction::triggered, this, [this]() { if (m_autoZoom) { m_autoZoom = false; m_actAuto->setChecked(false); } scaleX(0.8); });
    connect(actHXOut, &QAction::triggered, this, [this]() { if (m_autoZoom) { m_autoZoom = false; m_actAuto->setChecked(false); } scaleX(1.25); });
    connect(actVYIn, &QAction::triggered, this, [this]() { if (m_autoZoom) { m_autoZoom = false; m_actAuto->setChecked(false); } scaleY(0.8); });
    connect(actVYOut, &QAction::triggered, this, [this]() { if (m_autoZoom) { m_autoZoom = false; m_actAuto->setChecked(false); } scaleY(1.25); });
    
    
    connect(actMath, &QAction::triggered, this, &ChartWindow::onMathActionTriggered);


    setStatusBar(new QStatusBar(this));
    m_coordsLabel = new QLabel(this);

    QFont labelFont = m_coordsLabel->font();
    labelFont.setPointSize(18);
    m_coordsLabel->setFont(labelFont);

    statusBar()->addWidget(m_coordsLabel);

    
    connect(view, &TraceChartView::coordinatesChanged, this, [this](const QPointF& p) {
        auto formatValue = [](double val) -> QString {
            if (std::abs(val) > 100000 || (std::abs(val) < 1e-3 && val != 0.0)) {
                return QString::number(val, 'g', 3);
            }
            return QString::number(val, 'f', 2);
        };
        m_coordsLabel->setText(QString("X = %1 s, Y = %2 V").arg(formatValue(p.x())).arg(formatValue(p.y())));
    });


    connect(view, &TraceChartView::deltaValuesChanged, this, [this](const QPointF& p1, const QPointF& p2) {
        double dx = p2.x() - p1.x();
        double dy = p2.y() - p1.y();

        auto formatValue = [](double val) -> QString {
            if (std::abs(val) < 1e-3 && val != 0.0) {
                return QString::number(val, 'f', 6);
            }
            return QString::number(val, 'f', 2);
        };

        QString dxText = formatValue(dx);
        QString dyText = formatValue(dy);
        QString slopeText = (std::abs(dx) < 1e-9) ? "Inf" : formatValue(dy / dx);

        QString coordsText;
        if (m_axisYRight) {
            coordsText = QString("C1: X=%1, YL=%2 | C2: X=%3, YL=%4 | dX=%5, dY=%6, Slope=%7")
                .arg(formatValue(p1.x()))
                .arg(formatValue(p1.y()))
                .arg(formatValue(p2.x()))
                .arg(formatValue(p2.y()))
                .arg(dxText)
                .arg(dyText)
                .arg(slopeText);
        }
        else {
            coordsText = QString("C1: X = %1, Y = %2 | C2 = %3, Y = %4 | dX = %5, dY = %6, Slope = %7")
                .arg(formatValue(p1.x()))
                .arg(formatValue(p1.y()))
                .arg(formatValue(p2.x()))
                .arg(formatValue(p2.y()))
                .arg(dxText)
                .arg(dyText)
                .arg(slopeText);
        }
        m_coordsLabel->setText(coordsText);
    });

    recomputeAndApplyRanges();
    resize(1100, 700);
}


void ChartWindow::switchToManual() {
    if (m_autoZoom) {
        m_autoZoom = false;
        if (m_actAuto) m_actAuto->setChecked(false);
    }
}

void ChartWindow::scaleX(double factor)
{
    switchToManual();
    if (!m_axisX) return;
    double mn = m_axisX->min(), mx = m_axisX->max();
    double mid = 0.5 * (mn + mx), half = 0.5 * (mx - mn);
    if (half <= 0) { half = 1.0; }
    half *= factor;
    m_axisX->setRange(mid - half, mid + half);
}

void ChartWindow::scaleY(double factor)
{
    switchToManual();
    auto apply = [factor](QValueAxis* ax) {
        if (!ax) return;
        double mn = ax->min(), mx = ax->max();
        double mid = 0.5 * (mn + mx), half = 0.5 * (mx - mn);
        if (half <= 0) { half = 1.0; }
        half *= factor;
        ax->setRange(mid - half, mid + half);
    };
    apply(m_axisYLeft);
    apply(m_axisYRight);
}

void ChartWindow::recomputeAndApplyRanges()
{
    auto pad = [](double& mn, double& mx) {
        if (std::isnan(mn) || std::isnan(mx) || std::isinf(mn) || std::isinf(mx)) { mn = 0; mx = 1; return; }
        if (mn == mx) { double d = (mn == 0 ? 1.0 : std::abs(mn) * 0.1); mn -= d; mx += d; return; }
        double span = mx - mn; mn -= 0.05 * span; mx += 0.05 * span;
    };

    double xmin = std::numeric_limits<double>::infinity();
    double xmax = -std::numeric_limits<double>::infinity();

    auto minL = std::numeric_limits<double>::infinity();
    auto maxL = -std::numeric_limits<double>::infinity();
    auto minR = std::numeric_limits<double>::infinity();
    auto maxR = -std::numeric_limits<double>::infinity();

    auto sweep = [&](const std::vector<QLineSeries*>& list, double& vmin, double& vmax) {
        for (auto* s : list) if (s->isVisible()) {
            const auto pts = s->pointsVector();
            for (const auto& pt : pts) {
                if (!std::isfinite(pt.x()) || !std::isfinite(pt.y())) continue;
                xmin = std::min<double>(xmin, pt.x());
                xmax = std::max<double>(xmax, pt.x());
                vmin = std::min<double>(vmin, pt.y());
                vmax = std::max<double>(vmax, pt.y());
            }
        }
    };
    sweep(m_leftSeries, minL, maxL);
    sweep(m_rightSeries, minR, maxR);

    pad(xmin, xmax);
    pad(minL, maxL);
    if (m_axisYRight) pad(minR, maxR);

    m_axisX->setRange(xmin, xmax);
    m_axisYLeft->setRange(minL, maxL);
    if (m_axisYRight) m_axisYRight->setRange(minR, maxR);
}


void ChartWindow::onMathActionTriggered() {
    bool ok;
    
    QString formula = QInputDialog::getText(this, "Add Math Signal", "Enter Formula (e.g., V(n001) + V(n002) or V(n001)*V(n002))", QLineEdit::Normal, "", &ok);
    if (!ok || formula.isEmpty()) {
        return;
    }
    processMathFormula(formula);
}

void ChartWindow::processMathFormula(const QString& formula) {
    
    QRegularExpression formulaRegex("V\\(([^)]+)\\)\\s*([\\+\\*])\\s*V\\(([^)]+)\\)");
    QRegularExpressionMatch match = formulaRegex.match(formula);
    
    if (!match.hasMatch()) {
        QMessageBox::warning(this, "Error", "Invalid formula format. Please use 'V(sig1) + V(sig2)' or 'V(sig1)*V(sig2)'.");
        return;
    }

    QString signalName1 = "V(" + match.captured(1) + ")";
    QString op = match.captured(2);
    QString signalName2 = "V(" + match.captured(3) + ")";
    
    QLineSeries* series1 = nullptr;
    QLineSeries* series2 = nullptr;

    
    for (QAbstractSeries* s : m_chart->series()) {
        auto* ls = qobject_cast<QLineSeries*>(s);
        if (ls && ls->name() == signalName1) {
            series1 = ls;
        }
        if (ls && ls->name() == signalName2) {
            series2 = ls;
        }
    }
    
    if (!series1 || !series2) {
        QString missingSignal = series1 ? signalName2 : signalName1;
        QMessageBox::warning(this, "Error", QString("Could not find series with name: %1").arg(missingSignal));
        return;
    }
    
    const auto& pts1 = series1->pointsVector();
    const auto& pts2 = series2->pointsVector();
    
    if (pts1.size() != pts2.size() || pts1.isEmpty()) {
        QMessageBox::warning(this, "Error", "Input series have different number of data points or no data.");
        return;
    }

    
    auto* newSeries = new QLineSeries();
    newSeries->setName(formula);
    
    for (int j = 0; j < pts1.size(); ++j) {
        double x = pts1.at(j).x();
        double y1 = pts1.at(j).y();
        double y2 = pts2.at(j).y();
        double result;

        if (op == "+") {
            result = y1 + y2;
        } else if (op == "*") {
            result = y1 * y2;
        } else {
            
            QMessageBox::warning(this, "Error", "Unsupported operator.");
            delete newSeries;
            return;
        }
        newSeries->append(x, result);
    }
    
    QPen newPen = newSeries->pen();
    newPen.setWidthF(2.0);

    
    QList<QColor> defaultColors = {
        Qt::blue, Qt::red, Qt::green, Qt::cyan, Qt::magenta, Qt::yellow,
        Qt::darkBlue, Qt::darkRed, Qt::darkGreen, Qt::darkCyan, Qt::darkMagenta, Qt::darkYellow,
        QColor("#FFA07A"), QColor("#20B2AA"), QColor("#BA55D3"), QColor("#FF6347"),
        QColor("#4682B4"), QColor("#8FBC8F"), QColor("#D2691E"), QColor("#FFD700")
    };
    
    
    int seriesCount = m_chart->series().size();
    newPen.setColor(defaultColors.at(seriesCount % defaultColors.size()));

    newSeries->setPen(newPen);

    
    m_chart->addSeries(newSeries);
    newSeries->attachAxis(m_axisX);
    newSeries->attachAxis(m_axisYLeft);
    m_leftSeries.push_back(newSeries);

    
    QList<QLegendMarker*> markers = m_chart->legend()->markers();
    for (QLegendMarker* m : markers) {
        if (m->series() == newSeries) {
            connect(m, &QLegendMarker::clicked, this, [this, m]() {
                auto* s = m->series();
                s->setVisible(!s->isVisible());
                m->setVisible(true);
                if (m_autoZoom) { m_chart->zoomReset(); recomputeAndApplyRanges(); }
            });
            break;
        }
    }
    
    if (m_actAuto) {
        m_actAuto->setChecked(true);
    }
    recomputeAndApplyRanges();
}


#include "chart.moc"