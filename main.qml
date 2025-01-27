import "."

MainWindow {
    id: mainWindow
    visible: true

    ProbabilityAlgorithmView {
        id: probabilityAlgorithmView
        visible: false
    }

    StatisticsView {
        id: statisticsView
        visible: false
    }

    AboutView {
        id: aboutView
        visible: false
    }
}
