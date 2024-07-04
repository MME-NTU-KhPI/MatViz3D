#include "probability_algorithm.h"
#include "ui_probability_algorithm.h"

Probability_Algorithm::Probability_Algorithm(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::Probability_Algorithm)
{
    ui->setupUi(this);
}

Probability_Algorithm::~Probability_Algorithm()
{
    delete ui;
}
