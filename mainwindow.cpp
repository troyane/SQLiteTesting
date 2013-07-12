#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "sleeperthread.h"
#include "performancetimer.h"

#include <QDateTime>
#include <QDebug>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QThread>

#include <QtConcurrentRun>
#include <QFuture>

const QString tableName = QString ("test_table");
const QString createTableText = QString("CREATE TABLE IF NOT EXISTS test_table "
                                        "( id INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL UNIQUE, "
                                        "test_string CHAR( 255 ) )");
const QString connectionName = QString("main_connection");

const QString insertStartInfo = QString("INSERT INTO %1 (test_string) VALUES ('START: %2')")
        .arg(tableName)
        .arg(QDateTime::currentDateTime().toString());

const QString selectQuery = QString("SELECT * from %1").arg(tableName);

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    time.start();
    if (!prepareAndOpenDb()) {
        qDebug() << "Error while opening DB";
    }
}

bool MainWindow::prepareAndOpenDb()
{
    QStringList driverList( QSqlDatabase::drivers() );

    if (!driverList.contains(QString("QSQLITE"), Qt::CaseInsensitive)) {
        qDebug() << "No SQLITE support";
        return false;
    } else {
        qDebug() << "SQLITE support is present";
    }

    mWorkDb = QSqlDatabase::addDatabase("QSQLITE", connectionName);
    mWorkDb.setHostName("localhost");
    mWorkDb.setDatabaseName("sqlitetest");

    if (!mWorkDb.open()) {
        qDebug() << "DB was not opened!";
        return false;
    }

    QString journalMode("DELETE"); // default: "DELETE", "WAL"
    if (setDbJournalMode(mWorkDb, journalMode) )
    {
        qDebug() << "Sucessfuly switched to" << journalMode;
    }

    QStringList tables = mWorkDb.tables(QSql::Tables);

    if ( !tables.contains("test_table") ) {
        QSqlQuery createTableQuery(mWorkDb);
        if ( !createTableQuery.exec(createTableText) ) {
            qDebug() << "Can't create table" << tableName << "in" << mWorkDb.databaseName()
                     << ":" << createTableQuery.lastError().text();
            return false;
        }
    } else {
        qDebug() << mWorkDb.databaseName() << "already has table" << tableName;
    }
//    QSqlQuery q(insertStartInfo, mWorkDb);
//    q.exec("PRAGMA journal_mode");
//    qDebug() << "PRAGMA:";
//    outpuQuery(q);

    return true;
}

bool MainWindow::setDbJournalMode(QSqlDatabase &db, const QString &mode)
{
    QSqlQuery q(db);
    q.prepare( QString("PRAGMA journal_mode = %1").arg(mode) );
    return q.exec();
}

void MainWindow::readFromDb()
{
    const int threadId = QThread::currentThreadId();
    qDebug("Readers thread: %10d", threadId);

    // wait for transaction in writer begins
    while (transInProcess==false)
    {
        qDebug("\t# Reader is waiting, %10lld ns", time.elapsed());
        SleeperThread::msSleep(7);
    }
    qDebug("\t# Reader got transInProcess==true");

    QString connectionName = QString("Reader_%1").arg(threadId);
    QSqlDatabase readDb = QSqlDatabase::cloneDatabase(mWorkDb, connectionName);

    if (!readDb.open()) {
        qDebug() << "DB was not opened to read!";
        return;
    }

    qDebug("| %40s | %10lld ns |", "Before Reader query executed", time.elapsed());
    QSqlQuery q(selectQuery, readDb);
    qDebug("Reader got:");
    outpuQuery(q);
    qDebug("| %40s | %10lld ns |", "After Reader query executed", time.elapsed());
    outpuQuery(q);
}

void MainWindow::writeToDb()
{
    const int threadId = QThread::currentThreadId();
    qDebug("Writer thread: %10d", threadId);

    QString connectionName = QString ("Writer_%1").arg(threadId);
    QSqlDatabase writeDb = QSqlDatabase::cloneDatabase(mWorkDb, connectionName);

    if (!writeDb.open()) {
        qDebug() << "DB was not opened to write!";
        return;
    }

    const QString insertWriterInfo = QString("INSERT INTO %1 (test_string) VALUES ('WRITER at %2, %3')")
            .arg(tableName)
            .arg(QString::number(threadId))
            .arg(QDateTime::currentDateTime().toString());

    // insert some pause just to feel waiting process in Reader
    SleeperThread::msSleep(21);
    writeDb.transaction();
    transInProcess = true;
    QSqlQuery q(insertWriterInfo, writeDb);
    SleeperThread::msSleep(1000);
    qDebug("| %40s | %10lld ns |", "writeToDb query before commit", time.elapsed());
    writeDb.commit();
    qDebug("| %40s | %10lld ns |", "writeToDb query after commit", time.elapsed());
    transInProcess = false;
}

void MainWindow::writeToDbOther()
{
    const int threadId = QThread::currentThreadId();
    qDebug("Other writer thread: %10d", threadId);

    // wait for transaction in writeToDb() begins
    while (transInProcess==false)
    {
        qDebug("\t# Other writer is waiting, %10lld ns", time.elapsed());
        SleeperThread::msSleep(7);
    }
    qDebug("\t# Other writer got transInProcess==true");

    QString connectionName = QString ("OtherWriter_%1").arg(threadId);
    QSqlDatabase writeDb = QSqlDatabase::cloneDatabase(mWorkDb, connectionName);

    if (!writeDb.open()) {
        qDebug() << "DB was not opened to write!";
        return;
    }

    const QString insertWriterInfo = QString("INSERT INTO %1 (test_string) VALUES ('OTHER_WRITER at %2, %3')")
            .arg(tableName)
            .arg(QString::number(threadId))
            .arg(QDateTime::currentDateTime().toString());

    // insert some pause just to feel waiting process in Reader
    writeDb.transaction();
    QSqlQuery q(insertWriterInfo, writeDb);
    qDebug("| %40s | %10lld ns |", "writeToDbOther query before commit", time.elapsed());
    writeDb.commit();
    qDebug("| %40s | %10lld ns |", "writeToDbOther query after commit", time.elapsed());
}


void MainWindow::startRace()
{
    transInProcess = false;
    QtConcurrent::run(this, &MainWindow::writeToDb);
    QtConcurrent::run(this, &MainWindow::writeToDbOther);
    QtConcurrent::run(this, &MainWindow::readFromDb);

    // waiting all threads done
    if (QThreadPool::globalInstance()->activeThreadCount()!=0)
        QThreadPool::globalInstance()->waitForDone();
    qDebug() << "All threads done.\nSUMMARY: ";

    QSqlQuery q(selectQuery, mWorkDb);
    q.exec();
    outpuQuery(q);

}


MainWindow::~MainWindow()
{
    delete ui;
}


void MainWindow::outpuQuery(QSqlQuery &query)
{
    if (query.isActive()) {
        while (query.next()) {
            qDebug("%4i: %s", query.value(0).toInt(), qPrintable(query.value(1).toString())); // << query.value(0).toInt() << ":" << query.value(1).toString();
        }
    }
}
