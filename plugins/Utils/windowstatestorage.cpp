/*
 * Copyright 2015-2016 Canonical Ltd.
 * Copyright 2021 UBports Foundation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "windowstatestorage.h"

#include <QDebug>
#include <QDir>
#include <QMetaObject>
#include <QObject>
#include <QSqlQuery>
#include <QSqlError>
#include <QSqlResult>
#include <QStandardPaths>
#include <QRect>
#include <unity/shell/application/ApplicationInfoInterface.h>

inline QString sanitiseString(QString string) {
    return string.remove(QLatin1Char('\"'))
                 .remove(QLatin1Char('\''))
                 .remove(QLatin1Char('\\'));
}

AsyncQuery::AsyncQuery(const QString& dbName)
{
    QSqlDatabase connection = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connectionName);
    if (dbName == nullptr) {
        const QString dbPath = QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation) + QStringLiteral("/unity8/");
        QDir dir;
        dir.mkpath(dbPath);
        connection.setDatabaseName(dbPath + "windowstatestorage.sqlite");
    } else {
        connection.setDatabaseName(dbName);
    }

    initdb(m_connectionName);
}

AsyncQuery::~AsyncQuery()
{
    QSqlDatabase::removeDatabase(m_connectionName);
}

void AsyncQuery::initdb(const QString &connectionName)
{
    QSqlDatabase connection = QSqlDatabase::database(connectionName);
    if (!connection.isOpen()) {
        qWarning() << "Error opening state database:" << connection.lastError().driverText() << connection.lastError().databaseText();
        return;
    }

    if (!connection.tables().contains(QStringLiteral("geometry"))) {
        QSqlQuery query(connection);
        query.exec(QStringLiteral("CREATE TABLE geometry(windowId TEXT UNIQUE, x INTEGER, y INTEGER, width INTEGER, height INTEGER);"));
    }

    if (!connection.tables().contains(QStringLiteral("state"))) {
        QSqlQuery query(connection);
        query.exec(QStringLiteral("CREATE TABLE state(windowId TEXT UNIQUE, state INTEGER);"));
    }

    if (!connection.tables().contains(QStringLiteral("stage"))) {
        QSqlQuery query(connection);
        query.exec(QStringLiteral("CREATE TABLE stage(appId TEXT UNIQUE, stage INTEGER);"));
    }
}

QSqlQuery AsyncQuery::execute(const QString& queryString)
{
    QSqlDatabase connection = QSqlDatabase::database(m_connectionName);
    QSqlQuery query(connection);
    auto ok = query.exec(queryString);
    if (!ok) {
        qWarning() << "Error executing query" << queryString
                    << "Driver error:" << query.lastError().driverText()
                    << "Database error:" << query.lastError().databaseText();
    }
    return query;
}

WindowStateStorage::WindowStateStorage(const QString& dbName, QObject *parent):
    QObject(parent)
{
    m_asyncQuery = std::make_shared<AsyncQuery> (dbName);
    connect(this, &WindowStateStorage::executeAsyncQuery, m_asyncQuery.get(), &AsyncQuery::execute);
}

WindowStateStorage::~WindowStateStorage()
{
    m_thread.quit();
    m_thread.wait();
}

void WindowStateStorage::saveState(const QString &windowId, WindowStateStorage::WindowState state)
{
    const QString queryString = QStringLiteral("INSERT OR REPLACE INTO state (windowId, state) values ('%1', '%2');")
            .arg(sanitiseString(windowId))
            .arg((int)state);

    executeAsyncQuery(queryString);
}

WindowStateStorage::WindowState WindowStateStorage::getState(const QString &windowId, WindowStateStorage::WindowState defaultValue) const
{
    const QString queryString = QStringLiteral("SELECT state FROM state WHERE windowId = '%1';")
            .arg(sanitiseString(windowId));

    QSqlQuery query = getValue(queryString);

    if (!query.first()) {
        return defaultValue;
    }
    return (WindowState)query.value(QStringLiteral("state")).toInt();
}

void WindowStateStorage::saveGeometry(const QString &windowId, const QRect &rect)
{
    const QString queryString = QStringLiteral("INSERT OR REPLACE INTO geometry (windowId, x, y, width, height) values ('%1', '%2', '%3', '%4', '%5');")
            .arg(sanitiseString(windowId))
            .arg(rect.x())
            .arg(rect.y())
            .arg(rect.width())
            .arg(rect.height());

    executeAsyncQuery(queryString);
}

void WindowStateStorage::saveStage(const QString &appId, int stage)
{
    const QString queryString = QStringLiteral("INSERT OR REPLACE INTO stage (appId, stage) values ('%1', '%2');")
            .arg(sanitiseString(appId))
            .arg(stage);

    executeAsyncQuery(queryString);
}

int WindowStateStorage::getStage(const QString &appId, int defaultValue) const
{
    const QString queryString = QStringLiteral("SELECT stage FROM stage WHERE appId = '%1';")
            .arg(sanitiseString(appId));

    QSqlQuery query = getValue(queryString);

    if (!query.first()) {
        return defaultValue;
    }
    return query.value("stage").toInt();
}

QRect WindowStateStorage::getGeometry(const QString &windowId, const QRect &defaultValue) const
{
    QString queryString = QStringLiteral("SELECT * FROM geometry WHERE windowId = '%1';")
            .arg(sanitiseString(windowId));

    QSqlQuery query = getValue(queryString);

    if (!query.first()) {
        return defaultValue;
    }

    const QRect result(query.value(QStringLiteral("x")).toInt(), query.value(QStringLiteral("y")).toInt(),
                       query.value(QStringLiteral("width")).toInt(), query.value(QStringLiteral("height")).toInt());

    if (result.isValid()) {
        return result;
    }

    return defaultValue;
}

QSqlQuery WindowStateStorage::getValue(const QString &queryString) const
{
   QSqlQuery query;
   QMetaObject::invokeMethod(m_asyncQuery.get(), "execute", Qt::DirectConnection,
                             Q_RETURN_ARG(QSqlQuery, query),
                             Q_ARG(const QString&, queryString)
                             );
    return query;
}

Mir::State WindowStateStorage::toMirState(WindowState state) const
{
    // assumes a single state (not an OR of several)
    switch (state) {
        case WindowStateMaximized:             return Mir::MaximizedState;
        case WindowStateMinimized:             return Mir::MinimizedState;
        case WindowStateFullscreen:            return Mir::FullscreenState;
        case WindowStateMaximizedLeft:         return Mir::MaximizedLeftState;
        case WindowStateMaximizedRight:        return Mir::MaximizedRightState;
        case WindowStateMaximizedHorizontally: return Mir::HorizMaximizedState;
        case WindowStateMaximizedVertically:   return Mir::VertMaximizedState;
        case WindowStateMaximizedTopLeft:      return Mir::MaximizedTopLeftState;
        case WindowStateMaximizedTopRight:     return Mir::MaximizedTopRightState;
        case WindowStateMaximizedBottomLeft:   return Mir::MaximizedBottomLeftState;
        case WindowStateMaximizedBottomRight:  return Mir::MaximizedBottomRightState;

        case WindowStateNormal:
        case WindowStateRestored:
        default:
            return Mir::RestoredState;
    }
}

#include "windowstatestorage.moc"
