#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSqlDatabase>

class QSqlQuery;

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT
    QSqlDatabase mWorkDb;
public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

    bool prepareAndOpenDb();
    bool setDbJournalMode(QSqlDatabase& db,  const QString& mode);
    void readFromDb();
    void writeToDb();
    void startRace();
    void outpuQuery(QSqlQuery& query);
    volatile bool transInProcess;

private:
    Ui::MainWindow *ui;
};

#endif // MAINWINDOW_H
