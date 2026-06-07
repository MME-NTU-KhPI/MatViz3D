#include "statistics.h"
#include "ui_statistics.h"
#include "grain_analyzer.h"

#include <QtCharts>
#include <QVector>
#include <QFileDialog>
#include <algorithm>
#include <fstream>
#include <vector>
#include <cmath>
#include <queue>
#include <unordered_map>

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
    // 3D: delegate entirely to GrainAnalyzer — no UI code there
    m_grainStats = GrainAnalyzer::analyze(voxels, numCubes);

    // 2D: layer-by-layer ECR / area / perimeter (kept here — needs Qt types)
    processLayers2D(voxels, numCubes);
}

void Statistics::layersProcessing(int32_t*** voxels, int numCubes)
{
    // Compatibility wrapper — mainwindow.cpp can keep calling this
    analyzeAndDisplay(voxels, numCubes);
}

// ═══════════════════════════════════════════════════════════════════════════
// 2D layer analysis (private)
// ═══════════════════════════════════════════════════════════════════════════

// ── helpers ───────────────────────────────────────────────────────────────

struct Point2D { int x, y; };

static int compute_perimeter(const std::vector<std::vector<int>>& img,
                             int i, int j)
{
    const int rows = img.size();
    const int cols = img[0].size();
    const int color = img[i][j];

    const std::array<Point2D, 4> nb = {{ {i-1,j},{i+1,j},{i,j-1},{i,j+1} }};
    for (const auto& p : nb)
        if (p.x < 0 || p.x >= rows || p.y < 0 || p.y >= cols
            || img[p.x][p.y] != color)
            return 1;   // on boundary → contributes 1

    return 0;
}

static std::vector<Object>
label_connected_regions(const std::vector<std::vector<int>>& image)
{
    const int rows       = image.size();
    const int cols       = image[0].size();
    const double totalArea = rows * cols;
    constexpr double pi  = 3.14159265358979;

    std::vector<std::vector<bool>> visited(rows, std::vector<bool>(cols, false));
    std::unordered_map<int,
                       std::tuple<int,int,double,double,double>> grainData;

    for (int i = 0; i < rows; ++i)
        for (int j = 0; j < cols; ++j)
        {
            if (!image[i][j] || visited[i][j]) continue;

            int size = 0, perimeter = 0;
            const int color = image[i][j];
            std::queue<Point2D> q;
            q.push({i, j});
            visited[i][j] = true;

            while (!q.empty())
            {
                auto [ci, cj] = q.front(); q.pop();
                size++;
                perimeter += compute_perimeter(image, ci, cj);

                for (const auto& nb : std::array<Point2D,4>
                     {{ {ci-1,cj},{ci+1,cj},{ci,cj-1},{ci,cj+1} }})
                {
                    if (nb.x >= 0 && nb.x < rows &&
                        nb.y >= 0 && nb.y < cols &&
                        image[nb.x][nb.y] == color && !visited[nb.x][nb.y])
                    {
                        visited[nb.x][nb.y] = true;
                        q.push(nb);
                    }
                }
            }

            const double normArea    = static_cast<double>(size) / totalArea;
            const double ecr         = std::sqrt(normArea / pi);
            const double shape_factor = (perimeter > 0)
                                            ? 4.0 * pi * size / (static_cast<double>(perimeter) * perimeter)
                                            : 0.0;
            grainData[color] = { size, perimeter, normArea, shape_factor, ecr };
        }

    std::vector<Object> objects;
    for (const auto& [label, t] : grainData)
        objects.emplace_back(label,
                             std::get<0>(t), std::get<1>(t),
                             std::get<2>(t), std::get<3>(t), std::get<4>(t));
    return objects;
}

void Statistics::processLayers2D(int32_t*** voxels, int numCubes)
{
    allObjects2D.clear();

    for (int z = 0; z < numCubes; ++z)
    {
        std::vector<std::vector<int>> layer(numCubes,
                                            std::vector<int>(numCubes));
        for (int y = 0; y < numCubes; ++y)
            for (int x = 0; x < numCubes; ++x)
                layer[y][x] = voxels[x][y][z];

        auto objects = label_connected_regions(layer);
        allObjects2D.append(QList<Object>(objects.begin(), objects.end()));
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// selectProperty — reads from m_grainStats (3D) or allObjects2D (2D)
// ═══════════════════════════════════════════════════════════════════════════

void Statistics::selectProperty()
{
    QVector<float> values;
    QString title;
    const QString prop = ui->propertyBox->currentText();

    // ── 2D ────────────────────────────────────────────────────────────────
    if (prop == "Area") {
        title = "Distribution of grain area";
        for (const auto& o : allObjects2D) values.push_back(o.size);

    } else if (prop == "Norm Area") {
        title = "Distribution of normalized grain area";
        for (const auto& o : allObjects2D) values.push_back(o.normArea);

    } else if (prop == "Perimeter") {
        title = "Distribution of grain perimeter";
        for (const auto& o : allObjects2D) values.push_back(o.perimeter);

    } else if (prop == "ECR") {
        title = "Distribution of ECR";
        for (const auto& o : allObjects2D) values.push_back(o.ecr);

    } else if (prop == "Shape factor") {
        title = "Distribution of Shape factor";
        for (const auto& o : allObjects2D) values.push_back(o.shape_factor);

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
