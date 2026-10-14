#ifndef TEST_FFTHOMOG_H
#define TEST_FFTHOMOG_H

#include <QObject>

class TestFftHomog : public QObject
{
    Q_OBJECT

private slots:
    void init();

    void testHomogeneousCellReturnsItsMaterial();
    void testCgMatchesBasicAtModerateContrast();
    void testCgConvergesOnSolidVoidLaminate();
    void testSessionAutoPicksScheme();
    void testCliOption();
};

#endif // TEST_FFTHOMOG_H
