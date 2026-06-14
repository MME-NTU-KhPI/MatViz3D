#include "statistics.h"
#include "ui_statistics.h"
#include "grain_analyzer.h"
#include <QtCharts>
#include <QVector>
#include <QFileDialog>
#include <algorithm>
#include <cmath>

// ═══════════════════════════════════════════════════════════════════════════
// Constructor / Destructor
// ═══════════════════════════════════════════════════════════════════════════

Statistics::Statistics(QWidget *parent)
    : QWidget(parent), ui(new Ui::Statistics)
{
    ui->setupUi(this);

    connect(ui->d2RadiaoButton, &QRadioButton::toggled,
            this, &Statistics::updatePropertyBox);
    connect(ui->d3RadiaoButton, &QRadioButton::toggled,
            this, &Statistics::updatePropertyBox);

    ui->propertyBox->setCurrentText("-----");
    selectProperty();

    connect(ui->propertyBox,
            QOverload<int>::of(&QComboBox::activated),
            [this](int){ this->selectProperty(); });
}

Statistics::~Statistics()
{
    delete ui;
}

// ═══════════════════════════════════════════════════════════════════════════
// Main entry points
// ═══════════════════════════════════════════════════════════════════════════

void Statistics::analyzeAndDisplay(int32_t*** voxels, int numCubes)
{
    // 3D + 2D: both delegated to GrainAnalyzer — no compute code here
    m_grainStats   = GrainAnalyzer::analyze3D(voxels, numCubes);
    m_grainStats2D = GrainAnalyzer::analyze2D(voxels, numCubes);
}

void Statistics::layersProcessing(int32_t*** voxels, int numCubes)
{
    // Compatibility wrapper — mainwindow.cpp can keep calling this
    analyzeAndDisplay(voxels, numCubes);
}

// ═══════════════════════════════════════════════════════════════════════════
// selectProperty — reads from m_grainStats (3D) or m_grainStats2D (2D)
// ═══════════════════════════════════════════════════════════════════════════

void Statistics::selectProperty()
{
    QVector<float> values;
    QString title;
    const QString prop = ui->propertyBox->currentText();

    // ── 2D ────────────────────────────────────────────────────────────────
    if (prop == "Area") {
        title = "Distribution of grain area";
        for (const auto& o : m_grainStats2D) values.push_back(o.size);

    } else if (prop == "Norm Area") {
        title = "Distribution of normalized grain area";
        for (const auto& o : m_grainStats2D) values.push_back(o.norm_area);

    } else if (prop == "Perimeter") {
        title = "Distribution of grain perimeter";
        for (const auto& o : m_grainStats2D) values.push_back(o.perimeter);

    } else if (prop == "ECR") {
        title = "Distribution of ECR";
        for (const auto& o : m_grainStats2D) values.push_back(o.ecr);

    } else if (prop == "Shape factor") {
        title = "Distribution of Shape factor";
        for (const auto& o : m_grainStats2D) values.push_back(o.shape_factor);

        // ── 3D — from GrainAnalyzer ───────────────────────────────────────────
    } else if (prop == "Volume") {
        title = "Distribution of grain volume";
        for (const auto& [id, s] : m_grainStats)
            values.push_back(static_cast<float>(s.volume));

    } else if (prop == "Norm Volume") {
        title = "Distribution of normalized grain volume";
        for (const auto& [id, s] : m_grainStats)
            values.push_back(static_cast<float>(s.norm_volume));

    } else if (prop == "Surface Area") {
        title = "Distribution of grain surface area";
        for (const auto& [id, s] : m_grainStats)
            values.push_back(static_cast<float>(s.surface_area));

    } else if (prop == "ESR") {
        title = "Distribution of ESR";
        for (const auto& [id, s] : m_grainStats)
            values.push_back(static_cast<float>(s.esr));

    } else if (prop == "Inertia Moment") {
        title = "Distribution of grain inertia moment";
        for (const auto& [id, s] : m_grainStats)
            values.push_back(static_cast<float>(s.moment_inertia));

    } else {
        title = "Choose a grain property";
    }

    // Filter NaN / Inf / zero
    values.erase(
        std::remove_if(values.begin(), values.end(),
                       [](float v){ return std::isinf(v) || std::isnan(v) || v == 0.0f; }),
        values.end());

    clearLayout(ui->horizontalFrame->layout());
    buildHistogram(values, title);
}

// ═══════════════════════════════════════════════════════════════════════════
// UI helpers — unchanged from original
// ═══════════════════════════════════════════════════════════════════════════

void Statistics::setPropertyBoxText(const QString& text)
{
    ui->propertyBox->setCurrentText(text);
}

void Statistics::updatePropertyBox()
{
    ui->propertyBox->clear();

    if (ui->d2RadiaoButton->isChecked()) {
        ui->propertyBox->setCurrentText("-----");
        selectProperty();
        ui->propertyBox->addItems(
            {"-----", "Area", "Norm Area", "Perimeter", "ECR", "Shape factor"});
    } else if (ui->d3RadiaoButton->isChecked()) {
        ui->propertyBox->setCurrentText("-----");
        selectProperty();
        ui->propertyBox->addItems(
            {"-----", "Volume", "Norm Volume", "Surface Area",
             "ESR", "Inertia Moment"});
    }
}

QChart* Statistics::createChart(const QString& title)
{
    QChart* chart = new QChart();
    chart->setTitle(title);
    chart->setTitleFont(QFont("Arial", 20));
    chart->setTitleBrush(QBrush(QColor(0, 0, 0)));
    chart->setAnimationOptions(QChart::SeriesAnimations);
    chart->setBackgroundBrush(QColor(170, 170, 170));
    chart->setPlotAreaBackgroundBrush(QColor(170, 170, 170));
    chart->setPlotAreaBackgroundVisible(true);
    return chart;
}

QValueAxis* Statistics::createAxisX()
{
    QValueAxis* ax = new QValueAxis();
    ax->setTitleText("Value");
    ax->setTitleBrush(QBrush(Qt::black));
    ax->setTitleFont(QFont("Arial", 14));
    ax->setLabelsColor(Qt::black);
    ax->setLinePenColor(Qt::black);
    ax->setGridLineColor(QColor(0, 0, 0, 90));
    return ax;
}

QValueAxis* Statistics::createAxisY()
{
    QValueAxis* ay = new QValueAxis();
    ay->setTitleText("Frequency");
    ay->setLabelFormat("%.1f");
    ay->setTitleBrush(QBrush(Qt::black));
    ay->setTitleFont(QFont("Arial", 14));
    ay->setLinePenColor(Qt::black);
    ay->setGridLineColor(QColor(0, 0, 0, 90));
    ay->setLabelsColor(Qt::black);
    return ay;
}

void Statistics::adjustAxisX(QValueAxis* axisX,
                             const QVector<float>& counts)
{
    const float minV = *std::min_element(counts.constBegin(), counts.constEnd());
    const float maxV = *std::max_element(counts.constBegin(), counts.constEnd());
    const int   bins = static_cast<int>(std::round(std::sqrt(counts.size())) / 2);
    const float bin  = (maxV - minV) / bins;

    axisX->setTickCount(std::min(10, bins));
    axisX->setLabelFormat("%.4g");
    axisX->setMin(std::max(0.0f, minV - bin));
    axisX->setMax(maxV + bin);
}

QAreaSeries* Statistics::createHistogramSeries(const QVector<float>& counts)
{
    const float minV = *std::min_element(counts.constBegin(), counts.constEnd());
    const float maxV = *std::max_element(counts.constBegin(), counts.constEnd());
    const int   bins = static_cast<int>(std::round(std::sqrt(counts.size())) / 2);
    const float bin  = (maxV - minV) / bins;

    QVector<int> binCounts(bins, 0);
    for (float v : counts) {
        int idx = static_cast<int>((v - minV) / bin);
        if (idx >= 0 && idx < bins) binCounts[idx]++;
    }

    QVector<QPointF> lo, hi;
    for (int i = 0; i < bins; ++i) {
        const float s = minV + i * bin;
        const float e = s + bin;
        lo << QPointF(s, 0) << QPointF(e, 0);
        hi << QPointF(s, binCounts[i]) << QPointF(e, binCounts[i]);
    }

    auto* lower  = new QLineSeries(); lower->append(lo);
    auto* upper  = new QLineSeries(); upper->append(hi);
    auto* series = new QAreaSeries(upper, lower);

    series->setBrush(QBrush(QColor(0, 86, 77, 150)));
    series->setPen(QPen(QColor(0, 86, 77, 255), 2));
    return series;
}

void Statistics::buildHistogram(const QVector<float>& counts,
                                const QString& title)
{
    QChart*     chart = createChart(title);
    QValueAxis* axisX = createAxisX();
    QValueAxis* axisY = createAxisY();

    chart->addAxis(axisX, Qt::AlignBottom);
    chart->addAxis(axisY, Qt::AlignLeft);

    if (!counts.isEmpty())
    {
        auto* series = createHistogramSeries(counts);
        chart->addSeries(series);

        auto* lower = static_cast<QLineSeries*>(series->lowerSeries());
        lower->attachAxis(axisX);
        lower->attachAxis(axisY);

        adjustAxisX(axisX, counts);

        const int maxBin = *std::max_element(counts.constBegin(),
                                             counts.constEnd());
        axisY->setRange(0, (maxBin / 10 + 1) * 10 + 10);
    }

    chart->legend()->hide();

    auto* view = new QChartView(chart);
    view->setRenderHint(QPainter::Antialiasing);
    ui->horizontalFrame->layout()->addWidget(view);
}

void Statistics::clearLayout(QLayout* layout)
{
    QLayoutItem* item;
    while ((item = layout->takeAt(0)) != nullptr) {
        delete item->widget();
        delete item;
    }
}

void Statistics::on_saveChartAsIMGButton_clicked()
{
    const QRect rect = ui->horizontalFrame->geometry();
    QPixmap pixmap(rect.width(), rect.height());
    ui->horizontalFrame->render(&pixmap);

    const QString fileName = QFileDialog::getSaveFileName(
        this, "Save Image", "",
        "Image Files (*.png);;All Files (*.*)");
    if (!fileName.isEmpty())
        pixmap.save(fileName);
}
