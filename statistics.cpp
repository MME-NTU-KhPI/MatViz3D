#include "statistics.h"
#include "ui_statistics.h"

#include <QtCharts>
#include <QVector>
#include <algorithm>
#include <fstream>
#include <vector>
#include <QSet>
#include <cmath>
#include <vector>
#include <queue>
#include <unordered_map>
#include <unordered_set>

Statistics::Statistics(QWidget *parent)
    : QWidget(parent), ui(new Ui::Statistics)
{
    ui->setupUi(this);

    connect(ui->d2RadiaoButton, &QRadioButton::toggled, this, &Statistics::updatePropertyBox);
    connect(ui->d3RadiaoButton, &QRadioButton::toggled, this, &Statistics::updatePropertyBox);

    allObjects2D = QList<Object>();
    allObjects3D = QList<Object>();

    ui->propertyBox->setCurrentText("-----");
    selectProperty();
    connect(ui->propertyBox, QOverload<int>::of(&QComboBox::activated), [this](int index){
        Q_UNUSED(index);
        this->selectProperty();
    });
}


Statistics::~Statistics()
{
    delete ui;
}

void Statistics::setPropertyBoxText(const QString &text)
{
    ui->propertyBox->setCurrentText(text);
}

struct Point {
    int x, y;
    Point(int _x, int _y) : x(_x), y(_y) {}
};

void Statistics::updatePropertyBox() {
    ui->propertyBox->clear();

    if (ui->d2RadiaoButton->isChecked()) {
        ui->propertyBox->setCurrentText("-----");
        selectProperty();
        ui->propertyBox->addItems({"-----", "Area", "Norm Area", "Perimeter", "ECR", "Shape factor"});
    } else if (ui->d3RadiaoButton->isChecked()) {
        ui->propertyBox->setCurrentText("-----");
        selectProperty();
        ui->propertyBox->addItems({"-----", "Volume", "Norm Volume", "Surface Area", "ESR", "Inertia Moment"});
    }
}

// Функція для побудови гістограми
QChart* Statistics::createChart(const QString& selectedTitleProperty) {
    QChart *chart = new QChart();
    chart->setTitle(selectedTitleProperty);
    chart->setTitleFont(QFont("Arial", 20));
    chart->setTitleBrush(QBrush(QColor(0, 0, 0)));
    chart->setAnimationOptions(QChart::SeriesAnimations);

    chart->setBackgroundBrush(QColor(170, 170, 170));
    chart->setPlotAreaBackgroundBrush(QColor(170, 170, 170));
    chart->setPlotAreaBackgroundVisible(true);

    return chart;
}

QValueAxis* Statistics::createAxisX() {
    QValueAxis *axisX = new QValueAxis();
    axisX->setTitleText("Value");
    axisX->setTitleBrush(QBrush(QColor(0, 0, 0)));
    axisX->setTitleFont(QFont("Arial", 14));
    axisX->setLabelsColor(QColor(0, 0, 0));
    axisX->setLinePenColor(QColor(0, 0, 0));
    axisX->setGridLineColor(QColor(0, 0, 0, 90));
    return axisX;
}

QValueAxis* Statistics::createAxisY() {
    QValueAxis *axisY = new QValueAxis();
    axisY->setTitleText("Frequency");
    axisY->setLabelFormat("%.1f");
    axisY->setTitleBrush(QBrush(QColor(0, 0, 0)));
    axisY->setTitleFont(QFont("Arial", 14));
    axisY->setLinePenColor(QColor(0, 0, 0));
    axisY->setGridLineColor(QColor(0, 0, 0, 90));
    axisY->setLabelsColor(QColor(0, 0, 0));
    return axisY;
}

void Statistics::adjustAxisX(QValueAxis *axisX, const QVector<float>& counts) {
    float minValue = *std::min_element(counts.constBegin(), counts.constEnd());
    float maxValue = *std::max_element(counts.constBegin(), counts.constEnd());
    int numberOfBins = static_cast<int>(std::round(std::sqrt(counts.size()))/2);
    float binSize = (maxValue - minValue) / numberOfBins;

    float tickMinValue = minValue - binSize;
    float tickMaxValue = maxValue + binSize;

    tickMinValue = qMax(0.0f, tickMinValue);

    int tickCount = qMin(10, numberOfBins);
    axisX->setTickCount(tickCount);
    axisX->setLabelFormat("%.4g");
    axisX->setMin(tickMinValue);
    axisX->setMax(tickMaxValue);
}

QAreaSeries* Statistics::createHistogramSeries(const QVector<float>& counts) {
    QAreaSeries *series = new QAreaSeries();

    float minValue = *std::min_element(counts.constBegin(), counts.constEnd());
    float maxValue = *std::max_element(counts.constBegin(), counts.constEnd());
    int numberOfBins = static_cast<int>(std::round(std::sqrt(counts.size()))/2);
    float binSize = (maxValue - minValue) / numberOfBins;

    QVector<int> binCounts(numberOfBins, 0);

    for (float value : counts) {
        int binIndex = static_cast<int>((value - minValue) / binSize);
        if (binIndex >= 0 && binIndex < numberOfBins) {
            binCounts[binIndex]++;
        }
    }

    QVector<QPointF> lowerPoints;
    QVector<QPointF> upperPoints;
    for (int i = 0; i < numberOfBins; ++i) {
        float binStart = minValue + i * binSize;
        float binEnd = minValue + (i + 1) * binSize;
        lowerPoints.append(QPointF(binStart, 0));
        lowerPoints.append(QPointF(binEnd, 0));
        upperPoints.append(QPointF(binStart, binCounts[i]));
        upperPoints.append(QPointF(binEnd, binCounts[i]));
    }

    QLineSeries *lowerSeries = new QLineSeries();
    QLineSeries *upperSeries = new QLineSeries();
    lowerSeries->append(lowerPoints);
    upperSeries->append(upperPoints);
    series->setLowerSeries(lowerSeries);
    series->setUpperSeries(upperSeries);

    QBrush areaBrush(QColor(0, 86, 77, 150));
    QPen areaPen(QColor(0, 86, 77, 255));
    areaPen.setWidth(2);

    series->setBrush(areaBrush);
    series->setPen(areaPen);

    return series;
}


void Statistics::buildHistogram(const QVector<float>& counts, QString selectedTitleProperty)
{
    QChart *chart = createChart(selectedTitleProperty);

    QValueAxis *axisX = createAxisX();
    QValueAxis *axisY = createAxisY();

    chart->addAxis(axisX, Qt::AlignBottom);
    chart->addAxis(axisY, Qt::AlignLeft);

    if (!counts.isEmpty()) {
        QAreaSeries *series = createHistogramSeries(counts);
        chart->addSeries(series);

        QLineSeries *lowerSeries = static_cast<QLineSeries*>(series->lowerSeries());
        lowerSeries->attachAxis(axisX);
        lowerSeries->attachAxis(axisY);

        adjustAxisX(axisX, counts);

        int maxBinCount = *std::max_element(counts.constBegin(), counts.constEnd());
        int maxAxisValue = (maxBinCount + 9) / 10 * 10;
        axisY->setRange(0, maxAxisValue + 10);
    }

    chart->legend()->hide();

    clearLayout(ui->horizontalFrame->layout());

    QChartView *chartView = new QChartView(chart);
    chartView->setRenderHint(QPainter::Antialiasing);
    ui->horizontalFrame->layout()->addWidget(chartView);
}


void Statistics::clearLayout(QLayout *layout)
{
    QLayoutItem *item;
    while ((item = layout->takeAt(0)) != nullptr) {
        delete item->widget();
        delete item;
    }
}

void Statistics::selectProperty() {
    QVector<float> propertyValues;

    QString selectedProperty = ui->propertyBox->currentText();
    QString selectedTitleProperty;

    if (selectedProperty == "Norm Area") {
        selectedTitleProperty = "Distribution of normalized grain area";
        for (const auto& obj : allObjects2D) {
            propertyValues.push_back(obj.normArea);
        }
    } else if (selectedProperty == "Perimeter") {
        selectedTitleProperty = "Distribution of grain perimeter";
        for (const auto& obj : allObjects2D) {
            propertyValues.push_back(obj.perimeter);
        }
    } else if (selectedProperty == "ECR") {
        selectedTitleProperty = "Distribution of ECR";
        for (const auto& obj : allObjects2D) {
            propertyValues.push_back(obj.ecr);
        }
    } else if (selectedProperty == "Shape factor") {
        selectedTitleProperty = "Distribution of Shape factor";
        for (const auto& obj : allObjects2D) {
            propertyValues.push_back(obj.shape_factor);
        }
    } else if (selectedProperty == "Area") {
        selectedTitleProperty = "Distribution of grain area";
        for (const auto& obj : allObjects2D) {
            propertyValues.push_back(obj.size);
        }
    } else if (selectedProperty == "Volume") {
        selectedTitleProperty = "Distribution of grain volume";
        for (const auto& obj : allObjects3D) {
            propertyValues.push_back(obj.volume_3D);
        }
    } else if (selectedProperty == "Norm Volume") {
        selectedTitleProperty = "Distribution of normalized grain volume";
        for (const auto& obj : allObjects3D) {
            propertyValues.push_back(obj.norm_volume_3D);
        }
    } else if (selectedProperty == "Surface Area") {
        selectedTitleProperty = "Distribution of grain surface Area";
        for (const auto& obj : allObjects3D) {
            propertyValues.push_back(obj.surface_area_3D);
        }
    } else if (selectedProperty == "ESR") {
        selectedTitleProperty = "Distribution of ESR";
        for (const auto& obj : allObjects3D) {
            propertyValues.push_back(obj.ESR_3D);
        }
    } else if (selectedProperty == "Inertia Moment") {
        selectedTitleProperty = "Distribution of grain Inertia moment";
        for (const auto& obj : allObjects3D) {
            propertyValues.push_back(obj.moment_inertia_3D);
        }
    } else {
        selectedTitleProperty = "Choose a grain property";
        propertyValues.clear();
    }

    propertyValues.erase(std::remove_if(propertyValues.begin(), propertyValues.end(), [](float value) {
                             return std::isinf(value) || std::isnan(value) || value == 0.0f;
                         }), propertyValues.end());

    clearLayout(ui->horizontalFrame->layout());
    buildHistogram(propertyValues, selectedTitleProperty);
}


void saveToCSV(const QList<Object>& objects, const std::string& filename) {
    std::ofstream file(filename);
    if (!file.is_open()) {
        qDebug() << "Error: Unable to open file for writing";
        return;
    }

    file << "NormArea,Perimeter,Shape Factor,ECR,Volume,NormVolume,SurfaceArea,MomentInertia3D,ESR\n";

    for (const auto& obj : objects) {
        file << obj.normArea << "," << obj.perimeter << "," << obj.shape_factor << "," << obj.ecr << "," << obj.volume_3D << "," << obj.norm_volume_3D << "," << obj.surface_area_3D << "," << obj.moment_inertia_3D << "," << obj.ESR_3D << "\n";
    }

    file.close();
}


int compute_perimeter(const std::vector<std::vector<int>>& image, int i, int j) {
    int rows = image.size();
    int cols = image[0].size();
    int perimeter = 0;
    int color = image[i][j];
    bool onEdge = false;

    std::vector<Point> neighbors = {{i - 1, j}, {i + 1, j}, {i, j - 1}, {i, j + 1}};
    for (const auto& neighbor : neighbors) {
        int ni = neighbor.x;
        int nj = neighbor.y;
        if (ni < 0 || ni >= rows || nj < 0 || nj >= cols || image[ni][nj] != color) {
            perimeter++;
            onEdge = true;
        }
    }

    if (onEdge) {
        perimeter = 1;
    } else {
        perimeter = 0;
    }

    return perimeter;
}

const double pi = 3.14159;

std::vector<Object> label_connected_regions(const std::vector<std::vector<int>>& image) {
    int rows = image.size();
    int cols = image[0].size();
    double totalArea = rows * cols;

    std::vector<std::vector<bool>> visited(rows, std::vector<bool>(cols, false));
    std::unordered_map<int, std::tuple<int, int, double, double, double>> grainData;

    std::vector<Object> objects;

    for (int i = 0; i < rows; ++i) {
        for (int j = 0; j < cols; ++j) {
            if (image[i][j] && !visited[i][j]) {
                int size = 0;
                int perimeter = 0;
                int color = image[i][j];
                std::queue<Point> q;

                q.push(Point(i, j));
                visited[i][j] = true;

                while (!q.empty()) {
                    Point p = q.front();
                    q.pop();
                    size++;

                    perimeter += compute_perimeter(image, p.x, p.y);

                    std::vector<Point> neighbors = {{p.x - 1, p.y}, {p.x + 1, p.y}, {p.x, p.y - 1}, {p.x, p.y + 1}};
                    for (const auto& neighbor : neighbors) {
                        int ni = neighbor.x;
                        int nj = neighbor.y;
                        if (ni >= 0 && ni < rows && nj >= 0 && nj < cols && image[ni][nj] == color && !visited[ni][nj]) {
                            q.push(neighbor);
                            visited[ni][nj] = true;
                        }
                    }
                }

                double normArea = static_cast<double>(size) / totalArea;
                double ecr = sqrt(normArea/pi);
                double shape_factor = 4 * pi * size / (perimeter^2);

                grainData[color] = std::make_tuple(size, perimeter, normArea, shape_factor, ecr);
            }
        }
    }

    for (const auto& pair : grainData) {
        int label = pair.first;
        int size = std::get<0>(pair.second);
        int perimeter = std::get<1>(pair.second);
        double normArea = std::get<2>(pair.second);
        double shape_factor = std::get<3>(pair.second);
        double ecr = std::get<4>(pair.second);
        objects.emplace_back(label, size, perimeter, normArea, shape_factor, ecr);
    }

    return objects;
}


void Statistics::layersProcessing(int32_t*** voxels, int numCubes)
{
    allObjects2D.clear();
    allObjects3D.clear();

    int sizeX = numCubes;
    int sizeY = numCubes;
    int sizeZ = numCubes;

    for (int z = 0; z < sizeZ; ++z) {
        std::vector<std::vector<int>> layerImage(sizeY, std::vector<int>(sizeX));

        for (int y = 0; y < sizeY; ++y) {
            for (int x = 0; x < sizeX; ++x) {
                layerImage[y][x] = voxels[x][y][z];
            }
        }
        std::vector<Object> objects = label_connected_regions(layerImage);
        QList<Object> objectList(objects.begin(), objects.end());
        allObjects2D.append(objectList);
    }

    surfaceArea3D(voxels, numCubes);
    calcVolume3D(voxels, numCubes);
    calcNormVolume3D();
    calcESR();
    calcMomentInertia();

    // Збереження властивостей у CSV файл
    // std::string filename = "All_Layers_Properties_3.csv";
    // saveToCSV(allObjects, filename);
}


void Statistics::on_saveChartAsIMGButton_clicked()
{
    QRect rect = ui->horizontalFrame->geometry();
    int width = rect.width();
    int height = rect.height();

    QPixmap pixmap(width, height);
    ui->horizontalFrame->render(&pixmap);

    QString fileName = QFileDialog::getSaveFileName(this, "Save Image", "", "Image Files (*.png);;All Files (*.*)");
    if (!fileName.isEmpty()) {
        pixmap.save(fileName);
    }
}

//Functions for 3D properties

void Statistics::surfaceArea3D(int32_t ***voxels, int numCubes) {
    surface_area_3D.clear();

    std::map<int, int> local_surface_area;

    for (int z = 0; z < numCubes; ++z) {
        for (int y = 0; y < numCubes; ++y) {
            for (int x = 0; x < numCubes; ++x) {
                int current = voxels[z][y][x];
                if (current != 0) {
                    int face_count = 0;
                    face_count += (x == 0 || voxels[z][y][x-1] != current) ? 1 : 0;
                    face_count += (x == numCubes-1 || voxels[z][y][x+1] != current) ? 1 : 0;
                    face_count += (y == 0 || voxels[z][y-1][x] != current) ? 1 : 0;
                    face_count += (y == numCubes-1 || voxels[z][y+1][x] != current) ? 1 : 0;
                    face_count += (z == 0 || voxels[z-1][y][x] != current) ? 1 : 0;
                    face_count += (z == numCubes-1 || voxels[z+1][y][x] != current) ? 1 : 0;

                    local_surface_area[current] += face_count;
                }
            }
        }
    }

    for (auto &entry : local_surface_area) {
        surface_area_3D[entry.first] += entry.second;
    }

    for (const auto& pair : surface_area_3D) {
        if (pair.first >= allObjects3D.size()) {
            allObjects3D.resize(pair.first + 1);
        }
        allObjects3D[pair.first].surface_area_3D = pair.second;
    }
}

void Statistics::calcVolume3D(int32_t ***voxels, int numCubes) {
    volume_3D.clear();

    std::map<int, int> local_volume_counts;

    for (int z = 0; z < numCubes; ++z) {
        for (int y = 0; y < numCubes; ++y) {
            for (int x = 0; x < numCubes; ++x) {
                int grain_id = voxels[z][y][x];
                if (grain_id > 0) {
                    local_volume_counts[grain_id]++;
                }
            }
        }
    }

    for (const auto& pair : local_volume_counts) {
        volume_3D[pair.first] += pair.second;
    }

    for (const auto& pair : volume_3D) {
        if (pair.first >= allObjects3D.size()) {
            allObjects3D.resize(pair.first + 1);
        }
        allObjects3D[pair.first].volume_3D = pair.second;
    }
}

void Statistics::calcNormVolume3D() {
    norm_volume_3D.clear();
    double max_volume = 0;

    for (const auto& pair : volume_3D) {
        if (pair.second > max_volume) {
            max_volume = pair.second;
        }
    }

    if (max_volume > 0) {
        for (const auto& pair : volume_3D) {
            norm_volume_3D[pair.first] = pair.second / max_volume;

            if (pair.first >= allObjects3D.size()) {
                allObjects3D.resize(pair.first + 1);
            }
            allObjects3D[pair.first].norm_volume_3D = norm_volume_3D[pair.first];
        }
    }
}


void Statistics::calcESR() {
    ESR_3D.clear();

    for (const auto& pair : volume_3D) {
        double esr = std::pow((3.0 / (4.0 * M_PI)) * pair.second, 1.0 / 3.0);
        ESR_3D[pair.first] = esr;

        if (pair.first >= allObjects3D.size()) {
            allObjects3D.resize(pair.first + 1);
        }

        allObjects3D[pair.first].ESR_3D = esr;
    }
}


void Statistics::calcMomentInertia() {
    moment_inertia_3D.clear();

    for (const auto& pair : ESR_3D) {
        double moment_inertia = (2.0 / 5.0) * std::pow(pair.second, 2);
        moment_inertia_3D[pair.first] = moment_inertia;

        if (pair.first >= allObjects3D.size()) {
            allObjects3D.resize(pair.first + 1);
        }

        allObjects3D[pair.first].moment_inertia_3D = moment_inertia;
    }
}

