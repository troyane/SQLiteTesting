#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSqlDatabase>

#include "performancetimer.h"

class QSqlQuery;

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

    bool prepareAndOpenDb();
    bool setDbJournalMode(QSqlDatabase& db,  const QString& mode);
    void readFromDb();
    void writeToDb();
    void writeToDbOther();
    void startRace();
    void outpuQuery(QSqlQuery& query);
    volatile bool transInProcess;

private:
    PerformanceTimer time;
    QSqlDatabase mWorkDb;

    Ui::MainWindow *ui;
};

#endif // MAINWINDOW_H
