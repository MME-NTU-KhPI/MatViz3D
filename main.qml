import "."

MainWindow {
    id: mainWindow
    visible: true

    StatisticsView {
        id: statisticsView
        visible: false
    }

    AboutView {
        id: aboutView
        visible: false
    }

    MaterialDatabaseView {
        id: materialDatabaseView
        visible: false
    }


}
