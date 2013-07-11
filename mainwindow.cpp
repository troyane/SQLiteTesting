#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "sleeperthread.h"

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

//    QString journalMode("OFF"); // default: "DELETE", "WAL"
//    if (setDbJournalMode(mWorkDb, journalMode) )
//    {
//        qDebug() << "Sucessfuly switched to" << journalMode;
//    }

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
    QSqlQuery q(insertStartInfo, mWorkDb);
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
    qDebug() << "READER:" << threadId;

    // wait for transaction in writer begins
    while (transInProcess==false)
    {
        qDebug() << "  # Reader is waiting";
        SleeperThread::msSleep(7);
    }
    qDebug() << "# reader got transInProcess==true";
    QString connectionName = QString ("Reader_%1").arg(threadId);

    QSqlDatabase readDb = QSqlDatabase::cloneDatabase(mWorkDb, connectionName);

    if (!readDb.open()) {
        qDebug() << "DB was not opened to read!";
        return;
    }

    QSqlQuery q(selectQuery, readDb);
    q.exec();
    outpuQuery(q);
}

void MainWindow::writeToDb()
{
    const int threadId = QThread::currentThreadId();
    qDebug() << "WRITER:" << threadId;

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
    SleeperThread::msSleep(21);
    QSqlQuery q(insertWriterInfo, writeDb);
    writeDb.commit();
    transInProcess = false;
}

void MainWindow::startRace()
{
    transInProcess = false;
    QtConcurrent::run(this, &MainWindow::readFromDb);
    QtConcurrent::run(this, &MainWindow::writeToDb);

    // probably all queries will be applied
    SleeperThread::msSleep(600);
    qDebug() << "Summary:";
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
