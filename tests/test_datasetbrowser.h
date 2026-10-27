#ifndef TEST_DATASETBROWSER_H
#define TEST_DATASETBROWSER_H

#include <QObject>

class TestDatasetBrowser : public QObject
{
    Q_OBJECT
private slots:
    void testManifestFiltersAndSections();
};

#endif // TEST_DATASETBROWSER_H
