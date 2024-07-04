#ifndef PROBABILITY_ALGORITHM_H
#define PROBABILITY_ALGORITHM_H

#include <QWidget>
#include "parent_algorithm.h"

namespace Ui {
class Probability_Algorithm;
}

class Probability_Algorithm : public QWidget, public Parent_Algorithm
{
    Q_OBJECT

public:
    explicit Probability_Algorithm(QWidget *parent = nullptr);
    ~Probability_Algorithm();

private:
    Ui::Probability_Algorithm *ui;
};

#endif // PROBABILITY_ALGORITHM_H
